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

    // Estado inicial direto, SEM passar por enter(): a app chama begin() antes
    // de radio.begin() (o rele tem de ir para o estado seguro antes de
    // qualquer espera de hardware), e enter() reportaria status por uma UART
    // ainda nao instalada -- o que faz o driver logar "uart driver error" em
    // todo boot. O primeiro BOMBA_STATUS sai na primeira transicao real.
    state_ = State::kIdle;
}

void PumpController::loop() {
    link_.poll();

    // Failsafe 1: silencio prolongado -> desliga por seguranca.
    const bool linkStale =
        (platform::millis() - lastValidRxMs_) > cfg::Timing::kLinkLostTimeoutMs;
    if (linkStale && state_ != State::kLinkLost) {
        enter(State::kLinkLost);
        return;
    }

    // Failsafe 2: tempo maximo ligada de forma continua. Independe do enlace e
    // do reservatorio -- e por isso que existe.
    if (state_ == State::kPumping &&
        (platform::millis() - pumpingSinceMs_) > cfg::Timing::kMaxPumpRunMs) {
        overrunLatched_ = true;  // so um OFF explicito rearma
        enter(State::kOverrun);
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
    // coerente com a bomba atual. Um bloqueio por tempo maximo sobrevive a
    // queda do enlace: sem isso, bastaria um silencio de radio para limpa-lo.
    if (pkt.type == protocol::MessageType::kHeartbeat &&
        state_ == State::kLinkLost) {
        if (overrunLatched_) {
            enter(State::kOverrun);
            return;
        }
        enter(pump_.isOn() ? State::kPumping : State::kIdle);
    }
}

void PumpController::applyCommand(protocol::PumpCommand cmd) {
    if (cmd == protocol::PumpCommand::kOn) {
        // Com o bloqueio travado, LIGAR e ignorado -- inclusive o reenvio
        // periodico do reservatorio, que repete o mesmo comando a cada ciclo.
        enter(overrunLatched_ ? State::kOverrun : State::kPumping);
        return;
    }

    // DESLIGAR e o unico rearme: a caixa encheu, ou a boia foi consertada.
    overrunLatched_ = false;
    enter(State::kIdle);
}

void PumpController::enter(State next) {
    // Sai cedo quando nada muda: assim o reenvio periodico de LIGAR nao
    // reinicia o cronometro de bombeamento. O que conta e o tempo ligada SEM
    // interrupcao.
    if (state_ == next) return;
    state_ = next;

    switch (next) {
        case State::kPumping:
            pumpingSinceMs_ = platform::millis();
            pump_.turnOn();
            break;
        case State::kIdle:
        case State::kLinkLost:
        case State::kOverrun:
        case State::kBoot:
            pump_.turnOff();  // todo estado nao-bombeante desenergiza
            break;
    }

    reportStatus();
}

void PumpController::reportStatus() const {
    protocol::PumpStatusPayload s;
    switch (state_) {
        case State::kBoot:     s.state = protocol::PumpNodeState::kBoot;     break;
        case State::kIdle:     s.state = protocol::PumpNodeState::kIdle;     break;
        case State::kPumping:  s.state = protocol::PumpNodeState::kPumping;  break;
        case State::kLinkLost: s.state = protocol::PumpNodeState::kLinkLost; break;
        case State::kOverrun:  s.state = protocol::PumpNodeState::kOverrun;  break;
    }
    s.overrunLatched = overrunLatched_;
    s.outputOn       = pump_.isOn();

    // Tambem em kOverrun: e justamente ali que saber quanto tempo a bomba
    // ficou ligada antes de bloquear tem valor para quem le a telemetria.
    s.runSeconds = (state_ == State::kPumping || state_ == State::kOverrun)
                       ? static_cast<uint16_t>(
                             (platform::millis() - pumpingSinceMs_) / 1000)
                       : 0;

    // sendOnce: informativo e sem ACK. O laco do atuador nao pode ficar preso
    // esperando confirmacao, e perder um status nao afeta a seguranca. O src
    // fica em 0: o LinkLayer preenche com o endereco local do no.
    link_.sendOnce(protocol::makePumpStatus(0, reservoirNodeAddress_, 0, s));
}

}  // namespace core
