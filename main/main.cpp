// ============================================================================
//  main.cpp  -  Entrada do firmware: le o strap e assume o papel do no.
// ----------------------------------------------------------------------------
//  Um unico binario roda nos dois nos. O pino cfg::RolePins::kRoleStrap, com
//  pull-up interno, decide o papel no boot:
//
//      aberto (nivel 1) -> RESERVATORIO   (transmissor, le a boia)
//      ao GND (nivel 0) -> BOMBA          (receptor, aciona o rele)
//
//  O aberto cair no reservatorio e deliberado: e o papel que nao mexe no rele,
//  entao um strap solto nunca faz um no assumir o comando da bomba.
// ============================================================================

#include <driver/gpio.h>
#include <esp_log.h>

#include "config/PinConfig.h"
#include "node_app.h"
#include "platform/Clock.h"

namespace {

constexpr char kTag[] = "main";

enum class Role { kReservoir, kPump };

Role readRoleStrap() {
    const gpio_num_t pin = static_cast<gpio_num_t>(cfg::RolePins::kRoleStrap);

    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << cfg::RolePins::kRoleStrap);
    io.mode         = GPIO_MODE_INPUT;
    io.pull_up_en   = GPIO_PULLUP_ENABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type    = GPIO_INTR_DISABLE;
    if (gpio_config(&io) != ESP_OK) {
        ESP_LOGE(kTag, "falha ao configurar o strap; assumindo reservatorio");
        return Role::kReservoir;
    }

    // O pull-up interno leva alguns microssegundos para estabilizar a linha
    // depois do reset. Esperamos 1 ms e lemos tres vezes para nao decidir o
    // papel do no com base em uma unica leitura ruidosa.
    platform::delayMs(1);
    int low = 0;
    for (int i = 0; i < 3; ++i) {
        if (gpio_get_level(pin) == 0) ++low;
        platform::delayMs(1);
    }
    return (low >= 2) ? Role::kPump : Role::kReservoir;
}

}  // namespace

extern "C" void app_main() {
    const Role role = readRoleStrap();

    if (role == Role::kPump) {
        ESP_LOGI(kTag, "strap em GND -> papel BOMBA");
        app::runPump();
    }

    ESP_LOGI(kTag, "strap aberto -> papel RESERVATORIO");
    app::runReservoir();
}
