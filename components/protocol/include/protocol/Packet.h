#pragma once
// ============================================================================
//  Packet.h  -  Formato de pacote da camada de aplicacao/enlace do projeto.
// ----------------------------------------------------------------------------
//  Layout do quadro no ar (campos multi-byte em big-endian):
//
//   +----+----+----+----+-----+-----+----+----+---------+---------+------+-----+
//   |SYNC|VER |SRC |DST |TIPO |FLAGS|SEQ |LEN |COUNTER  | PAYLOAD | MAC  |CRC16|
//   |0xA5|0x02|1 B |1 B |1 B  |1 B  |1 B |1 B | 4 B     | 0..40 B | 8 B  | 2 B |
//   +----+----+----+----+-----+-----+----+----+---------+---------+------+-----+
//   \__________________ autenticado pelo MAC (SipHash) ___________/
//   \_______________________ coberto pelo CRC16 (framing) ________________/
//
//  - SYNC: ressincroniza o receptor no fluxo de bytes do E220.
//  - SEQ: casa o ACK com a transacao (1 byte, nao e anti-replay).
//  - COUNTER: contador monotonico de 32 bits -- frescor contra replay (#7).
//  - MAC: SipHash-2-4 de SYNC..PAYLOAD sob a chave pre-compartilhada -- prova
//    autenticidade/integridade do pacote (#5, #8). Sem a chave nao se forja.
//  - CRC16: integridade de framing (rejeita lixo antes de checar o MAC).
//
//  O formato subiu para a VERSAO 2 (contador + MAC). Nos em v1 nao decodificam
//  os v2 e vice-versa: regravar os dois nos juntos.
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace protocol {

constexpr uint8_t kSyncByte      = 0xA5;
constexpr uint8_t kProtocolVer   = 0x02;        // v2: contador + MAC
constexpr size_t  kHeaderSize    = 12;          // SYNC..COUNTER
constexpr size_t  kMacSize       = 8;           // SipHash-64
constexpr size_t  kCrcSize       = 2;
constexpr size_t  kMaxPayload    = 40;
constexpr size_t  kMaxPacketSize = kHeaderSize + kMaxPayload + kMacSize + kCrcSize;
constexpr size_t  kKeySize       = 16;          // chave do MAC (128 bits)

// Tipos de mensagem trafegados entre os nos.
enum class MessageType : uint8_t {
    kStatusNivel    = 0x01,  // reservatorio -> (informativo) estado do nivel
    kComandoBomba   = 0x02,  // reservatorio -> bomba: ligar/desligar
    kAck            = 0x03,  // confirmacao de um SEQ recebido
    kHeartbeat      = 0x04,  // mantem o enlace vivo / detecta perda
    kBateriaStatus  = 0x05,  // telemetria de energia
    kBombaStatus    = 0x06,  // bomba -> reservatorio: estado do atuador
};

// Flags do cabecalho.
enum PacketFlags : uint8_t {
    kFlagAckRequested = 0x01,
    kFlagIsAck        = 0x02,
};

// Representacao decodificada de um pacote (nao e o layout de fio).
struct Packet {
    uint8_t     src     = 0;
    uint8_t     dst     = 0;
    MessageType type    = MessageType::kHeartbeat;
    uint8_t     flags   = 0;
    uint8_t     seq     = 0;
    uint32_t    counter = 0;   // nonce anti-replay (atribuido no transmit)
    uint8_t     payload[kMaxPayload] = {0};
    uint8_t     payloadLen = 0;

    bool wantsAck() const { return flags & kFlagAckRequested; }
    bool isAck() const    { return flags & kFlagIsAck; }
};

// Motivo de uma decodificacao falhar -- deixa o chamador distinguir lixo de
// framing (kCrc) de adulteracao/chave errada (kMac), para log e metrica.
enum class DecodeError : uint8_t { kNone, kFraming, kCrc, kMac };

// Serializa `pkt` em `out` (>= kMaxPacketSize), assinando com `key` (16 B).
// Retorna o total de bytes escritos, ou 0 se o payload nao couber.
size_t encodePacket(const Packet& pkt, const uint8_t key[kKeySize],
                    uint8_t* out, size_t outSize);

// Decodifica de `in`, validando SYNC, versao, tamanho, CRC e MAC (sob `key`).
// Preenche `pkt` e retorna true em sucesso; em falha, opcionalmente informa o
// motivo em `*err`.
bool decodePacket(const uint8_t* in, size_t inLength, const uint8_t key[kKeySize],
                  Packet& pkt, DecodeError* err = nullptr);

}  // namespace protocol
