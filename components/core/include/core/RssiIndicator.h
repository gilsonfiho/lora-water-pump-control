#pragma once
// ============================================================================
//  RssiIndicator.h  -  Pisca um LED conforme o RSSI do ultimo pacote valido.
// ----------------------------------------------------------------------------
//  Nao bloqueante: onValidPacket() grava o RSSI, poll() decide o estado do LED.
//  Faixas e tempos vem de cfg::Indicator. Sem pacote por kNoSignalTimeoutMs o
//  LED apaga.
// ============================================================================

#include <cstdint>

#include "config/NodeConfig.h"
#include "hw/ISignalIndicator.h"

namespace core {

class RssiIndicator {
public:
    explicit RssiIndicator(hw::ISignalIndicator& led) : led_(led) {}

    // Periodo de pisca para um RSSI (dBm). Separado para ser testavel.
    static uint32_t blinkPeriodMs(int16_t rssiDbm);

    // Chamar so para pacote que passou por CRC, MAC e anti-replay.
    void onValidPacket(int16_t rssiDbm);

    // Chamar a cada volta do laco principal.
    void poll();

private:
    hw::ISignalIndicator& led_;
    bool     hasSignal_   = false;
    uint32_t periodMs_    = cfg::Indicator::kWeakPeriodMs;
    uint32_t lastRxMs_    = 0;
    uint32_t periodStart_ = 0;
    bool     ledOn_       = false;
};

}  // namespace core
