#pragma once
// ============================================================================
//  Crc16.h  -  CRC-16/CCITT-FALSE para integridade dos pacotes.
// ----------------------------------------------------------------------------
//  Polinomio 0x1021, valor inicial 0xFFFF, sem reflexao. Barato e adequado
//  para os quadros curtos deste protocolo. Implementacao bit-a-bit (sem tabela)
//  para poupar RAM no ESP32-C3.
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace protocol {

inline uint16_t crc16Ccitt(const uint8_t* data, size_t length) {
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < length; ++i) {
        crc ^= static_cast<uint16_t>(data[i]) << 8;
        for (uint8_t bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<uint16_t>(crc << 1);
        }
    }
    return crc;
}

}  // namespace protocol
