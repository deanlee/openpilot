#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <thread>
#include <vector>
#include <poll.h>
#include <linux/gpio.h>

#include "cereal/services.h"
#include "cereal/messaging/messaging.h"
#include "common/i2c.h"
#include "common/ratekeeper.h"
#include "common/swaglog.h"
#include "common/timing.h"
#include "common/util.h"
#include "system/sensord/sensors/bmx055_accel.h"
#include "system/sensord/sensors/bmx055_gyro.h"
#include "system/sensord/sensors/bmx055_magn.h"
#include "system/sensord/sensors/bmx055_temp.h"
#include "system/sensord/sensors/constants.h"
#include "system/sensord/sensors/lsm6ds3_accel.h"
#include "system/sensord/sensors/lsm6ds3_gyro.h"
#include "system/sensord/sensors/lsm6ds3_temp.h"
#include "system/sensord/sensors/mmc5603nj_magn.h"

#define I2C_BUS_IMU 1

ExitHandler do_exit;

struct SensorState {
  SensorState(Sensor *sensor, const char *name)
      : sensor(sensor), name(name), freq(services.at(name).frequency) {
    sensor->init();
  }

  ~SensorState() {
    sensor->shutdown();
    delete sensor;
  }

  Sensor *sensor;
  const char *name;
  int freq;
};

void interrupt_loop(PubMaster &pm, std::vector<SensorState> &sensors) {
  int fd = sensors[0].sensor->gpio_fd;
  struct pollfd fd_list[1] = {0};
  fd_list[0].fd = fd;
  fd_list[0].events = POLLIN | POLLPRI;

  while (!do_exit) {
    int err = poll(fd_list, 1, 100);
    if (err == -1) {
      if (errno == EINTR) {
        continue;
      }
      return;
    } else if (err == 0) {
      LOGE("poll timed out");
      continue;
    }

    if ((fd_list[0].revents & (POLLIN | POLLPRI)) == 0) {
      LOGE("no poll events set");
      continue;
    }

    // Read all events
    struct gpioevent_data evdata[16];
    err = read(fd, evdata, sizeof(evdata));
    if (err < 0 || err % sizeof(*evdata) != 0) {
      LOGE("error reading event data %d", err);
      continue;
    }

    int num_events = err / sizeof(*evdata);
    uint64_t offset = nanos_since_epoch() - nanos_since_boot();
    uint64_t ts = evdata[num_events - 1].timestamp - offset;

    for (auto &s : sensors) {
      MessageBuilder msg;
      if (s.sensor->get_event(msg, ts) && s.sensor->is_data_valid(ts)) {
        pm.send(s.name, msg);
      }
    }
  }
}

void polling_loop(PubMaster &pm, std::vector<SensorState> &sensors) {
  const int max_freq = 100;
  // Ensure that each sensor's frequency divides max_freq evenly for single-threaded operation
  for (const auto &s : sensors) {
    assert((max_freq % s.freq) == 0 && "Sensor frequency must evenly divide max_freq");
  }

  RateKeeper rk("sensord", max_freq);
  while (!do_exit) {
    for (auto &s : sensors) {
      // Check if it's time to poll the sensor based on its frequency
      if ((rk.frame() % (max_freq / s.freq)) == 0) {
        MessageBuilder msg;
        if (s.sensor->get_event(msg) && s.sensor->is_data_valid(nanos_since_boot())) {
          pm.send(s.name, msg);
        }
      }
    }
    rk.keepTime();
  }
}

int sensor_loop(I2CBus *i2c_bus_imu) {
  // Sensor init
  std::vector<SensorState> poll_sensors = {
    {new BMX055_Accel(i2c_bus_imu), "accelerometer2"},
    {new BMX055_Gyro(i2c_bus_imu), "gyroscope2"},
    {new BMX055_Magn(i2c_bus_imu), "magnetometer"},
    {new BMX055_Temp(i2c_bus_imu), "temperatureSensor2"},
    {new LSM6DS3_Temp(i2c_bus_imu), "temperatureSensor"},
    {new MMC5603NJ_Magn(i2c_bus_imu), "magnetometer"},
  };

  std::vector<SensorState> interrupt_sensors = {
    {new LSM6DS3_Accel(i2c_bus_imu, GPIO_LSM_INT), "accelerometer"},
    {new LSM6DS3_Gyro(i2c_bus_imu, GPIO_LSM_INT, true), "gyroscope"},
  };

  // increase interrupt quality by pinning interrupt and process to core 1
  setpriority(PRIO_PROCESS, 0, -18);
  util::set_core_affinity({1});

  // TODO: get the IRQ number from gpiochip
  std::string irq_path = "/proc/irq/336/smp_affinity_list";
  if (!util::file_exists(irq_path)) {
    irq_path = "/proc/irq/335/smp_affinity_list";
  }
  std::system(util::string_format("sudo su -c 'echo 1 > %s'", irq_path.c_str()).c_str());


  std::vector<const char*> pub_names;
  for (const auto &s : poll_sensors) pub_names.push_back(s.name);
  for (const auto &s : interrupt_sensors) pub_names.push_back(s.name);

  PubMaster pm(pub_names);
  // thread for reading events via interrupts
  std::thread thread = std::thread(&interrupt_loop, std::ref(pm), std::ref(interrupt_sensors));

  polling_loop(pm, poll_sensors);

  thread.join();
  return 0;
}

int main(int argc, char *argv[]) {
  try {
    auto i2c_bus_imu = std::make_unique<I2CBus>(I2C_BUS_IMU);
    return sensor_loop(i2c_bus_imu.get());
  } catch (std::exception &e) {
    LOGE("I2CBus init failed");
    return -1;
  }
}
