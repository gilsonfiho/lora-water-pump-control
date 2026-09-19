// ============================================================================
//  ReservoirController.cpp  -  Logica de decisao e envio do no do reservatorio.
// ============================================================================

#include "core/ReservoirController.h"

#include "platform/Clock.h"

#include "config/NodeConfig.h"

namespace core {

void ReservoirController::begin() {
    sensor_.begin();
    battery_.begin();
    lastLevel_ = hw::LevelState::kUnknown;
    desired_ = protocol::PumpCommand::kOff;
}

void ReservoirController::loop() {
    link_.poll();

    const uint32_t now = platform::millis();

    if (now - lastCycleMs_ >= cfg::Timing::kReservoirCyclePeriodMs) {
        lastCycleMs_ = now;
        evaluateLevel();
        maybeSendCommand(/*forceResend=*/true);  // reenvio periodico de robustez
    }

    maybeSendHeartbeat();
    maybeReportBattery();
}

void ReservoirController::evaluateLevel() {
    const hw::LevelState level = sensor_.read();

    if (level != lastLevel_) {
        levelChanged_.emit({lastLevel_, level});
        lastLevel_ = level;
    }

    const protocol::PumpCommand next = decide(level);
    if (next != desired_) {
        desired_ = next;
        maybeSendCommand(/*forceResend=*/false);  // envia ja na mudanca
    }
}

protocol::PumpCommand ReservoirController::decide(hw::LevelState level) const {
    switch (level) {
        case hw::LevelState::kLow:  return protocol::PumpCommand::kOn;
        case hw::LevelState::kHigh: return protocol::PumpCommand::kOff;
        case hw::LevelState::kMid:  return desired_;  // mantem (histerese)
        case hw::LevelState::kUnknown:
        default:                     return protocol::PumpCommand::kOff;  // seguro
    }
}

void ReservoirController::maybeSendCommand(bool /*forceResend*/) {
    // Nao empilha transacoes: se ja ha um envio confiavel em voo, espera o
    // proximo ciclo. Vale tanto para a mudanca imediata quanto para o reenvio
    // periodico de robustez -- ambos apenas garantem que o comando desejado
    // chegue ao no da bomba com ACK.
    if (link_.isDelivering()) return;
    link_.sendReliable(
        protocol::makeCommand(cfg::kAddrReservoir, pumpNodeAddress_, 0, desired_));
}

void ReservoirController::maybeSendHeartbeat() {
    const uint32_t now = platform::millis();
    if (now - lastHeartbeatMs_ < cfg::Timing::kHeartbeatPeriodMs) return;
    lastHeartbeatMs_ = now;
    link_.sendOnce(
        protocol::makeHeartbeat(cfg::kAddrReservoir, pumpNodeAddress_, 0));
}

void ReservoirController::maybeReportBattery() {
    const uint32_t now = platform::millis();
    if (now - lastBatteryMs_ < cfg::Timing::kBatteryReportPeriodMs) return;
    lastBatteryMs_ = now;
    const hw::BatteryTelemetry t = battery_.read();
    link_.sendOnce(
        protocol::makeBatteryStatus(cfg::kAddrReservoir, pumpNodeAddress_, 0, t));
}

}  // namespace core
