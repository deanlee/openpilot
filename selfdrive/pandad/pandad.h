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

class Pandad {
public:
  Pandad();
  ~Pandad();

  bool connect(const std::vector<std::string> &serials);
  void pandad_thread();
  void can_recv();
  void can_send(bool fake_send);
  void process_panda_state(bool spoofing_started);
  void peripheral_control(bool no_fan_control);

protected:
  std::optional<bool> send_panda_states(bool spoofing_started);
  void send_peripheral_state(Panda *panda);

  PubMaster pm;
  SubMaster sm;
  Params params;
  std::vector<Panda *> pandas;
  FirstOrderFilter integ_lines_filter;
  std::vector<std::string> serials;
  std::future<bool> safety_future;
  std::unique_ptr<Context> context;
  std::unique_ptr<SubSocket> send_can_sock;
};
