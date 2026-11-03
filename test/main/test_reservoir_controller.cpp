// ============================================================================
//  test_reservoir_controller.cpp  -  Decisao de nivel, histerese, heartbeat e
//                                    telemetria do no do reservatorio.
// ============================================================================

#include <unity.h>

#include <vector>

#include "config/NodeConfig.h"
#include "config/SecurityConfig.h"
#include "core/ReservoirController.h"
#include "fakes.h"
#include "protocol/LinkLayer.h"
#include "protocol/Messages.h"

using core::ReservoirController;
using test::FakeBattery;
using test::FakeClock;
using test::FakeLevelSensor;
using test::FakeRadio;

namespace {

struct ReservoirFixture {
    FakeRadio radio;
    FakeLevelSensor sensor;
    FakeBattery battery;
    protocol::LinkLayer link{radio, cfg::kAddrReservoir};
    ReservoirController controller{link, sensor, battery, cfg::kAddrPump};
    uint32_t counter_ = 0;  // frescor crescente dos pacotes injetados

    ReservoirFixture() {
        FakeClock::install();
        controller.begin();
    }
    ~ReservoirFixture() { FakeClock::uninstall(); }

    void tick(uint32_t ms, uint32_t stepMs = 100) {
        for (uint32_t elapsed = 0; elapsed < ms; elapsed += stepMs) {
            FakeClock::advance(stepMs);
            controller.loop();
        }
    }

    // Confirma a transacao confiavel em voo, para o proximo comando poder sair.
    void ackPending() {
        for (size_t i = radio.sent.size(); i > 0; --i) {
            protocol::Packet p;
            const std::vector<uint8_t>& f = radio.sent[i - 1];
            if (!protocol::decodePacket(f.data(), f.size(), cfg::Security::kLinkKey, p)) continue;
            if (p.type != protocol::MessageType::kComandoBomba) continue;
            uint8_t wire[protocol::kMaxPacketSize];
            protocol::Packet ack =
                protocol::makeAck(cfg::kAddrPump, cfg::kAddrReservoir, p.seq);
            ack.counter = ++counter_;
            const size_t n =
                protocol::encodePacket(ack, cfg::Security::kLinkKey, wire, sizeof(wire));
            radio.inject(wire, n);
            controller.loop();
            return;
        }
    }

    // Conta quadros transmitidos de um tipo.
    int count(protocol::MessageType type) const {
        int total = 0;
        for (const std::vector<uint8_t>& f : radio.sent) {
            protocol::Packet p;
            if (protocol::decodePacket(f.data(), f.size(), cfg::Security::kLinkKey, p) && p.type == type) {
                ++total;
            }
        }
        return total;
    }

    // Ultimo comando de bomba que foi ao ar.
    bool lastCommand(protocol::PumpCommand& out) const {
        for (size_t i = radio.sent.size(); i > 0; --i) {
            protocol::Packet p;
            const std::vector<uint8_t>& f = radio.sent[i - 1];
            if (!protocol::decodePacket(f.data(), f.size(), cfg::Security::kLinkKey, p)) continue;
            if (protocol::parseCommand(p, out)) return true;
        }
        return false;
    }
};

}  // namespace

TEST_CASE("caixa baixa manda LIGAR", "[core][reservoir]")
{
    ReservoirFixture f;
    f.sensor.level = hw::LevelState::kLow;

    f.tick(cfg::Timing::kReservoirCyclePeriodMs + 200);

    protocol::PumpCommand cmd;
    TEST_ASSERT_TRUE(f.lastCommand(cmd));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpCommand::kOn),
                            static_cast<uint8_t>(cmd));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpCommand::kOn),
                            static_cast<uint8_t>(f.controller.desiredCommand()));
}

TEST_CASE("caixa cheia manda DESLIGAR", "[core][reservoir]")
{
    ReservoirFixture f;
    f.sensor.level = hw::LevelState::kLow;
    f.tick(cfg::Timing::kReservoirCyclePeriodMs + 200);
    f.ackPending();

    f.sensor.level = hw::LevelState::kHigh;
    f.tick(cfg::Timing::kReservoirCyclePeriodMs + 200);

    protocol::PumpCommand cmd;
    TEST_ASSERT_TRUE(f.lastCommand(cmd));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpCommand::kOff),
                            static_cast<uint8_t>(cmd));
}

TEST_CASE("nivel intermediario mantem o comando (histerese)", "[core][reservoir]")
{
    ReservoirFixture f;
    f.sensor.level = hw::LevelState::kLow;
    f.tick(cfg::Timing::kReservoirCyclePeriodMs + 200);
    f.ackPending();
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpCommand::kOn),
                            static_cast<uint8_t>(f.controller.desiredCommand()));

    // Subiu para o meio: continua enchendo, sem liga-desliga.
    f.sensor.level = hw::LevelState::kMid;
    f.tick(cfg::Timing::kReservoirCyclePeriodMs * 3);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpCommand::kOn),
                            static_cast<uint8_t>(f.controller.desiredCommand()));
}

TEST_CASE("sensor falho manda DESLIGAR (seguro)", "[core][reservoir]")
{
    ReservoirFixture f;
    f.sensor.level = hw::LevelState::kLow;
    f.tick(cfg::Timing::kReservoirCyclePeriodMs + 200);
    f.ackPending();

    f.sensor.level = hw::LevelState::kUnknown;
    f.tick(cfg::Timing::kReservoirCyclePeriodMs + 200);

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(protocol::PumpCommand::kOff),
                            static_cast<uint8_t>(f.controller.desiredCommand()));
}

TEST_CASE("mudanca de nivel emite evento para o Observer", "[core][reservoir]")
{
    ReservoirFixture f;
    std::vector<core::LevelChangedEvent> events;
    f.controller.onLevelChanged().subscribe(
        [&](const core::LevelChangedEvent& e) { events.push_back(e); });

    f.sensor.level = hw::LevelState::kLow;
    f.tick(cfg::Timing::kReservoirCyclePeriodMs + 200);

    TEST_ASSERT_EQUAL_UINT32(1, events.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(hw::LevelState::kUnknown),
                            static_cast<uint8_t>(events[0].previous));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(hw::LevelState::kLow),
                            static_cast<uint8_t>(events[0].current));

    // Nivel estavel nao gera evento novo.
    f.tick(cfg::Timing::kReservoirCyclePeriodMs * 2);
    TEST_ASSERT_EQUAL_UINT32(1, events.size());
}

TEST_CASE("heartbeat sai na cadencia configurada", "[core][reservoir]")
{
    ReservoirFixture f;
    f.sensor.level = hw::LevelState::kHigh;  // sem comando para atrapalhar

    f.tick(cfg::Timing::kHeartbeatPeriodMs * 3 + 200);

    const int beats = f.count(protocol::MessageType::kHeartbeat);
    TEST_ASSERT_GREATER_OR_EQUAL_INT(3, beats);
    TEST_ASSERT_LESS_OR_EQUAL_INT(5, beats);
}

TEST_CASE("telemetria de bateria sai na cadencia e com os valores lidos",
          "[core][reservoir]")
{
    ReservoirFixture f;
    f.battery.telemetry.milliVolts = 4050;
    f.battery.telemetry.socPercent = 92;
    f.battery.telemetry.source = hw::PowerSource::kBattery;

    f.tick(cfg::Timing::kBatteryReportPeriodMs + 500);

    TEST_ASSERT_GREATER_OR_EQUAL_INT(1, f.count(protocol::MessageType::kBateriaStatus));

    // Confere o conteudo do ultimo relatorio.
    hw::BatteryTelemetry out;
    bool found = false;
    for (size_t i = f.radio.sent.size(); i > 0 && !found; --i) {
        protocol::Packet p;
        const std::vector<uint8_t>& fr = f.radio.sent[i - 1];
        if (!protocol::decodePacket(fr.data(), fr.size(), cfg::Security::kLinkKey, p)) continue;
        found = protocol::parseBatteryStatus(p, out);
    }
    TEST_ASSERT_TRUE(found);
    TEST_ASSERT_EQUAL_UINT16(4050, out.milliVolts);
    TEST_ASSERT_EQUAL_UINT8(92, out.socPercent);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(hw::PowerSource::kBattery),
                            static_cast<uint8_t>(out.source));
}

TEST_CASE("BOMBA_STATUS recebido vira evento no reservatorio",
          "[core][reservoir][issue9]")
{
    ReservoirFixture f;
    std::vector<protocol::PumpStatusPayload> seen;
    f.controller.onPumpStatus().subscribe(
        [&](const protocol::PumpStatusPayload& s) { seen.push_back(s); });

    protocol::PumpStatusPayload status;
    status.state = protocol::PumpNodeState::kOverrun;
    status.overrunLatched = true;
    status.runSeconds = 61;

    uint8_t wire[protocol::kMaxPacketSize];
    protocol::Packet st =
        protocol::makePumpStatus(cfg::kAddrPump, cfg::kAddrReservoir, 1, status);
    st.counter = 1;
    const size_t n = protocol::encodePacket(st, cfg::Security::kLinkKey, wire, sizeof(wire));
    f.radio.inject(wire, n);
    f.controller.loop();

    TEST_ASSERT_EQUAL_UINT32(1, seen.size());
    TEST_ASSERT_TRUE(seen[0].overrunLatched);
    TEST_ASSERT_EQUAL_UINT16(61, seen[0].runSeconds);
}

TEST_CASE("nao empilha transacoes: cada SEQ vive uma rodada de retentativas",
          "[core][reservoir]")
{
    ReservoirFixture f;
    f.sensor.level = hw::LevelState::kLow;

    // Varios ciclos sem nenhum ACK. Dentro de uma transacao, as retransmissoes
    // reusam o mesmo SEQ. Esgotadas as retentativas, a entrega falha e o
    // reservatorio tem o direito de abrir uma transacao nova (SEQ novo) --
    // o que nao pode e haver duas em voo ao mesmo tempo.
    f.tick(cfg::Timing::kReservoirCyclePeriodMs * 3);

    // Conta quantos quadros cada SEQ de comando gerou.
    uint8_t seqs[16] = {0};
    int counts[16] = {0};
    size_t distinct = 0;
    for (const std::vector<uint8_t>& fr : f.radio.sent) {
        protocol::Packet p;
        if (!protocol::decodePacket(fr.data(), fr.size(), cfg::Security::kLinkKey, p)) continue;
        if (p.type != protocol::MessageType::kComandoBomba) continue;
        size_t idx = distinct;
        for (size_t i = 0; i < distinct; ++i) {
            if (seqs[i] == p.seq) { idx = i; break; }
        }
        if (idx == distinct && distinct < 16) seqs[distinct++] = p.seq;
        if (idx < 16) ++counts[idx];
    }

    TEST_ASSERT_GREATER_OR_EQUAL_UINT32(1, distinct);
    for (size_t i = 0; i < distinct; ++i) {
        // 1 envio + no maximo kMaxRetries retransmissoes por transacao.
        TEST_ASSERT_LESS_OR_EQUAL_INT(1 + cfg::Timing::kMaxRetries, counts[i]);
    }
}
