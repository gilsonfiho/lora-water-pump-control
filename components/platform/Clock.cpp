// ============================================================================
//  Clock.cpp  -  Implementacao ESP-IDF do relogio (esp_timer + FreeRTOS).
// ============================================================================

#include "platform/Clock.h"

#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace platform {

uint32_t millis() {
    // esp_timer_get_time() e o tempo em microssegundos desde o boot (int64).
    // O truncamento para 32 bits e intencional: ver a nota no header.
    return static_cast<uint32_t>(esp_timer_get_time() / 1000);
}

void delayMs(uint32_t ms) {
    // pdMS_TO_TICKS arredonda para baixo; com tick de 1 kHz (padrao do IDF)
    // qualquer ms > 0 vira ao menos 1 tick. Garantimos isso explicitamente
    // para que delayMs(1) nunca vire um busy-loop.
    TickType_t ticks = pdMS_TO_TICKS(ms);
    if (ticks == 0 && ms > 0) {
        ticks = 1;
    }
    vTaskDelay(ticks);
}

}  // namespace platform
