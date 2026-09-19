// ============================================================================
//  pump_app.cpp  -  No da BOMBA (captacao) / RECEPTOR / ATUADOR
// ----------------------------------------------------------------------------
//  Arquitetura FreeRTOS:
//    * A task de RX do LinkLayer recebe/decodifica quadros e auto-ACKa.
//    * A tarefa de controle (este app_main task) consome a fila de pacotes do
//      LinkLayer; o TIMEOUT da fila serve de "batida" para checar o failsafe
//      (perda de enlace -> bomba OFF).
// ============================================================================

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <esp_log.h>

#include "config/NodeConfig.h"
#include "config/PinConfig.h"
#include "config/RadioConfig.h"
#include "core/PumpController.h"
#include "hw/E220Radio.h"
#include "hw/RelayPumpActuator.h"
#include "node_app.h"
#include "protocol/LinkLayer.h"

namespace app {

namespace {

constexpr char kTag[] = "bomba";
constexpr uint32_t kFailsafeTickMs = 1000;  // periodicidade de checagem

const char* stateName(core::PumpController::State s) {
    switch (s) {
        case core::PumpController::State::kBoot:     return "BOOT";
        case core::PumpController::State::kIdle:     return "OCIOSO";
        case core::PumpController::State::kPumping:  return "BOMBEANDO";
        case core::PumpController::State::kLinkLost: return "ENLACE PERDIDO (failsafe)";
        default:                                     return "?";
    }
}

cfg::LoraProfile makeProfile() {
    cfg::LoraProfile p;
    p.variant = cfg::E220Variant::kT22D;  // TODO(hw): confirmar o par do prototipo
    p.airRate = cfg::LoraAirRate::k2_4k;   // DEVE casar com o reservatorio
    return p;
}

}  // namespace

[[noreturn]] void runPump() {
    ESP_LOGI(kTag, "iniciando...");

    static hw::E220Radio         radio(UART_NUM_1, makeProfile());
    static hw::RelayPumpActuator pump;
    static protocol::LinkLayer   link(radio, cfg::kAddrPump);
    static core::PumpController  controller(pump);

    // Estado seguro primeiro: o controlador forca a bomba DESLIGADA.
    controller.begin();

    if (!radio.begin()) {
        ESP_LOGE(kTag, "FALHA ao iniciar o radio E220; failsafe mantem OFF");
    }
    if (!link.begin()) {
        ESP_LOGE(kTag, "FALHA ao subir a camada de enlace");
    }

    ESP_LOGI(kTag, "pronto (bancada: LED no GPIO %d, failsafe em %lu ms)",
             cfg::PumpPins::kRelay,
             static_cast<unsigned long>(cfg::Timing::kLinkLostTimeoutMs));

    QueueHandle_t q = link.dataQueue();
    core::PumpController::State last = core::PumpController::State::kBoot;
    for (;;) {
        protocol::Packet pkt;
        if (xQueueReceive(q, &pkt, pdMS_TO_TICKS(kFailsafeTickMs)) == pdTRUE) {
            controller.handlePacket(pkt);
        }
        controller.checkFailsafe();  // roda mesmo sem pacotes

        if (controller.state() != last) {
            last = controller.state();
            ESP_LOGI(kTag, "estado -> %s (saida=%s)",
                     stateName(last), pump.isOn() ? "LIGADA" : "DESLIGADA");
        }
    }
}

}  // namespace app
