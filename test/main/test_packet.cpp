// ============================================================================
//  test_packet.cpp  -  Quadro no ar: serializacao, CRC-16 e validacoes.
// ============================================================================

#include <unity.h>

#include "protocol/Crc16.h"
#include "protocol/Packet.h"

using namespace protocol;

namespace {

Packet samplePacket() {
    Packet p;
    p.src = 0x01;
    p.dst = 0x02;
    p.type = MessageType::kComandoBomba;
    p.flags = kFlagAckRequested;
    p.seq = 42;
    p.payload[0] = 1;
    p.payloadLen = 1;
    return p;
}

}  // namespace

TEST_CASE("CRC-16/CCITT-FALSE confere com o vetor conhecido", "[protocol]")
{
    // Vetor classico do CCITT-FALSE (poly 0x1021, init 0xFFFF): "123456789".
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16Ccitt(data, sizeof(data)));
}

TEST_CASE("encode/decode preserva todos os campos", "[protocol]")
{
    const Packet original = samplePacket();
    uint8_t wire[kMaxPacketSize];

    const size_t written = encodePacket(original, wire, sizeof(wire));
    TEST_ASSERT_EQUAL_UINT32(kHeaderSize + 1 + kCrcSize, written);
    TEST_ASSERT_EQUAL_HEX8(kSyncByte, wire[0]);
    TEST_ASSERT_EQUAL_HEX8(kProtocolVer, wire[1]);

    Packet decoded;
    TEST_ASSERT_TRUE(decodePacket(wire, written, decoded));
    TEST_ASSERT_EQUAL_HEX8(original.src, decoded.src);
    TEST_ASSERT_EQUAL_HEX8(original.dst, decoded.dst);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(original.type),
                            static_cast<uint8_t>(decoded.type));
    TEST_ASSERT_EQUAL_HEX8(original.flags, decoded.flags);
    TEST_ASSERT_EQUAL_UINT8(original.seq, decoded.seq);
    TEST_ASSERT_EQUAL_UINT8(original.payloadLen, decoded.payloadLen);
    TEST_ASSERT_EQUAL_UINT8(original.payload[0], decoded.payload[0]);
    TEST_ASSERT_TRUE(decoded.wantsAck());
}

TEST_CASE("um bit trocado no payload reprova no CRC", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(samplePacket(), wire, sizeof(wire));

    wire[kHeaderSize] ^= 0x01;  // corrompe o payload, mantendo o CRC antigo

    Packet decoded;
    TEST_ASSERT_FALSE(decodePacket(wire, written, decoded));
}

TEST_CASE("quadro truncado nao decodifica", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(samplePacket(), wire, sizeof(wire));

    Packet decoded;
    TEST_ASSERT_FALSE(decodePacket(wire, written - 1, decoded));
    TEST_ASSERT_FALSE(decodePacket(wire, kHeaderSize - 1, decoded));
}

TEST_CASE("sync ou versao errados sao rejeitados", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(samplePacket(), wire, sizeof(wire));
    Packet decoded;

    const uint8_t goodSync = wire[0];
    wire[0] = 0x00;
    TEST_ASSERT_FALSE(decodePacket(wire, written, decoded));
    wire[0] = goodSync;

    wire[1] = 0x02;  // versao futura
    TEST_ASSERT_FALSE(decodePacket(wire, written, decoded));
}

TEST_CASE("payload acima do maximo nao e serializado", "[protocol]")
{
    Packet p = samplePacket();
    p.payloadLen = kMaxPayload + 1;

    uint8_t wire[kMaxPacketSize];
    TEST_ASSERT_EQUAL_UINT32(0, encodePacket(p, wire, sizeof(wire)));
}

TEST_CASE("payload do tamanho maximo faz a volta completa", "[protocol]")
{
    Packet p = samplePacket();
    p.type = MessageType::kBateriaStatus;
    p.payloadLen = kMaxPayload;
    for (size_t i = 0; i < kMaxPayload; ++i) p.payload[i] = static_cast<uint8_t>(i);

    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(p, wire, sizeof(wire));
    TEST_ASSERT_EQUAL_UINT32(kMaxPacketSize, written);

    Packet decoded;
    TEST_ASSERT_TRUE(decodePacket(wire, written, decoded));
    TEST_ASSERT_EQUAL_UINT8(kMaxPayload, decoded.payloadLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p.payload, decoded.payload, kMaxPayload);
}

TEST_CASE("buffer de saida pequeno nao e sobrescrito", "[protocol]")
{
    uint8_t tiny[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(0, encodePacket(samplePacket(), tiny, sizeof(tiny)));
    for (uint8_t b : tiny) TEST_ASSERT_EQUAL_HEX8(0, b);
}
