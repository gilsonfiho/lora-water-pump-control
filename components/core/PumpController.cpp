// ============================================================================
//  PumpController.cpp  -  Logica do no da bomba, com failsafe por perda de link.
// ============================================================================

#include "core/PumpController.h"

#include <esp_log.h>

#include "config/NodeConfig.h"
#include "platform/Clock.h"

namespace core {

namespace {
constexpr char TAG[] = "PUMP";
}

void PumpController::begin() {
    pump_.begin();  // garante DESLIGADA
    lastValidRxMs_ = platform::millis();
    enter(State::kIdle);
}

void PumpController::handlePacket(const protocol::Packet& pkt) {
    // Qualquer pacote valido do reservatorio conta como "enlace vivo".
    lastValidRxMs_ = platform::millis();

    if (pkt.type == protocol::MessageType::kComandoBomba) {
        protocol::PumpCommand cmd;
        if (protocol::parseCommand(pkt, cmd)) applyCommand(cmd);
        return;
    }

    // Heartbeat revalida o enlace; se estava perdido, volta ao estado coerente.
    if (pkt.type == protocol::MessageType::kHeartbeat &&
        state_ == State::kLinkLost) {
        enter(pump_.isOn() ? State::kPumping : State::kIdle);
    }
}

void PumpController::checkFailsafe() {
    if (state_ == State::kLinkLost) return;
    if (platform::millis() - lastValidRxMs_ > cfg::Timing::kLinkLostTimeoutMs) {
        ESP_LOGW(TAG, "enlace perdido -> failsafe (bomba OFF)");
        enter(State::kLinkLost);
    }
}

void PumpController::applyCommand(protocol::PumpCommand cmd) {
    enter(cmd == protocol::PumpCommand::kOn ? State::kPumping : State::kIdle);
}

void PumpController::enter(State next) {
    if (state_ == next) return;
    state_ = next;

    switch (next) {
        case State::kPumping:
            pump_.turnOn();
            break;
        case State::kIdle:
        case State::kLinkLost:
        case State::kBoot:
            pump_.turnOff();  // todo estado nao-bombeante desenergiza
            break;
    }
}

}  // namespace core
