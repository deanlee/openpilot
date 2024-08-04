#include <bitset>

#include "common/swaglog.h"
#include "selfdrive/pandad/pandad.h"

PandaState::PandaState(const std::vector<Panda *> &pandas, bool spoofing_started)
    : pandas_(pandas), sm_({"controlsState"}), spoofing_started_(spoofing_started) {
  red_panda_comma_three_ = (pandas_.size() == 2) &&
                           (pandas_[0]->hw_type == cereal::PandaState::PandaType::DOS) &&
                           (pandas_[1]->hw_type == cereal::PandaState::PandaType::RED_PANDA);

  for (auto &panda : pandas_) {
    connected_serials_.insert(panda->hw_serial());
  }
}

void PandaState::processPandaStates(PubMaster *pm) {
  // Retrieve the health statuses of all pandas
  auto healths = retrieveHealthStatuses();
  if (healths.empty()) {
    LOGE("Failed to get panda health status");
    return;
  }

  // Determine if ignition is on by checking health statuses
  ignition_ = std::any_of(healths.begin(), healths.end(), [](const auto &h) {
    return h.ignition_line_pkt != 0 || h.ignition_can_pkt != 0;
  });

  // Update the safety mode and power settings for each panda
  updateSafetyModeAndPower(healths);

  // Publish the states of all pandas to the PubMaster
  publishPandaStates(pm, healths);

  // Send heartbeat to all pandas
  sm_.update(0);
  const bool engaged = sm_.allAliveAndValid({"controlsState"}) && sm_["controlsState"].getControlsState().getEnabled();
  for (const auto &panda : pandas_) {
    panda->send_heartbeat(engaged);
  }
}

bool PandaState::needReconnect() {
  // If ignition is on, no need to reconnect
  if (ignition_) {
    return false;
  }

  // Check if all pandas have healthy communication
  bool comms_healthy = std::all_of(pandas_.begin(), pandas_.end(), [](auto &p) { return p->comms_healthy(); });
  if (!comms_healthy) {
    LOGE("Reconnecting, communication to pandas not healthy");
    return false;
  }

  // Check for new pandas
  for (const auto &serial : Panda::list(true)) {
    if (!connected_serials_.count(serial)) {
      LOGW("Reconnecting to new panda: %s", serial.c_str());
      return true;
    }
  }
  return false;
}

std::vector<health_t> PandaState::retrieveHealthStatuses() {
  std::vector<health_t> healths;
  healths.reserve(pandas_.size());

  for (auto &panda : pandas_) {
    auto health_opt = panda->get_state();
    if (!health_opt) {
      return {};  // Return empty vector if any health state is missing
    }

    health_t health = *health_opt;
    if (spoofing_started_) {
      health.ignition_line_pkt = 1;
    }

    // on comma three setups with a red panda, the dos can
    // get false positive ignitions due to the harness box
    // without a harness connector, so ignore it
    if (red_panda_comma_three_ && (panda->hw_type == cereal::PandaState::PandaType::DOS)) {
      health.ignition_line_pkt = 0;
    }
    healths.emplace_back(health);
  }
  return healths;
}

void PandaState::updateSafetyModeAndPower(const std::vector<health_t> &healths) {
  for (int i = 0; i < pandas_.size(); ++i) {
    // Make sure CAN buses are live: safety_setter_thread does not work if Panda CAN are silent and there is only one other CAN node
    if (healths[i].safety_mode_pkt == (uint8_t)(cereal::CarParams::SafetyModel::SILENT)) {
      pandas_[i]->set_safety_model(cereal::CarParams::SafetyModel::NO_OUTPUT);
    }

    bool power_save_desired = !ignition_;
    if (healths[i].power_save_enabled_pkt != power_save_desired) {
      pandas_[i]->set_power_saving(power_save_desired);
    }

    // set safety mode to NO_OUTPUT when car is off. ELM327 is an alternative if we want to leverage athenad/connect
    if (!ignition_ && (healths[i].safety_mode_pkt != (uint8_t)(cereal::CarParams::SafetyModel::NO_OUTPUT))) {
      pandas_[i]->set_safety_model(cereal::CarParams::SafetyModel::NO_OUTPUT);
    }
  }
}

bool PandaState::publishPandaStates(PubMaster *pm, const std::vector<health_t> &healths) {
  MessageBuilder msg;
  auto evt = msg.initEvent();
  auto pss = evt.initPandaStates(pandas_.size());

  for (int i = 0; i < pandas_.size(); ++i) {
    auto panda = pandas_[i];
    if (!panda->comms_healthy()) {
      evt.setValid(false);
    }

    setPandaState(pss[i], panda->hw_type, healths[i]);

    auto can_states = std::array{pss[i].initCanState0(), pss[i].initCanState1(), pss[i].initCanState2()};
    for (uint32_t j = 0; j < can_states.size(); j++) {
      auto can_health_opt = panda->get_can_state(j);
      if (!can_health_opt) {
        return false;
      }
      setCanState(can_states[j], *can_health_opt);
    }

    // Convert faults bitset to capnp list
    std::bitset<sizeof(healths[i].faults_pkt) * 8> fault_bits(healths[i].faults_pkt);
    auto faults = pss[i].initFaults(fault_bits.count());

    size_t fault_index = 0;
    for (size_t f = size_t(cereal::PandaState::FaultType::RELAY_MALFUNCTION);
         f <= size_t(cereal::PandaState::FaultType::HEARTBEAT_LOOP_WATCHDOG); f++) {
      if (fault_bits.test(f)) {
        faults.set(fault_index++, cereal::PandaState::FaultType(f));
      }
    }
  }

  pm->send("pandaStates", msg);
  return true;
}

void PandaState::setPandaState(cereal::PandaState::Builder ps, cereal::PandaState::PandaType hw_type, const health_t &health) {
  ps.setVoltage(health.voltage_pkt);
  ps.setCurrent(health.current_pkt);
  ps.setUptime(health.uptime_pkt);
  ps.setSafetyTxBlocked(health.safety_tx_blocked_pkt);
  ps.setSafetyRxInvalid(health.safety_rx_invalid_pkt);
  ps.setIgnitionLine(health.ignition_line_pkt);
  ps.setIgnitionCan(health.ignition_can_pkt);
  ps.setControlsAllowed(health.controls_allowed_pkt);
  ps.setTxBufferOverflow(health.tx_buffer_overflow_pkt);
  ps.setRxBufferOverflow(health.rx_buffer_overflow_pkt);
  ps.setPandaType(hw_type);
  ps.setSafetyModel(cereal::CarParams::SafetyModel(health.safety_mode_pkt));
  ps.setSafetyParam(health.safety_param_pkt);
  ps.setFaultStatus(cereal::PandaState::FaultStatus(health.fault_status_pkt));
  ps.setPowerSaveEnabled((bool)(health.power_save_enabled_pkt));
  ps.setHeartbeatLost((bool)(health.heartbeat_lost_pkt));
  ps.setAlternativeExperience(health.alternative_experience_pkt);
  ps.setHarnessStatus(cereal::PandaState::HarnessStatus(health.car_harness_status_pkt));
  ps.setInterruptLoad(health.interrupt_load_pkt);
  ps.setFanPower(health.fan_power);
  ps.setFanStallCount(health.fan_stall_count);
  ps.setSafetyRxChecksInvalid((bool)(health.safety_rx_checks_invalid_pkt));
  ps.setSpiChecksumErrorCount(health.spi_checksum_error_count_pkt);
  ps.setSbu1Voltage(health.sbu1_voltage_mV / 1000.0f);
  ps.setSbu2Voltage(health.sbu2_voltage_mV / 1000.0f);
}

void PandaState::setCanState(cereal::PandaState::PandaCanState::Builder &cs, const can_health_t &can_health) {
  cs.setBusOff((bool)can_health.bus_off);
  cs.setBusOffCnt(can_health.bus_off_cnt);
  cs.setErrorWarning((bool)can_health.error_warning);
  cs.setErrorPassive((bool)can_health.error_passive);
  cs.setLastError(cereal::PandaState::PandaCanState::LecErrorCode(can_health.last_error));
  cs.setLastStoredError(cereal::PandaState::PandaCanState::LecErrorCode(can_health.last_stored_error));
  cs.setLastDataError(cereal::PandaState::PandaCanState::LecErrorCode(can_health.last_data_error));
  cs.setLastDataStoredError(cereal::PandaState::PandaCanState::LecErrorCode(can_health.last_data_stored_error));
  cs.setReceiveErrorCnt(can_health.receive_error_cnt);
  cs.setTransmitErrorCnt(can_health.transmit_error_cnt);
  cs.setTotalErrorCnt(can_health.total_error_cnt);
  cs.setTotalTxLostCnt(can_health.total_tx_lost_cnt);
  cs.setTotalRxLostCnt(can_health.total_rx_lost_cnt);
  cs.setTotalTxCnt(can_health.total_tx_cnt);
  cs.setTotalRxCnt(can_health.total_rx_cnt);
  cs.setTotalFwdCnt(can_health.total_fwd_cnt);
  cs.setCanSpeed(can_health.can_speed);
  cs.setCanDataSpeed(can_health.can_data_speed);
  cs.setCanfdEnabled(can_health.canfd_enabled);
  cs.setBrsEnabled(can_health.brs_enabled);
  cs.setCanfdNonIso(can_health.canfd_non_iso);
  cs.setIrq0CallRate(can_health.irq0_call_rate);
  cs.setIrq1CallRate(can_health.irq1_call_rate);
  cs.setIrq2CallRate(can_health.irq2_call_rate);
  cs.setCanCoreResetCnt(can_health.can_core_reset_cnt);
}
