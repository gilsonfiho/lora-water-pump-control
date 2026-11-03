// ============================================================================
//  test_packet.cpp  -  Quadro no ar: serializacao, CRC-16, MAC e validacoes.
// ============================================================================

#include <unity.h>

#include "config/SecurityConfig.h"
#include "protocol/Crc16.h"
#include "protocol/Packet.h"
#include "protocol/SipHash.h"

using namespace protocol;

namespace {

const uint8_t* key() { return cfg::Security::kLinkKey; }

Packet samplePacket() {
    Packet p;
    p.src = 0x01;
    p.dst = 0x02;
    p.type = MessageType::kComandoBomba;
    p.flags = kFlagAckRequested;
    p.seq = 42;
    p.counter = 0x01020304;
    p.payload[0] = 1;
    p.payloadLen = 1;
    return p;
}

}  // namespace

TEST_CASE("CRC-16/CCITT-FALSE confere com o vetor conhecido", "[protocol]")
{
    const uint8_t data[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_ASSERT_EQUAL_HEX16(0x29B1, crc16Ccitt(data, sizeof(data)));
}

// A Unity do ESP-IDF vem sem suporte a 64 bits (TEST_ASSERT_EQUAL_HEX64),
// entao comparamos o hash em duas metades de 32 bits.
static void assertHash64(uint32_t hi, uint32_t lo, uint64_t got)
{
    TEST_ASSERT_EQUAL_HEX32(hi, static_cast<uint32_t>(got >> 32));
    TEST_ASSERT_EQUAL_HEX32(lo, static_cast<uint32_t>(got & 0xFFFFFFFFu));
}

TEST_CASE("SipHash-2-4 confere com os vetores oficiais", "[protocol]")
{
    // Vetores de referencia (Aumasson/Bernstein), chave 00 01 .. 0f.
    uint8_t k[16];
    for (int i = 0; i < 16; ++i) k[i] = static_cast<uint8_t>(i);

    assertHash64(0x726fdb47u, 0xdd0e0e31u, siphash24(k, nullptr, 0));

    const uint8_t msg8[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    assertHash64(0x93f5f579u, 0x9a932462u, siphash24(k, msg8, 8));
}

TEST_CASE("encode/decode preserva todos os campos", "[protocol]")
{
    const Packet original = samplePacket();
    uint8_t wire[kMaxPacketSize];

    const size_t written = encodePacket(original, key(), wire, sizeof(wire));
    TEST_ASSERT_EQUAL_UINT32(kHeaderSize + 1 + kMacSize + kCrcSize, written);
    TEST_ASSERT_EQUAL_HEX8(kSyncByte, wire[0]);
    TEST_ASSERT_EQUAL_HEX8(kProtocolVer, wire[1]);

    Packet decoded;
    TEST_ASSERT_TRUE(decodePacket(wire, written, key(), decoded));
    TEST_ASSERT_EQUAL_HEX8(original.src, decoded.src);
    TEST_ASSERT_EQUAL_HEX8(original.dst, decoded.dst);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(original.type),
                            static_cast<uint8_t>(decoded.type));
    TEST_ASSERT_EQUAL_HEX8(original.flags, decoded.flags);
    TEST_ASSERT_EQUAL_UINT8(original.seq, decoded.seq);
    TEST_ASSERT_EQUAL_UINT32(original.counter, decoded.counter);
    TEST_ASSERT_EQUAL_UINT8(original.payloadLen, decoded.payloadLen);
    TEST_ASSERT_EQUAL_UINT8(original.payload[0], decoded.payload[0]);
    TEST_ASSERT_TRUE(decoded.wantsAck());
}

TEST_CASE("um bit trocado no payload reprova no CRC", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(samplePacket(), key(), wire, sizeof(wire));

    wire[kHeaderSize] ^= 0x01;  // corrompe o payload, mantendo o CRC antigo

    Packet decoded;
    DecodeError err = DecodeError::kNone;
    TEST_ASSERT_FALSE(decodePacket(wire, written, key(), decoded, &err));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DecodeError::kCrc),
                            static_cast<uint8_t>(err));
}

TEST_CASE("adulteracao com CRC corrigido reprova no MAC", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const Packet p = samplePacket();
    const size_t written = encodePacket(p, key(), wire, sizeof(wire));

    // Troca o payload e RECALCULA o CRC -- so o MAC pega a adulteracao.
    wire[kHeaderSize] ^= 0x80;
    const size_t crcCovered = written - kCrcSize;
    const uint16_t crc = crc16Ccitt(wire, crcCovered);
    wire[crcCovered]     = static_cast<uint8_t>(crc >> 8);
    wire[crcCovered + 1] = static_cast<uint8_t>(crc & 0xFF);

    Packet decoded;
    DecodeError err = DecodeError::kNone;
    TEST_ASSERT_FALSE(decodePacket(wire, written, key(), decoded, &err));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DecodeError::kMac),
                            static_cast<uint8_t>(err));
}

TEST_CASE("chave errada reprova no MAC", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(samplePacket(), key(), wire, sizeof(wire));

    uint8_t wrongKey[16];
    for (int i = 0; i < 16; ++i) wrongKey[i] = cfg::Security::kLinkKey[i] ^ 0xFF;

    Packet decoded;
    DecodeError err = DecodeError::kNone;
    TEST_ASSERT_FALSE(decodePacket(wire, written, wrongKey, decoded, &err));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(DecodeError::kMac),
                            static_cast<uint8_t>(err));
}

TEST_CASE("quadro truncado nao decodifica", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(samplePacket(), key(), wire, sizeof(wire));

    Packet decoded;
    TEST_ASSERT_FALSE(decodePacket(wire, written - 1, key(), decoded));
    TEST_ASSERT_FALSE(decodePacket(wire, kHeaderSize - 1, key(), decoded));
}

TEST_CASE("sync ou versao errados sao rejeitados", "[protocol]")
{
    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(samplePacket(), key(), wire, sizeof(wire));
    Packet decoded;

    const uint8_t goodSync = wire[0];
    wire[0] = 0x00;
    TEST_ASSERT_FALSE(decodePacket(wire, written, key(), decoded));
    wire[0] = goodSync;

    wire[1] = 0x03;  // versao futura (atual e 0x02)
    TEST_ASSERT_FALSE(decodePacket(wire, written, key(), decoded));
}

TEST_CASE("payload acima do maximo nao e serializado", "[protocol]")
{
    Packet p = samplePacket();
    p.payloadLen = kMaxPayload + 1;

    uint8_t wire[kMaxPacketSize];
    TEST_ASSERT_EQUAL_UINT32(0, encodePacket(p, key(), wire, sizeof(wire)));
}

TEST_CASE("payload do tamanho maximo faz a volta completa", "[protocol]")
{
    Packet p = samplePacket();
    p.type = MessageType::kBateriaStatus;
    p.payloadLen = kMaxPayload;
    for (size_t i = 0; i < kMaxPayload; ++i) p.payload[i] = static_cast<uint8_t>(i);

    uint8_t wire[kMaxPacketSize];
    const size_t written = encodePacket(p, key(), wire, sizeof(wire));
    TEST_ASSERT_EQUAL_UINT32(kMaxPacketSize, written);

    Packet decoded;
    TEST_ASSERT_TRUE(decodePacket(wire, written, key(), decoded));
    TEST_ASSERT_EQUAL_UINT8(kMaxPayload, decoded.payloadLen);
    TEST_ASSERT_EQUAL_UINT8_ARRAY(p.payload, decoded.payload, kMaxPayload);
}

TEST_CASE("buffer de saida pequeno nao e sobrescrito", "[protocol]")
{
    uint8_t tiny[4] = {0, 0, 0, 0};
    TEST_ASSERT_EQUAL_UINT32(0, encodePacket(samplePacket(), key(), tiny, sizeof(tiny)));
    for (uint8_t b : tiny) TEST_ASSERT_EQUAL_HEX8(0, b);
}
