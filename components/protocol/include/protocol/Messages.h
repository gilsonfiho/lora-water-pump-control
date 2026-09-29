#pragma once
// ============================================================================
//  Messages.h  -  Payloads tipados de cada MessageType e seus (de)serializers.
// ----------------------------------------------------------------------------
//  Mantem o conhecimento do layout de cada payload em um so lugar, separado do
//  cabecalho generico do pacote (Packet.h). Cada builder preenche um Packet
//  pronto para envio; cada parser extrai a struct correspondente.
// ============================================================================

#include <cstdint>

#include "protocol/Packet.h"
#include "hw/ILevelSensor.h"
#include "hw/IBatteryMonitor.h"

namespace protocol {

// ---- Comando de bomba -----------------------------------------------------
enum class PumpCommand : uint8_t {
    kOff = 0,
    kOn  = 1,
};

// ---- STATUS_NIVEL ---------------------------------------------------------
struct LevelStatusPayload {
    hw::LevelState level = hw::LevelState::kUnknown;
    uint8_t percent = 0;      // 0 se desconhecido
    bool percentValid = false;
};

// ---- BATERIA_STATUS -------------------------------------------------------
// Reaproveita hw::BatteryTelemetry como conteudo logico.

// ===== Builders (preenchem um Packet) ======================================

inline Packet makeCommand(uint8_t src, uint8_t dst, uint8_t seq, PumpCommand cmd) {
    Packet p;
    p.src = src; p.dst = dst; p.seq = seq;
    p.type = MessageType::kComandoBomba;
    p.flags = kFlagAckRequested;             // comando critico -> exige ACK
    p.payload[0] = static_cast<uint8_t>(cmd);
    p.payloadLen = 1;
    return p;
}

inline Packet makeAck(uint8_t src, uint8_t dst, uint8_t seqToAck) {
    Packet p;
    p.src = src; p.dst = dst; p.seq = seqToAck;
    p.type = MessageType::kAck;
    p.flags = kFlagIsAck;
    p.payloadLen = 0;
    return p;
}

inline Packet makeHeartbeat(uint8_t src, uint8_t dst, uint8_t seq) {
    Packet p;
    p.src = src; p.dst = dst; p.seq = seq;
    p.type = MessageType::kHeartbeat;
    p.payloadLen = 0;
    return p;
}

inline Packet makeLevelStatus(uint8_t src, uint8_t dst, uint8_t seq,
                              const LevelStatusPayload& s) {
    Packet p;
    p.src = src; p.dst = dst; p.seq = seq;
    p.type = MessageType::kStatusNivel;
    p.payload[0] = static_cast<uint8_t>(s.level);
    p.payload[1] = s.percentValid ? s.percent : 0xFF;  // 0xFF = invalido
    p.payloadLen = 2;
    return p;
}

inline Packet makeBatteryStatus(uint8_t src, uint8_t dst, uint8_t seq,
                                const hw::BatteryTelemetry& t) {
    Packet p;
    p.src = src; p.dst = dst; p.seq = seq;
    p.type = MessageType::kBateriaStatus;
    p.payload[0] = static_cast<uint8_t>(t.milliVolts >> 8);
    p.payload[1] = static_cast<uint8_t>(t.milliVolts & 0xFF);
    p.payload[2] = static_cast<uint8_t>(t.milliAmps >> 8);
    p.payload[3] = static_cast<uint8_t>(t.milliAmps & 0xFF);
    p.payload[4] = t.socPercent;
    p.payload[5] = static_cast<uint8_t>(t.source);
    p.payloadLen = 6;
    return p;
}

// ===== Parsers =============================================================

inline bool parseCommand(const Packet& p, PumpCommand& cmdOut) {
    if (p.type != MessageType::kComandoBomba || p.payloadLen < 1) return false;
    cmdOut = static_cast<PumpCommand>(p.payload[0]);
    return true;
}

inline bool parseLevelStatus(const Packet& p, LevelStatusPayload& out) {
    if (p.type != MessageType::kStatusNivel || p.payloadLen < 2) return false;
    out.level = static_cast<hw::LevelState>(p.payload[0]);
    out.percentValid = (p.payload[1] != 0xFF);
    out.percent = out.percentValid ? p.payload[1] : 0;
    return true;
}

inline bool parseBatteryStatus(const Packet& p, hw::BatteryTelemetry& out) {
    if (p.type != MessageType::kBateriaStatus || p.payloadLen < 6) return false;
    out.milliVolts = static_cast<uint16_t>(p.payload[0] << 8) | p.payload[1];
    out.milliAmps  = static_cast<int16_t>((p.payload[2] << 8) | p.payload[3]);
    out.socPercent = p.payload[4];
    out.source     = static_cast<hw::PowerSource>(p.payload[5]);
    out.currentValid = true;
    return true;
}

}  // namespace protocol
