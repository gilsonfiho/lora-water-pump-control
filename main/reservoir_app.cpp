// ============================================================================
//  reservoir_app.cpp  -  No do RESERVATORIO (caixa d'agua) / TRANSMISSOR
// ----------------------------------------------------------------------------
//  Arquitetura FreeRTOS:
//    * Uma FILA de eventos e alimentada por:
//        - ISR da chave/boia (mudanca de nivel)  -> evento kLevel
//        - timer periodico de ciclo              -> evento kCycle  (reenvio)
//        - timer de heartbeat                    -> evento kHeartbeat
//        - timer de bateria                      -> evento kBattery
//    * A tarefa de controle (este app_main task) serializa tudo: le a fila e
//      chama o controlador, que envia comandos de forma confiavel (ACK/retry).
//
//  PROTOTIPO DE BANCADA: no lugar da boia entra uma CHAVE manual e, no lugar do
//  ADC de bateria, um stub fixo. Para o hardware de campo, troque as duas linhas
//  marcadas com BANCADA (e o pino da ISR) pela boia e pelo AdcBatteryMonitor.
// ============================================================================

#include <cstdint>

#include <driver/gpio.h>
#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_attr.h>
#include <esp_log.h>
#include <esp_timer.h>

#include "config/NodeConfig.h"
#include "config/PinConfig.h"
#include "config/RadioConfig.h"
#include "core/ReservoirController.h"
#include "hw/BenchSwitchLevelSensor.h"
#include "hw/E220Radio.h"
#include "hw/StubBatteryMonitor.h"
#include "node_app.h"
#include "power/PowerManager.h"
#include "protocol/LinkLayer.h"

namespace app {

namespace {

constexpr char kTag[] = "reservatorio";

enum class Event : uint8_t { kLevel, kCycle, kHeartbeat, kBattery };

QueueHandle_t g_events = nullptr;
core::ReservoirController* g_controller = nullptr;
power::PowerManager*       g_power = nullptr;

const char* levelName(hw::LevelState s) {
    switch (s) {
        case hw::LevelState::kLow:  return "BAIXA (pedindo agua)";
        case hw::LevelState::kMid:  return "MEIO (mantem)";
        case hw::LevelState::kHigh: return "CHEIA (para)";
        case hw::LevelState::kUnknown:
        default:                    return "DESCONHECIDA (seguro: desliga)";
    }
}

cfg::LoraProfile makeProfile() {
    cfg::LoraProfile p;
    p.variant = cfg::E220Variant::kT22D;  // TODO(hw): confirmar o par do prototipo
    p.airRate = cfg::LoraAirRate::k2_4k;   // maior alcance
    return p;
}

// ---- Fontes de eventos ----------------------------------------------------
void IRAM_ATTR level_isr(void*) {
    const Event e = Event::kLevel;
    BaseType_t hpTaskWoken = pdFALSE;
    xQueueSendFromISR(g_events, &e, &hpTaskWoken);
    if (hpTaskWoken) portYIELD_FROM_ISR();
}

void post_event(void* arg) {
    const Event e = static_cast<Event>(reinterpret_cast<intptr_t>(arg));
    xQueueSend(g_events, &e, 0);
}

void start_periodic(const char* name, uint32_t periodMs, Event ev) {
    esp_timer_create_args_t args = {};
    args.callback = &post_event;
    args.arg = reinterpret_cast<void*>(static_cast<intptr_t>(ev));
    args.dispatch_method = ESP_TIMER_TASK;
    args.name = name;
    esp_timer_handle_t handle = nullptr;
    esp_timer_create(&args, &handle);
    esp_timer_start_periodic(handle, static_cast<uint64_t>(periodMs) * 1000ULL);
}

}  // namespace

[[noreturn]] void runReservoir() {
    ESP_LOGI(kTag, "iniciando...");

    static hw::E220Radio radio(UART_NUM_1, makeProfile());

    // BANCADA: trocar por AdcBatteryMonitor battery(2.0f) com o subsistema real.
    static hw::StubBatteryMonitor battery;
    // BANCADA: trocar por FloatSwitchLevelSensor sensor(true) com as boias.
    static hw::BenchSwitchLevelSensor sensor(/*closedIsLow=*/true);

    static protocol::LinkLayer       link(radio, cfg::kAddrReservoir);
    static core::ReservoirController controller(link, sensor, battery, cfg::kAddrPump);
    static power::PowerManager       powerMgr(battery);

    g_controller = &controller;
    g_power = &powerMgr;

    controller.begin();
    powerMgr.begin();
    if (!radio.begin()) ESP_LOGE(kTag, "FALHA ao iniciar o radio E220");
    if (!link.begin())  ESP_LOGE(kTag, "FALHA ao subir a camada de enlace");

    controller.onLevelChanged().subscribe([](const core::LevelChangedEvent& e) {
        ESP_LOGI(kTag, "chave: %s -> %s", levelName(e.previous), levelName(e.current));
    });

    g_events = xQueueCreate(8, sizeof(Event));

    // ISR na chave de nivel (bancada) para reagir na hora a mudancas.
    gpio_install_isr_service(0);
    gpio_set_intr_type(static_cast<gpio_num_t>(cfg::BenchPins::kLevelSwitch),
                       GPIO_INTR_ANYEDGE);
    gpio_isr_handler_add(static_cast<gpio_num_t>(cfg::BenchPins::kLevelSwitch),
                         level_isr, nullptr);

    start_periodic("cycle", cfg::Timing::kReservoirCyclePeriodMs, Event::kCycle);
    start_periodic("hb", cfg::Timing::kHeartbeatPeriodMs, Event::kHeartbeat);
    start_periodic("bat", cfg::Timing::kBatteryReportPeriodMs, Event::kBattery);

    ESP_LOGI(kTag, "pronto (bancada: chave no GPIO %d, ciclo de %lu ms)",
             cfg::BenchPins::kLevelSwitch,
             static_cast<unsigned long>(cfg::Timing::kReservoirCyclePeriodMs));

    // Avaliacao imediata no boot.
    const Event first = Event::kCycle;
    xQueueSend(g_events, &first, 0);

    for (;;) {
        Event e;
        if (xQueueReceive(g_events, &e, portMAX_DELAY) != pdTRUE) continue;
        switch (e) {
            case Event::kLevel:
                vTaskDelay(pdMS_TO_TICKS(50));  // debounce da chave
                controller.evaluateAndSend();
                break;
            case Event::kCycle:
                controller.evaluateAndSend();
                break;
            case Event::kHeartbeat:
                controller.sendHeartbeat();
                break;
            case Event::kBattery:
                powerMgr.update();
                controller.reportBattery();
                break;
        }
    }
}

}  // namespace app
