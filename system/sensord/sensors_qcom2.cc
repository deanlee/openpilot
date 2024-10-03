#include <sys/resource.h>

#include <cassert>
#include <chrono>
#include <thread>
#include <vector>
#include <map>
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

void interrupt_loop(std::vector<std::tuple<I2CSensor *, std::string>> sensors) {
  PubMaster pm({"gyroscope", "accelerometer"});

  int fd = gpiochip_get_ro_value_fd("sensord", GPIOCHIP_INT, GPIO_LSM_INT);
  if (fd < 0) {
    assert(0);
    return;
  }

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

    for (auto &[sensor, msg_name] : sensors) {
      MessageBuilder msg;
      if (sensor->get_event(msg, ts) && sensor->is_data_valid(ts)) {
        pm.send(msg_name.c_str(), msg);
      }
    }
  }

  close(fd);
}

void polling_loop(I2CSensor *sensor, std::string msg_name) {
  PubMaster pm({msg_name.c_str()});
  RateKeeper rk(msg_name, services.at(msg_name).frequency);
  while (!do_exit) {
    MessageBuilder msg;
    if (sensor->get_event(msg) && sensor->is_data_valid(nanos_since_boot())) {
      pm.send(msg_name.c_str(), msg);
    }
    rk.keepTime();
  }
}

int sensor_loop(I2CBus *i2c_bus_imu) {
  // Sensor init
  std::vector<std::tuple<I2CSensor *, std::string>> polling_service = {
    {new BMX055_Accel(i2c_bus_imu), "accelerometer2"},
    {new BMX055_Gyro(i2c_bus_imu), "gyroscope2"},
    {new BMX055_Magn(i2c_bus_imu), "magnetometer"},
    {new BMX055_Temp(i2c_bus_imu), "temperatureSensor2"},
    {new LSM6DS3_Temp(i2c_bus_imu), "temperatureSensor"},
    {new MMC5603NJ_Magn(i2c_bus_imu), "magnetometer"},
  };

  std::vector<std::tuple<I2CSensor *, std::string>> gpio_service = {
    {new LSM6DS3_Accel(i2c_bus_imu), "accelerometer"},
    {new LSM6DS3_Gyro(i2c_bus_imu), "gyroscope"},
  }

  // Initialize sensors
  std::vector<std::thread> threads;
  for (auto &[sensor, msg_name] : polling_service) {
    int err = sensor->init();
    if (err < 0) {
      continue;
    }
    threads.emplace_back(polling_loop, sensor, msg_name);
  }

  for (auto &[sensor, msg_name] : gpio_service) {
    sensor->init();
  }

  // increase interrupt quality by pinning interrupt and process to core 1
  setpriority(PRIO_PROCESS, 0, -18);
  util::set_core_affinity({1});

  // TODO: get the IRQ number from gpiochip
  std::string irq_path = "/proc/irq/336/smp_affinity_list";
  if (!util::file_exists(irq_path)) {
    irq_path = "/proc/irq/335/smp_affinity_list";
  }
  std::system(util::string_format("sudo su -c 'echo 1 > %s'", irq_path.c_str()).c_str());

  // thread for reading events via interrupts
  threads.emplace_back(&interrupt_loop, std::ref(gpio_service));

  // wait for all threads to finish
  for (auto &t : threads) {
    t.join();
  }

  for (auto &[sensor, msg_name] : sensors_init) {
    sensor->shutdown();
    delete sensor;
  }
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
