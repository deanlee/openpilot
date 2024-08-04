#include "selfdrive/pandad/pandad.h"

PandaState::PandaState(const std::vector<Panda *> &pandas)
    : sm_({"controlsState"}), pandas_(pandas) {
}
