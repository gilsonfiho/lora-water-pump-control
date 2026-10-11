#pragma once
// ============================================================================
//  PumpController.h  -  Maquina de estados do no da bomba (receptor/atuador).
// ----------------------------------------------------------------------------
//  Estados:
//    kBoot     : inicializando; bomba DESLIGADA.
//    kIdle     : enlace ok, comando atual = desligar.
//    kPumping  : enlace ok, comando atual = ligar.
//    kLinkLost : sem trafego valido dentro do timeout -> failsafe DESLIGA.
//    kOverrun  : passou de kMaxPumpRunMs ligada -> failsafe local DESLIGA.
//
//  Dois failsafes independentes:
//
//  1) ENLACE: qualquer ausencia de comando/heartbeat por mais de
//     kLinkLostTimeoutMs desliga a bomba. So sai de kLinkLost ao receber
//     trafego valido novamente.
//
//  2) TEMPO MAXIMO LIGADA (defesa em profundidade, decidida AQUI e nao pelo
//     reservatorio): passar de kMaxPumpRunMs bombeando de forma continua
//     desliga a bomba, mesmo com o enlace perfeito. Protege contra boia
//     travada, comando ON preso por bug e comando forjado.
//
//     O bloqueio e um LATCH (overrunLatched_), nao apenas um estado: se fosse
//     estado, um silencio de radio levaria a kLinkLost e o ON seguinte
//     religaria a bomba, o que daria um jeito trivial de burlar o limite. So um
//     comando OFF explicito do reservatorio rearma -- na pratica, so quando a
//     caixa der "cheia" (ou a boia for consertada).
// ============================================================================

#include <cstdint>

#include "hw/IPumpActuator.h"
#include "protocol/LinkLayer.h"
#include "protocol/Messages.h"

namespace core {

class PumpController {
public:
    enum class State : uint8_t { kBoot, kIdle, kPumping, kLinkLost, kOverrun };

    PumpController(protocol::LinkLayer& link, hw::IPumpActuator& pump,
                   uint8_t reservoirNodeAddress)
        : link_(link), pump_(pump), reservoirNodeAddress_(reservoirNodeAddress) {}

    void begin();
    void loop();

    State state() const { return state_; }

    // Bloqueio por tempo maximo ativo; so um comando OFF rearma.
    bool overrunLatched() const { return overrunLatched_; }

private:
    void onPacket(const protocol::Packet& pkt);
    void enter(State next);
    void applyCommand(protocol::PumpCommand cmd);
    void reportStatus() const;

    protocol::LinkLayer& link_;
    hw::IPumpActuator& pump_;
    uint8_t reservoirNodeAddress_;
    State state_ = State::kBoot;
    uint32_t lastValidRxMs_ = 0;

    // Failsafe de tempo maximo ligada.
    uint32_t pumpingSinceMs_ = 0;
    bool overrunLatched_ = false;
};

}  // namespace core
