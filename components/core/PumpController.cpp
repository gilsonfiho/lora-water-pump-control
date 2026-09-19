// ============================================================================
//  PumpController.cpp  -  Logica do no da bomba, com failsafe por perda de link.
// ============================================================================

#include "core/PumpController.h"

#include "platform/Clock.h"

#include "config/NodeConfig.h"

namespace core {

void PumpController::begin() {
    pump_.begin();  // garante DESLIGADA
    link_.onReceive([this](const protocol::Packet& pkt) { onPacket(pkt); });
    lastValidRxMs_ = platform::millis();
    enter(State::kIdle);
}

void PumpController::loop() {
    link_.poll();

    // Failsafe: silencio prolongado -> desliga por seguranca.
    const bool linkStale =
        (platform::millis() - lastValidRxMs_) > cfg::Timing::kLinkLostTimeoutMs;
    if (linkStale && state_ != State::kLinkLost) {
        enter(State::kLinkLost);
    }
}

void PumpController::onPacket(const protocol::Packet& pkt) {
    // Qualquer pacote valido do reservatorio conta como "enlace vivo".
    lastValidRxMs_ = platform::millis();

    if (pkt.type == protocol::MessageType::kComandoBomba) {
        protocol::PumpCommand cmd;
        if (protocol::parseCommand(pkt, cmd)) applyCommand(cmd);
        return;
    }

    // Heartbeat apenas revalida o enlace; se estava perdido, volta ao estado
    // coerente com a bomba atual.
    if (pkt.type == protocol::MessageType::kHeartbeat &&
        state_ == State::kLinkLost) {
        enter(pump_.isOn() ? State::kPumping : State::kIdle);
    }
}

void PumpController::applyCommand(protocol::PumpCommand cmd) {
    if (cmd == protocol::PumpCommand::kOn) {
        enter(State::kPumping);
    } else {
        enter(State::kIdle);
    }
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
