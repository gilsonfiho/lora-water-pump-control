#pragma once
// ============================================================================
//  node_app.h  -  Ponto de entrada de cada papel + log de diagnostico do enlace.
// ----------------------------------------------------------------------------
//  Cada funcao monta os objetos do seu no e entra no laco principal. Nenhuma
//  delas retorna.
// ============================================================================

#include <esp_log.h>

#include "protocol/LinkLayer.h"

namespace app {

[[noreturn]] void runReservoir();
[[noreturn]] void runPump();

// Loga um evento de diagnostico do enlace. Vive na camada de app (pode usar
// ESP_LOG); protocol/ apenas emite o LinkEvent via std::function, sem conhecer
// o ESP-IDF.
inline void logLinkEvent(const char* tag, const protocol::LinkEvent& e) {
    using T = protocol::LinkEvent::Type;
    switch (e.type) {
        case T::kTx:
            ESP_LOGI(tag, "TX cmd seq=%u dst=0x%02x (%u B)",
                     static_cast<unsigned>(e.seq), static_cast<unsigned>(e.dst),
                     static_cast<unsigned>(e.bytes));
            break;
        case T::kAckOk:
            ESP_LOGI(tag, "ACK seq=%u (rtt %lu ms)", static_cast<unsigned>(e.seq),
                     static_cast<unsigned long>(e.rttMs));
            break;
        case T::kRetry:
            ESP_LOGW(tag, "RETRY seq=%u (restam %u)", static_cast<unsigned>(e.seq),
                     static_cast<unsigned>(e.retriesLeft));
            break;
        case T::kTimeout:
            ESP_LOGE(tag, "TIMEOUT seq=%u -- entrega falhou",
                     static_cast<unsigned>(e.seq));
            break;
        case T::kCrcFail:
            ESP_LOGW(tag, "RX com SYNC mas CRC/quadro invalido (satura/colisao?)");
            break;
        case T::kNotForMe:
            ESP_LOGD(tag, "RX de outro no (dst=0x%02x)", static_cast<unsigned>(e.dst));
            break;
    }
}

}  // namespace app
