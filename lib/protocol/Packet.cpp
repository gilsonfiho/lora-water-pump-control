// ============================================================================
//  Packet.cpp  -  Serializacao/desserializacao do formato de pacote.
// ============================================================================

#include "Packet.h"

#include <cstring>

#include "Crc16.h"

namespace protocol {

size_t encodePacket(const Packet& pkt, uint8_t* out, size_t outSize) {
    if (pkt.payloadLen > kMaxPayload) return 0;
    const size_t total = kHeaderSize + pkt.payloadLen + kCrcSize;
    if (outSize < total) return 0;

    out[0] = kSyncByte;
    out[1] = kProtocolVer;
    out[2] = pkt.src;
    out[3] = pkt.dst;
    out[4] = static_cast<uint8_t>(pkt.type);
    out[5] = pkt.flags;
    out[6] = pkt.seq;
    out[7] = pkt.payloadLen;
    memcpy(out + kHeaderSize, pkt.payload, pkt.payloadLen);

    const uint16_t crc = crc16Ccitt(out, kHeaderSize + pkt.payloadLen);
    out[kHeaderSize + pkt.payloadLen]     = static_cast<uint8_t>(crc >> 8);
    out[kHeaderSize + pkt.payloadLen + 1] = static_cast<uint8_t>(crc & 0xFF);
    return total;
}

bool decodePacket(const uint8_t* in, size_t inLength, Packet& pkt) {
    if (inLength < kHeaderSize + kCrcSize) return false;
    if (in[0] != kSyncByte || in[1] != kProtocolVer) return false;

    const uint8_t payloadLen = in[7];
    if (payloadLen > kMaxPayload) return false;

    const size_t total = kHeaderSize + payloadLen + kCrcSize;
    if (inLength < total) return false;

    const uint16_t received = static_cast<uint16_t>(in[kHeaderSize + payloadLen] << 8) |
                              in[kHeaderSize + payloadLen + 1];
    const uint16_t computed = crc16Ccitt(in, kHeaderSize + payloadLen);
    if (received != computed) return false;

    pkt.src        = in[2];
    pkt.dst        = in[3];
    pkt.type       = static_cast<MessageType>(in[4]);
    pkt.flags      = in[5];
    pkt.seq        = in[6];
    pkt.payloadLen = payloadLen;
    memcpy(pkt.payload, in + kHeaderSize, payloadLen);
    return true;
}

}  // namespace protocol
