#pragma once

#include "selfdrive/ui/ui.h"
class OnroadAlerts {

public:
  OnroadAlerts() {}
  void updateState(const UIState &s);
  void clear();
  void draw();

protected:
  struct Alert {
    std::string text1;
    std::string text2;
    std::string type;
    cereal::ControlsState::AlertSize size;
    cereal::ControlsState::AlertStatus status;

    bool equal(const Alert &other) const {
      return text1 == other.text1 && text2 == other.text2 && type == other.type;
    }
  };

  OnroadAlerts::Alert getAlert(const SubMaster &sm, uint64_t started_frame);
  Alert alert = {};
};
