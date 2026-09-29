#pragma once
// ============================================================================
//  Packet.h  -  Formato de pacote da camada de aplicacao/enlace do projeto.
// ----------------------------------------------------------------------------
//  Layout do quadro no ar (todos os campos multi-byte em big-endian):
//
//    +------+------+------+------+--------+-------+------+--------+---------+------+
//    | SYNC | VER  | SRC  | DST  | TIPO   | FLAGS | SEQ  | LEN    | PAYLOAD | CRC16|
//    | 0xA5 | 0x01 | 1 B  | 1 B  | 1 B    | 1 B   | 1 B  | 1 B    | 0..40 B | 2 B  |
//    +------+------+------+------+--------+-------+------+--------+---------+------+
//    \___________________ coberto pelo CRC16 ___________________/
//
//  - SYNC permite ao receptor ressincronizar no fluxo de bytes do E220.
//  - SEQ identifica a transacao para casar o ACK correspondente.
//  - FLAGS: bit0 = pede ACK, bit1 = este quadro E um ACK.
//  - LEN e o tamanho do PAYLOAD (o CRC nao entra em LEN).
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace protocol {

constexpr uint8_t kSyncByte      = 0xA5;
constexpr uint8_t kProtocolVer   = 0x01;
constexpr size_t  kHeaderSize    = 8;   // SYNC..LEN
constexpr size_t  kCrcSize       = 2;
constexpr size_t  kMaxPayload    = 40;
constexpr size_t  kMaxPacketSize = kHeaderSize + kMaxPayload + kCrcSize;

// Tipos de mensagem trafegados entre os nos.
enum class MessageType : uint8_t {
    kStatusNivel    = 0x01,  // reservatorio -> (informativo) estado do nivel
    kComandoBomba   = 0x02,  // reservatorio -> bomba: ligar/desligar
    kAck            = 0x03,  // confirmacao de um SEQ recebido
    kHeartbeat      = 0x04,  // mantem o enlace vivo / detecta perda
    kBateriaStatus  = 0x05,  // telemetria de energia
};

// Flags do cabecalho.
enum PacketFlags : uint8_t {
    kFlagAckRequested = 0x01,
    kFlagIsAck        = 0x02,
};

// Representacao decodificada de um pacote (nao e o layout de fio).
struct Packet {
    uint8_t  src     = 0;
    uint8_t  dst     = 0;
    MessageType type = MessageType::kHeartbeat;
    uint8_t  flags   = 0;
    uint8_t  seq     = 0;
    uint8_t  payload[kMaxPayload] = {0};
    uint8_t  payloadLen = 0;

    bool wantsAck() const { return flags & kFlagAckRequested; }
    bool isAck() const    { return flags & kFlagIsAck; }
};

// Serializa `pkt` em `out` (deve ter ao menos kMaxPacketSize bytes). Retorna o
// numero total de bytes escritos, ou 0 se o payload for grande demais.
size_t encodePacket(const Packet& pkt, uint8_t* out, size_t outSize);

// Tenta decodificar um pacote a partir de `in`. Valida SYNC, versao, tamanho e
// CRC. Retorna true e preenche `pkt` em caso de sucesso.
bool decodePacket(const uint8_t* in, size_t inLength, Packet& pkt);

}  // namespace protocol
