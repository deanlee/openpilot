#pragma once

#include <memory>
#include <future>
#include <string>
#include <vector>
#include <optional>

#include "cereal/messaging/messaging.h"
#include "common/params.h"
#include "common/util.h"
#include "selfdrive/pandad/panda.h"

bool safety_setter_thread(std::vector<Panda *> pandas);
void pandad_main_thread(std::vector<std::string> serials);

class Pandad {
public:
  Pandad();

  PubMaster pm;
  SubMaster sm;
  Params params;
  std::vector<Panda *> pandas;
  void pandad_thread(std::vector<Panda *> p);
  void can_recv();
  void can_send(bool fake_send);
  void panda_state(bool spoofing_started);
  void peripheral_control(Panda *panda, bool no_fan_control);

  void checkConnections();
  FirstOrderFilter integ_lines_filter;
  // data:
  std::vector<can_frame> raw_can_data;
  std::vector<std::string> connected_serials;
  std::optional<bool> send_panda_states(bool spoofing_started);
  void send_peripheral_state(Panda *panda);
  std::future<bool> safety_future;
  std::unique_ptr<Context> context;
  std::unique_ptr<SubSocket> send_can_sock;
};