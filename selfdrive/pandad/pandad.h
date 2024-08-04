#pragma once

#include <set>
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
  void processPandaStates(PubMaster *pm);
  bool needReconnect();

private:
  std::vector<health_t> retrieveHealthStatuses();
  bool publishPandaStates(PubMaster *pm, const std::vector<health_t> &healths);
  void updateSafetyModeAndPower(const std::vector<health_t> &healths);
  void setPandaState(cereal::PandaState::Builder &ps, cereal::PandaState::PandaType hw_type, const health_t &health);
  void setCanState(cereal::PandaState::PandaCanState::Builder &cs, const can_health_t &can_health);

  SubMaster sm_;
  std::vector<Panda *> pandas_;
  bool spoofing_started_ = false;
  bool red_panda_comma_three_ = false;
  std::set<std::string> connected_serials_;
  bool ignition_ = false;
};
