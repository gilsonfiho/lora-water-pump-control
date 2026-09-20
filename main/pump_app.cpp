// ============================================================================
//  pump_app.cpp  -  No da BOMBA (captacao) / RECEPTOR / ATUADOR
// ----------------------------------------------------------------------------
//  Recebe comandos do reservatorio via LoRa, aciona o rele/contator e aplica o
//  FAILSAFE: se o enlace ficar em silencio por mais de kLinkLostTimeoutMs, a
//  bomba e DESLIGADA por seguranca ate o enlace voltar.
// ============================================================================

#include <driver/uart.h>
#include <esp_log.h>

#include "config/NodeConfig.h"
#include "config/PinConfig.h"
#include "config/RadioConfig.h"
#include "core/PumpController.h"
#include "hw/E220Radio.h"
#include "hw/RelayPumpActuator.h"
#include "node_app.h"
#include "platform/Clock.h"
#include "protocol/LinkLayer.h"

namespace app {

namespace {

constexpr char kTag[] = "bomba";

// Cadencia do laco principal. Precisa ser bem menor que a folga de 12 ms que
// fecha um quadro no E220, e precisa ceder o processador para nao disparar o
// watchdog de tarefa.
constexpr uint32_t kLoopPeriodMs = 5;

// Log legivel: na bancada o que importa e ler o estado de relance.
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
    p.variant = cfg::E220Variant::kT30D;  // par do prototipo (T22D: comparacao em campo)
    p.airRate = cfg::LoraAirRate::k2_4k;   // DEVE casar com o reservatorio
    return p;
}

}  // namespace

[[noreturn]] void runPump() {
    ESP_LOGI(kTag, "iniciando...");

    static hw::E220Radio          radio(UART_NUM_1, makeProfile());
    static hw::RelayPumpActuator  pump;
    static protocol::LinkLayer     link(radio, cfg::kAddrPump);
    static core::PumpController    controller(link, pump);

    // Estado seguro primeiro: o controlador ja forca a bomba DESLIGADA.
    controller.begin();

    if (!radio.begin()) {
        ESP_LOGE(kTag, "FALHA ao iniciar o radio E220");
        // Sem radio, o failsafe manda: a bomba permanece desligada.
    }

    // Diagnostico do enlace: RX de outro no, CRC invalido, etc. no monitor.
    link.onEvent([](const protocol::LinkEvent& e) { logLinkEvent(kTag, e); });

    ESP_LOGI(kTag, "pronto (bancada: LED no GPIO %d, failsafe em %lu ms)",
             cfg::PumpPins::kRelay,
             static_cast<unsigned long>(cfg::Timing::kLinkLostTimeoutMs));

    core::PumpController::State last = core::PumpController::State::kBoot;
    while (true) {
        controller.loop();

        // Log leve de transicao de estado.
        if (controller.state() != last) {
            last = controller.state();
            ESP_LOGI(kTag, "estado -> %s (saida=%s)",
                     stateName(last), pump.isOn() ? "LIGADA" : "DESLIGADA");
        }

        platform::delayMs(kLoopPeriodMs);
    }
}

}  // namespace app
