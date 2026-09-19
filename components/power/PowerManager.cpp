// ============================================================================
//  PowerManager.cpp  -  Implementacao do deep sleep no ESP32-C3.
// ============================================================================

#include "power/PowerManager.h"

#include <esp_sleep.h>

namespace power {

void PowerManager::deepSleepFor(uint32_t ms) {
    if (ms == 0) return;  // sleep desabilitado

    // Converte para microssegundos e arma o timer de wake-up.
    esp_sleep_enable_timer_wakeup(static_cast<uint64_t>(ms) * 1000ULL);
    // TODO(hw): se quiser acordar por evento externo (ex.: boia mudou de
    // estado), armar tambem esp_deep_sleep_enable_gpio_wakeup aqui.
    esp_deep_sleep_start();  // nao retorna
}

}  // namespace power
