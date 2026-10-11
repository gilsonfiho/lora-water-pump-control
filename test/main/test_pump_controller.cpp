// ============================================================================
//  test_pump_controller.cpp  -  Maquina de estados do no da bomba.
// ----------------------------------------------------------------------------
//  Cobre os dois failsafes e, em especial, o latch de tempo maximo da
//  issue #9: o bloqueio tem de sobreviver a uma queda de enlace, senao um
//  silencio de radio o limparia e o LIGAR seguinte religaria a bomba.
// ============================================================================

#include <unity.h>

#include <vector>

#include "config/NodeConfig.h"
#include "core/PumpController.h"
#include "fakes.h"
#include "protocol/LinkLayer.h"
#include "protocol/Messages.h"

using core::PumpController;
using test::FakeClock;
using test::FakePump;
using test::FakeRadio;

namespace {

constexpr uint8_t kPumpAddr = cfg::kAddrPump;
constexpr uint8_t kResAddr  = cfg::kAddrReservoir;

// Contexto completo do no da bomba, com radio e atuador falsos.
struct PumpFixture {
    FakeRadio radio;
    FakePump pump;
    protocol::LinkLayer link{radio, kPumpAddr};
    PumpController controller{link, pump, kResAddr};

    PumpFixture() {
        FakeClock::install();
        controller.begin();
    }
    ~PumpFixture() { FakeClock::uninstall(); }

    // Entrega um pacote como se tivesse vindo do reservatorio.
    void deliver(const protocol::Packet& pkt) {
        uint8_t wire[protocol::kMaxPacketSize];
        const size_t n = protocol::encodePacket(pkt, wire, sizeof(wire));
        radio.inject(wire, n);
        controller.loop();
    }

    void sendCommand(protocol::PumpCommand cmd) {
        deliver(protocol::makeCommand(kResAddr, kPumpAddr, ++seq_, cmd));
    }

    void sendHeartbeat() {
        deliver(protocol::makeHeartbeat(kResAddr, kPumpAddr, ++seq_));
    }

    // Avanca o relogio mantendo o ENLACE VIVO, com heartbeats na cadencia real.
    // E o que o reservatorio faz em operacao normal. Sem isso, o failsafe de
    // enlace (15 s) dispararia antes do limite de bombeamento (60 s) e os
    // testes do tempo maximo nunca chegariam ao que querem exercitar.
    void tick(uint32_t ms, uint32_t stepMs = 100) {
        for (uint32_t elapsed = 0; elapsed < ms; elapsed += stepMs) {
            FakeClock::advance(stepMs);
            if (FakeClock::now() - lastBeatMs_ >= cfg::Timing::kHeartbeatPeriodMs) {
                lastBeatMs_ = FakeClock::now();
                sendHeartbeat();  // ja chama loop()
                continue;
            }
            controller.loop();
        }
    }

    // Avanca o relogio em SILENCIO: nada chega do reservatorio. Usado para
    // exercitar o failsafe de perda de enlace.
    void tickSilent(uint32_t ms, uint32_t stepMs = 100) {
        for (uint32_t elapsed = 0; elapsed < ms; elapsed += stepMs) {
            FakeClock::advance(stepMs);
            controller.loop();
        }
    }

    // Ultimo BOMBA_STATUS que foi ao ar (ignora ACKs automaticos).
    bool lastStatus(protocol::PumpStatusPayload& out) const {
        for (size_t i = radio.sent.size(); i > 0; --i) {
            protocol::Packet p;
            const std::vector<uint8_t>& f = radio.sent[i - 1];
            if (!protocol::decodePacket(f.data(), f.size(), p)) continue;
            if (protocol::parsePumpStatus(p, out)) return true;
        }
        return false;
    }

private:
    uint8_t seq_ = 0;
    uint32_t lastBeatMs_ = 0;
};

}  // namespace

TEST_CASE("no boot a bomba fica DESLIGADA e em OCIOSO", "[core][pump]")
{
    PumpFixture f;
    TEST_ASSERT_EQUAL_INT(1, f.pump.beginCount);
    TEST_ASSERT_FALSE(f.pump.isOn());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kIdle),
                            static_cast<uint8_t>(f.controller.state()));
    TEST_ASSERT_FALSE(f.controller.overrunLatched());
}

TEST_CASE("begin() nao transmite nada", "[core][pump]")
{
    PumpFixture f;
    // A app chama begin() ANTES de radio.begin(). Se o boot reportasse status,
    // a escrita cairia numa UART nao instalada e o driver logaria
    // "uart driver error" em todo boot -- ruido justamente em quem depura
    // enlace pelo log.
    TEST_ASSERT_EQUAL_UINT32(0, f.radio.sent.size());
}

TEST_CASE("o src do BOMBA_STATUS vem do LinkLayer", "[core][pump]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);

    bool found = false;
    for (size_t i = f.radio.sent.size(); i > 0 && !found; --i) {
        protocol::Packet p;
        const std::vector<uint8_t>& fr = f.radio.sent[i - 1];
        if (!protocol::decodePacket(fr.data(), fr.size(), p)) continue;
        if (p.type != protocol::MessageType::kBombaStatus) continue;
        found = true;
        TEST_ASSERT_EQUAL_HEX8(kPumpAddr, p.src);
        TEST_ASSERT_EQUAL_HEX8(kResAddr, p.dst);
    }
    TEST_ASSERT_TRUE(found);
}

TEST_CASE("LIGAR liga a bomba; DESLIGAR desliga", "[core][pump]")
{
    PumpFixture f;

    f.sendCommand(protocol::PumpCommand::kOn);
    TEST_ASSERT_TRUE(f.pump.isOn());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kPumping),
                            static_cast<uint8_t>(f.controller.state()));

    f.sendCommand(protocol::PumpCommand::kOff);
    TEST_ASSERT_FALSE(f.pump.isOn());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kIdle),
                            static_cast<uint8_t>(f.controller.state()));
}

TEST_CASE("silencio prolongado desliga a bomba (failsafe de enlace)",
          "[core][pump]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);
    TEST_ASSERT_TRUE(f.pump.isOn());

    f.tickSilent(cfg::Timing::kLinkLostTimeoutMs + 200);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kLinkLost),
                            static_cast<uint8_t>(f.controller.state()));
    TEST_ASSERT_FALSE(f.pump.isOn());
}

TEST_CASE("enlace volta: heartbeat tira de ENLACE PERDIDO", "[core][pump]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);
    f.tickSilent(cfg::Timing::kLinkLostTimeoutMs + 200);

    f.sendHeartbeat();

    // A bomba foi desenergizada no failsafe, entao volta para OCIOSO: religar
    // exige um comando novo, nunca a simples volta do enlace.
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kIdle),
                            static_cast<uint8_t>(f.controller.state()));
    TEST_ASSERT_FALSE(f.pump.isOn());
}

TEST_CASE("heartbeat sozinho nunca liga a bomba", "[core][pump]")
{
    PumpFixture f;
    for (int i = 0; i < 5; ++i) f.sendHeartbeat();
    TEST_ASSERT_FALSE(f.pump.isOn());
}

// ---- Issue #9: tempo maximo de bombeamento --------------------------------

TEST_CASE("bomba desliga sozinha ao passar do tempo maximo", "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);

    // Heartbeats regulares: o enlace esta perfeito o tempo todo.
    for (uint32_t t = 0; t < cfg::Timing::kMaxPumpRunMs + 1000; t += 1000) {
        FakeClock::advance(1000);
        f.sendHeartbeat();
    }

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kOverrun),
                            static_cast<uint8_t>(f.controller.state()));
    TEST_ASSERT_FALSE(f.pump.isOn());
    TEST_ASSERT_TRUE(f.controller.overrunLatched());
}

TEST_CASE("nao bloqueia pouco antes do limite", "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);

    f.tick(cfg::Timing::kMaxPumpRunMs - 500);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kPumping),
                            static_cast<uint8_t>(f.controller.state()));
    TEST_ASSERT_TRUE(f.pump.isOn());
    TEST_ASSERT_FALSE(f.controller.overrunLatched());
}

TEST_CASE("com bloqueio travado, LIGAR e ignorado", "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);
    f.tick(cfg::Timing::kMaxPumpRunMs + 500);
    TEST_ASSERT_TRUE(f.controller.overrunLatched());

    const int offsBefore = f.pump.offTransitions;

    // O reservatorio reenvia LIGAR a cada ciclo: tudo tem de ser ignorado.
    for (int i = 0; i < 5; ++i) {
        f.sendCommand(protocol::PumpCommand::kOn);
        TEST_ASSERT_FALSE(f.pump.isOn());
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kOverrun),
                                static_cast<uint8_t>(f.controller.state()));
    }

    // A bomba nem piscou: nenhuma transicao nova de desligamento.
    TEST_ASSERT_EQUAL_INT(offsBefore, f.pump.offTransitions);
    TEST_ASSERT_EQUAL_INT(1, f.pump.onTransitions);
}

TEST_CASE("DESLIGAR rearma o bloqueio e a bomba volta a funcionar",
          "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);
    f.tick(cfg::Timing::kMaxPumpRunMs + 500);
    TEST_ASSERT_TRUE(f.controller.overrunLatched());

    f.sendCommand(protocol::PumpCommand::kOff);
    TEST_ASSERT_FALSE(f.controller.overrunLatched());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kIdle),
                            static_cast<uint8_t>(f.controller.state()));

    f.sendCommand(protocol::PumpCommand::kOn);
    TEST_ASSERT_TRUE(f.pump.isOn());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kPumping),
                            static_cast<uint8_t>(f.controller.state()));
}

TEST_CASE("depois do rearme o cronometro comeca do zero", "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);
    f.tick(cfg::Timing::kMaxPumpRunMs + 500);
    f.sendCommand(protocol::PumpCommand::kOff);
    f.sendCommand(protocol::PumpCommand::kOn);

    // Quase um ciclo inteiro depois do rearme: ainda bombeando.
    f.tick(cfg::Timing::kMaxPumpRunMs - 500);
    TEST_ASSERT_TRUE(f.pump.isOn());

    // Passando do limite, bloqueia de novo.
    f.tick(1000);
    TEST_ASSERT_FALSE(f.pump.isOn());
    TEST_ASSERT_TRUE(f.controller.overrunLatched());
}

TEST_CASE("reenvio de LIGAR nao reinicia o cronometro", "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);

    // Metade do tempo, depois um LIGAR repetido (reenvio periodico de robustez).
    f.tick(cfg::Timing::kMaxPumpRunMs / 2);
    f.sendCommand(protocol::PumpCommand::kOn);
    TEST_ASSERT_TRUE(f.pump.isOn());

    // Completando o limite contado desde o PRIMEIRO LIGAR, tem de bloquear.
    f.tick(cfg::Timing::kMaxPumpRunMs / 2 + 1000);

    TEST_ASSERT_TRUE(f.controller.overrunLatched());
    TEST_ASSERT_FALSE(f.pump.isOn());
}

TEST_CASE("o bloqueio sobrevive a queda e a volta do enlace",
          "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);
    f.tick(cfg::Timing::kMaxPumpRunMs + 500);
    TEST_ASSERT_TRUE(f.controller.overrunLatched());

    // Enlace cai: o estado passa a ENLACE PERDIDO, mas o latch continua.
    f.tickSilent(cfg::Timing::kLinkLostTimeoutMs + 500);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kLinkLost),
                            static_cast<uint8_t>(f.controller.state()));
    TEST_ASSERT_TRUE(f.controller.overrunLatched());

    // Enlace volta. Sem o latch, o no voltaria para OCIOSO e o LIGAR seguinte
    // religaria a bomba -- ou seja, derrubar o radio burlaria o limite.
    f.sendHeartbeat();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpController::State::kOverrun),
                            static_cast<uint8_t>(f.controller.state()));

    f.sendCommand(protocol::PumpCommand::kOn);
    TEST_ASSERT_FALSE(f.pump.isOn());
    TEST_ASSERT_TRUE(f.controller.overrunLatched());
}

TEST_CASE("o bloqueio e reportado em BOMBA_STATUS", "[core][pump][issue9]")
{
    PumpFixture f;
    f.sendCommand(protocol::PumpCommand::kOn);

    protocol::PumpStatusPayload status;
    TEST_ASSERT_TRUE(f.lastStatus(status));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpNodeState::kPumping),
                            static_cast<uint8_t>(status.state));
    TEST_ASSERT_TRUE(status.outputOn);
    TEST_ASSERT_FALSE(status.overrunLatched);

    f.tick(cfg::Timing::kMaxPumpRunMs + 500);

    TEST_ASSERT_TRUE(f.lastStatus(status));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpNodeState::kOverrun),
                            static_cast<uint8_t>(status.state));
    TEST_ASSERT_TRUE(status.overrunLatched);
    TEST_ASSERT_FALSE(status.outputOn);

    // O status do bloqueio diz quanto tempo a bomba ficou ligada antes dele.
    const uint16_t limitSeconds = cfg::Timing::kMaxPumpRunMs / 1000;
    TEST_ASSERT_GREATER_OR_EQUAL_UINT16(limitSeconds, status.runSeconds);
    TEST_ASSERT_LESS_OR_EQUAL_UINT16(limitSeconds + 5, status.runSeconds);
}
