// ============================================================================
//  ReservoirController.cpp  -  Decisao e envio do no do reservatorio.
// ============================================================================

#include "core/ReservoirController.h"

#include <esp_log.h>

#include "config/NodeConfig.h"

namespace core {

namespace {
constexpr char TAG[] = "RES";
}

void ReservoirController::begin() {
    sensor_.begin();
    battery_.begin();
    lastLevel_ = hw::LevelState::kUnknown;
    desired_ = protocol::PumpCommand::kOff;
}

void ReservoirController::evaluateAndSend() {
    const hw::LevelState level = sensor_.read();

    if (level != lastLevel_) {
        levelChanged_.emit({lastLevel_, level});
        lastLevel_ = level;
    }

    desired_ = decide(level);

    // Envio confiavel (com ACK/retry). Serve tanto para a mudanca imediata
    // quanto para o reenvio periodico de robustez.
    const bool ok = link_.sendReliable(
        protocol::makeCommand(cfg::kAddrReservoir, pumpNodeAddress_, 0, desired_));
    ESP_LOGI(TAG, "nivel=%d comando=%d ack=%s", static_cast<int>(level),
             static_cast<int>(desired_), ok ? "sim" : "nao");
}

protocol::PumpCommand ReservoirController::decide(hw::LevelState level) const {
    switch (level) {
        case hw::LevelState::kLow:  return protocol::PumpCommand::kOn;
        case hw::LevelState::kHigh: return protocol::PumpCommand::kOff;
        case hw::LevelState::kMid:  return desired_;  // mantem (histerese)
        case hw::LevelState::kUnknown:
        default:                    return protocol::PumpCommand::kOff;  // seguro
    }
}

void ReservoirController::sendHeartbeat() {
    link_.sendOnce(
        protocol::makeHeartbeat(cfg::kAddrReservoir, pumpNodeAddress_, 0));
}

void ReservoirController::reportBattery() {
    const hw::BatteryTelemetry t = battery_.read();
    link_.sendOnce(
        protocol::makeBatteryStatus(cfg::kAddrReservoir, pumpNodeAddress_, 0, t));
    ESP_LOGI(TAG, "bateria: %u mV, SoC %u%%, fonte=%d",
             static_cast<unsigned>(t.milliVolts),
             static_cast<unsigned>(t.socPercent), static_cast<int>(t.source));
}

}  // namespace core
