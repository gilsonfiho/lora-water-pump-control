#pragma once
// ============================================================================
//  PumpController.h  -  Maquina de estados do no da bomba (receptor/atuador).
// ----------------------------------------------------------------------------
//  Estados: kBoot -> kIdle/kPumping (comando) -> kLinkLost (failsafe).
//  Seguranca central: sem trafego valido por kLinkLostTimeoutMs -> DESLIGA.
//  Dirigido pela tarefa de controle: handlePacket() a cada pacote recebido e
//  checkFailsafe() periodicamente (no timeout da fila do LinkLayer).
// ============================================================================

#include <cstdint>

#include "hw/IPumpActuator.h"
#include "protocol/Messages.h"
#include "protocol/Packet.h"

namespace core {

class PumpController {
public:
    enum class State : uint8_t { kBoot, kIdle, kPumping, kLinkLost };

    explicit PumpController(hw::IPumpActuator& pump) : pump_(pump) {}

    void begin();
    void handlePacket(const protocol::Packet& pkt);
    void checkFailsafe();

    State state() const { return state_; }

private:
    void enter(State next);
    void applyCommand(protocol::PumpCommand cmd);

    hw::IPumpActuator& pump_;
    State    state_ = State::kBoot;
    uint32_t lastValidRxMs_ = 0;
};

}  // namespace core
