#include "selfdrive/pandad/pandad.h"

#include <algorithm>
#include <array>
#include <bitset>
#include <cassert>
#include <cerrno>
#include <memory>
#include <thread>
#include <utility>

#include "cereal/gen/cpp/car.capnp.h"
#include "cereal/messaging/messaging.h"
#include "common/ratekeeper.h"
#include "common/swaglog.h"
#include "common/timing.h"
#include "common/util.h"
#include "system/hardware/hw.h"

// -- Multi-panda conventions --
// Ordering:
// - The internal panda will always be the first panda
// - Consecutive pandas will be sorted based on panda type, and then serial number
// Connecting:
// - If a panda connection is dropped, pandad will reconnect to all pandas
// - If a panda is added, we will only reconnect when we are offroad
// CAN buses:
// - Each panda will have it's block of 4 buses. E.g.: the second panda will use
//   bus numbers 4, 5, 6 and 7
// - The internal panda will always be used for accessing the OBD2 port,
//   and thus firmware queries
// Safety:
// - SafetyConfig is a list, which is mapped to the connected pandas
// - If there are more pandas connected than there are SafetyConfigs,
//   the excess pandas will remain in "silent" or "noOutput" mode
// Ignition:
// - If any of the ignition sources in any panda is high, ignition is high

#define MAX_IR_POWER 0.5f
#define MIN_IR_POWER 0.0f
#define CUTOFF_IL 400
#define SATURATE_IL 1000

ExitHandler do_exit;

bool check_all_connected(const std::vector<Panda *> &pandas) {
  for (const auto& panda : pandas) {
    if (!panda->connected()) {
      do_exit = true;
      return false;
    }
  }
  return true;
}

Panda *connect(std::string serial="", uint32_t index=0) {
  std::unique_ptr<Panda> panda;
  try {
    panda = std::make_unique<Panda>(serial, (index * PANDA_BUS_OFFSET));
  } catch (std::exception &e) {
    return nullptr;
  }

  // common panda config
  if (getenv("BOARDD_LOOPBACK")) {
    panda->set_loopback(true);
  }
  //panda->enable_deepsleep();

  if (!panda->up_to_date() && !getenv("BOARDD_SKIP_FW_CHECK")) {
    throw std::runtime_error("Panda firmware out of date. Run pandad.py to update.");
  }

  return panda.release();
}

void can_send_thread(std::vector<Panda *> pandas, bool fake_send) {
  util::set_thread_name("pandad_can_send");

  AlignedBuffer aligned_buf;
  std::unique_ptr<Context> context(Context::create());
  std::unique_ptr<SubSocket> subscriber(SubSocket::create(context.get(), "sendcan"));
  assert(subscriber != NULL);
  subscriber->setTimeout(100);

  // run as fast as messages come in
  while (!do_exit && check_all_connected(pandas)) {
    std::unique_ptr<Message> msg(subscriber->receive());
    if (!msg) {
      if (errno == EINTR) {
        do_exit = true;
      }
      continue;
    }

    capnp::FlatArrayMessageReader cmsg(aligned_buf.align(msg.get()));
    cereal::Event::Reader event = cmsg.getRoot<cereal::Event>();

    // Don't send if older than 1 second
    if ((nanos_since_boot() - event.getLogMonoTime() < 1e9) && !fake_send) {
      for (const auto& panda : pandas) {
        LOGT("sending sendcan to panda: %s", (panda->hw_serial()).c_str());
        panda->can_send(event.getSendcan());
        LOGT("sendcan sent to panda: %s", (panda->hw_serial()).c_str());
      }
    } else {
      LOGE("sendcan too old to send: %" PRIu64 ", %" PRIu64, nanos_since_boot(), event.getLogMonoTime());
    }
  }
}

void can_recv(std::vector<Panda *> &pandas, PubMaster *pm) {
  static std::vector<can_frame> raw_can_data;
  {
    bool comms_healthy = true;
    raw_can_data.clear();
    for (const auto& panda : pandas) {
      comms_healthy &= panda->can_receive(raw_can_data);
    }

    MessageBuilder msg;
    auto evt = msg.initEvent();
    evt.setValid(comms_healthy);
    auto canData = evt.initCan(raw_can_data.size());
    for (size_t i = 0; i < raw_can_data.size(); ++i) {
      canData[i].setAddress(raw_can_data[i].address);
      canData[i].setDat(kj::arrayPtr((uint8_t*)raw_can_data[i].dat.data(), raw_can_data[i].dat.size()));
      canData[i].setSrc(raw_can_data[i].src);
    }
    pm->send("can", msg);
  }
}

void send_peripheral_state(Panda *panda, PubMaster *pm) {
  // build msg
  MessageBuilder msg;
  auto evt = msg.initEvent();
  evt.setValid(panda->comms_healthy());

  auto ps = evt.initPeripheralState();
  ps.setPandaType(panda->hw_type);

  double read_time = millis_since_boot();
  ps.setVoltage(Hardware::get_voltage());
  ps.setCurrent(Hardware::get_current());
  read_time = millis_since_boot() - read_time;
  if (read_time > 50) {
    LOGW("reading hwmon took %lfms", read_time);
  }

  uint16_t fan_speed_rpm = panda->get_fan_speed();
  ps.setFanSpeedRpm(fan_speed_rpm);

  pm->send("peripheralState", msg);
}

void process_peripheral_state(Panda *panda, PubMaster *pm, bool no_fan_control) {
  static SubMaster sm({"deviceState", "driverCameraState"});

  static uint64_t last_driver_camera_t = 0;
  static uint16_t prev_fan_speed = 999;
  static uint16_t ir_pwr = 0;
  static uint16_t prev_ir_pwr = 999;

  static FirstOrderFilter integ_lines_filter(0, 30.0, 0.05);

  {
    sm.update(0);
    if (sm.updated("deviceState") && !no_fan_control) {
      // Fan speed
      uint16_t fan_speed = sm["deviceState"].getDeviceState().getFanSpeedPercentDesired();
      if (fan_speed != prev_fan_speed || sm.frame % 100 == 0) {
        panda->set_fan_speed(fan_speed);
        prev_fan_speed = fan_speed;
      }
    }

    if (sm.updated("driverCameraState")) {
      auto event = sm["driverCameraState"];
      int cur_integ_lines = event.getDriverCameraState().getIntegLines();

      cur_integ_lines = integ_lines_filter.update(cur_integ_lines);
      last_driver_camera_t = event.getLogMonoTime();

      if (cur_integ_lines <= CUTOFF_IL) {
        ir_pwr = 100.0 * MIN_IR_POWER;
      } else if (cur_integ_lines > SATURATE_IL) {
        ir_pwr = 100.0 * MAX_IR_POWER;
      } else {
        ir_pwr = 100.0 * (MIN_IR_POWER + ((cur_integ_lines - CUTOFF_IL) * (MAX_IR_POWER - MIN_IR_POWER) / (SATURATE_IL - CUTOFF_IL)));
      }
    }

    // Disable IR on input timeout
    if (nanos_since_boot() - last_driver_camera_t > 1e9) {
      ir_pwr = 0;
    }

    if (ir_pwr != prev_ir_pwr || sm.frame % 100 == 0 || ir_pwr >= 50.0) {
      panda->set_ir_pwr(ir_pwr);
      prev_ir_pwr = ir_pwr;
    }
  }
}

void pandad_run(std::vector<Panda *> &pandas) {
  const bool no_fan_control = getenv("NO_FAN_CONTROL") != nullptr;
  const bool spoofing_started = getenv("STARTED") != nullptr;
  const bool fake_send = getenv("FAKESEND") != nullptr;

  // Start the CAN send thread
  std::thread send_thread(can_send_thread, pandas, fake_send);

  RateKeeper rk("pandad", 100);
  PubMaster pm({"can", "pandaStates", "peripheralState"});
  PandaSafety panda_safety(pandas);
  PandaState panda_state(pandas, spoofing_started);
  Panda *peripheral_panda = pandas[0];

  // Main loop: receive CAN data and process states
  while (!do_exit && check_all_connected(pandas)) {
    can_recv(pandas, &pm);

    // Process peripheral state at 20 Hz
    if (rk.frame() % 5 == 0) {
      process_peripheral_state(peripheral_panda, &pm, no_fan_control);
    }

    // Process panda state at 10 Hz
    if (rk.frame() % 10 == 0) {
      panda_state.process_panda_state(&pm);
      panda_safety.configureSafetyMode();
    }

    // Send out peripheralState at 2Hz
    if (rk.frame() % 50 == 0) {
      send_peripheral_state(peripheral_panda, &pm);
    }

    rk.keepTime();
  }

  send_thread.join();
}

void pandad_main_thread(std::vector<std::string> serials) {
  if (serials.size() == 0) {
    serials = Panda::list();

    if (serials.size() == 0) {
      LOGW("no pandas found, exiting");
      return;
    }
  }

  std::string serials_str;
  for (int i = 0; i < serials.size(); i++) {
    serials_str += serials[i];
    if (i < serials.size() - 1) serials_str += ", ";
  }
  LOGW("connecting to pandas: %s", serials_str.c_str());

  // connect to all provided serials
  std::vector<Panda *> pandas;
  for (int i = 0; i < serials.size() && !do_exit; /**/) {
    Panda *p = connect(serials[i], i);
    if (!p) {
      util::sleep_for(100);
      continue;
    }

    pandas.push_back(p);
    ++i;
  }

  if (!do_exit) {
    LOGW("connected to all pandas");
    pandad_run(pandas);
  }

  for (Panda *panda : pandas) {
    delete panda;
  }
}
