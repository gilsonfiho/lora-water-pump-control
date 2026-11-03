// ============================================================================
//  Packet.cpp  -  Serializacao/desserializacao do formato de pacote (v2).
// ============================================================================

#include "protocol/Packet.h"

#include <cstring>

#include "protocol/Crc16.h"
#include "protocol/SipHash.h"

namespace protocol {

namespace {

void writeMac(const uint8_t key[kKeySize], const uint8_t* authData,
              size_t authLen, uint8_t* out) {
    const uint64_t mac = siphash24(key, authData, authLen);
    for (size_t j = 0; j < kMacSize; ++j) {
        out[j] = static_cast<uint8_t>(mac >> (8 * j));  // little-endian
    }
}

bool macMatches(const uint8_t key[kKeySize], const uint8_t* authData,
                size_t authLen, const uint8_t* macBytes) {
    uint8_t expected[kMacSize];
    writeMac(key, authData, authLen, expected);
    // Comparacao de tempo constante: nao vaza em quais bytes diverge.
    uint8_t diff = 0;
    for (size_t j = 0; j < kMacSize; ++j) diff |= expected[j] ^ macBytes[j];
    return diff == 0;
}

}  // namespace

size_t encodePacket(const Packet& pkt, const uint8_t key[kKeySize],
                    uint8_t* out, size_t outSize) {
    if (pkt.payloadLen > kMaxPayload) return 0;
    const size_t total = kHeaderSize + pkt.payloadLen + kMacSize + kCrcSize;
    if (outSize < total) return 0;

    out[0] = kSyncByte;
    out[1] = kProtocolVer;
    out[2] = pkt.src;
    out[3] = pkt.dst;
    out[4] = static_cast<uint8_t>(pkt.type);
    out[5] = pkt.flags;
    out[6] = pkt.seq;
    out[7] = pkt.payloadLen;
    out[8]  = static_cast<uint8_t>(pkt.counter >> 24);
    out[9]  = static_cast<uint8_t>(pkt.counter >> 16);
    out[10] = static_cast<uint8_t>(pkt.counter >> 8);
    out[11] = static_cast<uint8_t>(pkt.counter & 0xFF);
    memcpy(out + kHeaderSize, pkt.payload, pkt.payloadLen);

    // MAC sobre SYNC..PAYLOAD; depois CRC sobre tudo (incl. MAC) para framing.
    const size_t authLen = kHeaderSize + pkt.payloadLen;
    writeMac(key, out, authLen, out + authLen);

    const size_t crcCovered = authLen + kMacSize;
    const uint16_t crc = crc16Ccitt(out, crcCovered);
    out[crcCovered]     = static_cast<uint8_t>(crc >> 8);
    out[crcCovered + 1] = static_cast<uint8_t>(crc & 0xFF);
    return total;
}

bool decodePacket(const uint8_t* in, size_t inLength, const uint8_t key[kKeySize],
                  Packet& pkt, DecodeError* err) {
    auto fail = [&](DecodeError e) { if (err) *err = e; return false; };

    if (inLength < kHeaderSize + kMacSize + kCrcSize) return fail(DecodeError::kFraming);
    if (in[0] != kSyncByte || in[1] != kProtocolVer) return fail(DecodeError::kFraming);

    const uint8_t payloadLen = in[7];
    if (payloadLen > kMaxPayload) return fail(DecodeError::kFraming);

    const size_t authLen = kHeaderSize + payloadLen;
    const size_t crcCovered = authLen + kMacSize;
    const size_t total = crcCovered + kCrcSize;
    if (inLength < total) return fail(DecodeError::kFraming);

    const uint16_t received = static_cast<uint16_t>(in[crcCovered] << 8) | in[crcCovered + 1];
    if (received != crc16Ccitt(in, crcCovered)) return fail(DecodeError::kCrc);

    if (!macMatches(key, in, authLen, in + authLen)) return fail(DecodeError::kMac);

    pkt.src        = in[2];
    pkt.dst        = in[3];
    pkt.type       = static_cast<MessageType>(in[4]);
    pkt.flags      = in[5];
    pkt.seq        = in[6];
    pkt.counter    = (static_cast<uint32_t>(in[8]) << 24) |
                     (static_cast<uint32_t>(in[9]) << 16) |
                     (static_cast<uint32_t>(in[10]) << 8) |
                     static_cast<uint32_t>(in[11]);
    pkt.payloadLen = payloadLen;
    memcpy(pkt.payload, in + kHeaderSize, payloadLen);
    if (err) *err = DecodeError::kNone;
    return true;
}

}  // namespace protocol
