// ============================================================================
//  test_messages.cpp  -  Builders e parsers de cada tipo de mensagem.
// ============================================================================

#include <unity.h>

#include "protocol/Messages.h"

using namespace protocol;

TEST_CASE("COMANDO_BOMBA pede ACK e sobrevive ao ar", "[protocol]")
{
    const Packet p = makeCommand(0x01, 0x02, 7, PumpCommand::kOn);
    TEST_ASSERT_TRUE(p.wantsAck());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MessageType::kComandoBomba),
                            static_cast<uint8_t>(p.type));

    uint8_t wire[kMaxPacketSize];
    const size_t n = encodePacket(p, wire, sizeof(wire));
    Packet decoded;
    TEST_ASSERT_TRUE(decodePacket(wire, n, decoded));

    PumpCommand cmd = PumpCommand::kOff;
    TEST_ASSERT_TRUE(parseCommand(decoded, cmd));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpCommand::kOn),
                            static_cast<uint8_t>(cmd));
}

TEST_CASE("parseCommand recusa tipo errado e payload vazio", "[protocol]")
{
    PumpCommand cmd;
    TEST_ASSERT_FALSE(parseCommand(makeHeartbeat(0x01, 0x02, 1), cmd));

    Packet truncated = makeCommand(0x01, 0x02, 1, PumpCommand::kOn);
    truncated.payloadLen = 0;
    TEST_ASSERT_FALSE(parseCommand(truncated, cmd));
}

TEST_CASE("ACK carrega o SEQ confirmado e a flag", "[protocol]")
{
    const Packet ack = makeAck(0x02, 0x01, 99);
    TEST_ASSERT_TRUE(ack.isAck());
    TEST_ASSERT_FALSE(ack.wantsAck());
    TEST_ASSERT_EQUAL_UINT8(99, ack.seq);
}

TEST_CASE("STATUS_NIVEL leva estado e porcentagem", "[protocol]")
{
    LevelStatusPayload in;
    in.level = hw::LevelState::kLow;
    in.percent = 37;
    in.percentValid = true;

    LevelStatusPayload out;
    TEST_ASSERT_TRUE(parseLevelStatus(makeLevelStatus(0x01, 0xFF, 1, in), out));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(hw::LevelState::kLow),
                            static_cast<uint8_t>(out.level));
    TEST_ASSERT_TRUE(out.percentValid);
    TEST_ASSERT_EQUAL_UINT8(37, out.percent);
}

TEST_CASE("STATUS_NIVEL marca porcentagem invalida", "[protocol]")
{
    LevelStatusPayload in;
    in.level = hw::LevelState::kUnknown;
    in.percentValid = false;

    LevelStatusPayload out;
    out.percent = 55;
    TEST_ASSERT_TRUE(parseLevelStatus(makeLevelStatus(0x01, 0xFF, 1, in), out));
    TEST_ASSERT_FALSE(out.percentValid);
    TEST_ASSERT_EQUAL_UINT8(0, out.percent);
}

TEST_CASE("BATERIA_STATUS preserva tensao, corrente e fonte", "[protocol]")
{
    hw::BatteryTelemetry in;
    in.milliVolts = 3912;
    in.milliAmps = -250;  // carregando
    in.socPercent = 78;
    in.source = hw::PowerSource::kExternal;

    uint8_t wire[kMaxPacketSize];
    const size_t n = encodePacket(makeBatteryStatus(0x01, 0xFF, 3, in), wire,
                                  sizeof(wire));
    Packet decoded;
    TEST_ASSERT_TRUE(decodePacket(wire, n, decoded));

    hw::BatteryTelemetry out;
    TEST_ASSERT_TRUE(parseBatteryStatus(decoded, out));
    TEST_ASSERT_EQUAL_UINT16(3912, out.milliVolts);
    TEST_ASSERT_EQUAL_INT16(-250, out.milliAmps);
    TEST_ASSERT_EQUAL_UINT8(78, out.socPercent);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(hw::PowerSource::kExternal),
                            static_cast<uint8_t>(out.source));
    TEST_ASSERT_TRUE(out.currentValid);
}

TEST_CASE("BOMBA_STATUS leva bloqueio, saida e tempo ligada", "[protocol]")
{
    PumpStatusPayload in;
    in.state = PumpNodeState::kOverrun;
    in.overrunLatched = true;
    in.outputOn = false;
    in.runSeconds = 1234;

    const Packet p = makePumpStatus(0x02, 0x01, 5, in);
    TEST_ASSERT_FALSE(p.wantsAck());  // informativo: nao pode travar o atuador

    uint8_t wire[kMaxPacketSize];
    const size_t n = encodePacket(p, wire, sizeof(wire));
    Packet decoded;
    TEST_ASSERT_TRUE(decodePacket(wire, n, decoded));

    PumpStatusPayload out;
    TEST_ASSERT_TRUE(parsePumpStatus(decoded, out));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpNodeState::kOverrun),
                            static_cast<uint8_t>(out.state));
    TEST_ASSERT_TRUE(out.overrunLatched);
    TEST_ASSERT_FALSE(out.outputOn);
    TEST_ASSERT_EQUAL_UINT16(1234, out.runSeconds);
}

TEST_CASE("BOMBA_STATUS distingue bomba ligada sem bloqueio", "[protocol]")
{
    PumpStatusPayload in;
    in.state = PumpNodeState::kPumping;
    in.overrunLatched = false;
    in.outputOn = true;
    in.runSeconds = 12;

    PumpStatusPayload out;
    TEST_ASSERT_TRUE(parsePumpStatus(makePumpStatus(0x02, 0x01, 1, in), out));
    TEST_ASSERT_FALSE(out.overrunLatched);
    TEST_ASSERT_TRUE(out.outputOn);
    TEST_ASSERT_EQUAL_UINT16(12, out.runSeconds);
}

TEST_CASE("parsePumpStatus recusa tipo errado", "[protocol]")
{
    PumpStatusPayload out;
    TEST_ASSERT_FALSE(parsePumpStatus(makeHeartbeat(0x02, 0x01, 1), out));
}
