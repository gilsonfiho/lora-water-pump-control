#pragma once
// ============================================================================
//  PumpController.h  -  Maquina de estados do no da bomba (receptor/atuador).
// ----------------------------------------------------------------------------
//  Estados:
//    kBoot     : inicializando; bomba DESLIGADA.
//    kIdle     : enlace ok, comando atual = desligar.
//    kPumping  : enlace ok, comando atual = ligar.
//    kLinkLost : sem trafego valido dentro do timeout -> failsafe DESLIGA.
//
//  Regra de seguranca central: qualquer ausencia de comando/heartbeat por mais
//  de kLinkLostTimeoutMs desliga a bomba. So sai de kLinkLost ao receber
//  trafego valido novamente.
// ============================================================================

#include <cstdint>

#include "hal/IPumpActuator.h"
#include "protocol/LinkLayer.h"
#include "protocol/Messages.h"

namespace core {

class PumpController {
public:
    enum class State : uint8_t { kBoot, kIdle, kPumping, kLinkLost };

    PumpController(protocol::LinkLayer& link, hal::IPumpActuator& pump)
        : link_(link), pump_(pump) {}

    void begin();
    void loop();

    State state() const { return state_; }

private:
    void onPacket(const protocol::Packet& pkt);
    void enter(State next);
    void applyCommand(protocol::PumpCommand cmd);

    protocol::LinkLayer& link_;
    hal::IPumpActuator& pump_;
    State state_ = State::kBoot;
    uint32_t lastValidRxMs_ = 0;
};

}  // namespace core
