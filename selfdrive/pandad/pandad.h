#pragma once

#include <string>
#include <vector>

#include "cereal/messaging/messaging.h"
#include "common/params.h"
#include "selfdrive/pandad/panda.h"

void pandad_main_thread(std::vector<std::string> serials);

class PandaSafety {
public:
  PandaSafety(const std::vector<Panda *> &pandas) : pandas_(pandas) {}
  void configureSafetyMode();

private:
  void updateMultiplexingMode();
  std::string fetchCarParams();
  void setSafetyMode(const std::string &params_string);

  bool initialized_ = false;
  bool safety_configured_ = false;
  bool prev_obd_multiplexing_ = false;
  std::vector<Panda *> pandas_;
  Params params_;
};

class PandaState {
public:
  PandaState(const std::vector<Panda *> &pandas, bool spoofing_started);
  std::vector<health_t> get_healths();
  bool send_panda_states(PubMaster *pm, const std::vector<health_t> &healths);
  void process_panda_state(PubMaster *pm);

  bool needAbort();

private:
  SubMaster sm_;
  std::vector<Panda *> pandas_;
  bool spoofing_started_ = false;
  bool red_panda_comma_three_ = false;
  std::vector<std::string> connected_serials_;
  bool ignition_ = false;
};
