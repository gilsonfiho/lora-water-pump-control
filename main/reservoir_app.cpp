// ============================================================================
//  reservoir_app.cpp  -  No do RESERVATORIO (caixa d'agua) / TRANSMISSOR
// ----------------------------------------------------------------------------
//  Le o nivel, decide encher/parar com histerese e comanda o no da bomba via
//  LoRa com ACK/retry. Reporta bateria e envia heartbeat.
//
//  PROTOTIPO DE BANCADA: no lugar da boia entra uma CHAVE manual e, no lugar do
//  ADC de bateria, um stub fixo. A logica de controle, o protocolo e o no da
//  bomba nao sabem da diferenca -- so as implementacoes de ILevelSensor e
//  IBatteryMonitor mudam. Para voltar ao hardware de campo, troque as duas
//  linhas marcadas com BANCADA abaixo.
//
//  Sensor de campo: BOIA (FloatSwitchLevelSensor). O ultrassonico e OPCIONAL e
//  so entra se USE_ULTRASONIC_SENSOR for definido.
// ============================================================================

#include <driver/uart.h>
#include <esp_log.h>

#include "config/NodeConfig.h"
#include "config/PinConfig.h"
#include "config/RadioConfig.h"
#include "core/ReservoirController.h"
#include "hw/BenchSwitchLevelSensor.h"
#include "hw/E220Radio.h"
#include "hw/ILevelSensor.h"
#include "hw/StubBatteryMonitor.h"
#include "node_app.h"
#include "platform/Clock.h"
#include "power/PowerManager.h"
#include "protocol/LinkLayer.h"

namespace app {

namespace {

constexpr char kTag[] = "reservatorio";

// Mesma cadencia do no da bomba; ver a nota em pump_app.cpp.
constexpr uint32_t kLoopPeriodMs = 5;

// Log legivel: na bancada o que importa e ler o estado de relance.
const char* levelName(hw::LevelState s) {
    switch (s) {
        case hw::LevelState::kLow:  return "BAIXA (pedindo agua)";
        case hw::LevelState::kMid:  return "MEIO (mantem)";
        case hw::LevelState::kHigh: return "CHEIA (para)";
        case hw::LevelState::kUnknown:
        default:                     return "DESCONHECIDA (seguro: desliga)";
    }
}

cfg::LoraProfile makeProfile() {
    cfg::LoraProfile p;
    p.variant = cfg::E220Variant::kT30D;  // par do prototipo (T22D: comparacao em campo)
    p.airRate = cfg::LoraAirRate::k2_4k;   // maior alcance
    return p;
}

}  // namespace

[[noreturn]] void runReservoir() {
    ESP_LOGI(kTag, "iniciando...");

    static hw::E220Radio radio(UART_NUM_1, makeProfile());

    // BANCADA: trocar por AdcBatteryMonitor battery(2.0f) quando o subsistema
    // de energia estiver montado.
    static hw::StubBatteryMonitor battery;

    // BANCADA: trocar por FloatSwitchLevelSensor sensor(true) quando as boias
    // estiverem instaladas.
    static hw::BenchSwitchLevelSensor sensor(/*closedIsLow=*/true);

    static protocol::LinkLayer       link(radio, cfg::kAddrReservoir);
    static core::ReservoirController controller(link, sensor, battery, cfg::kAddrPump);
    static power::PowerManager       powerMgr(battery);

    if (!radio.begin()) {
        ESP_LOGE(kTag, "FALHA ao iniciar o radio E220");
        // Sem radio nao ha o que fazer com seguranca; segue tentando no laco.
    }

    powerMgr.begin();
    controller.begin();

    // Observer: exemplo de reacao a mudanca de nivel (log). Substitua/estenda
    // por LED, display, etc., sem tocar na logica de controle.
    controller.onLevelChanged().subscribe([](const core::LevelChangedEvent& e) {
        ESP_LOGI(kTag, "chave: %s -> %s",
                 levelName(e.previous), levelName(e.current));
    });

    ESP_LOGI(kTag, "pronto (bancada: chave no GPIO %d, ciclo de %lu ms)",
             cfg::BenchPins::kLevelSwitch,
             static_cast<unsigned long>(cfg::Timing::kReservoirCyclePeriodMs));

    while (true) {
        controller.loop();
        powerMgr.update();

        // Politica de sleep (desabilitada por padrao). Em bateria e sem envio
        // pendente, dorme kDeepSleepMs entre ciclos. Ver SleepPolicy.
        if (cfg::SleepPolicy::kDeepSleepMs > 0 && powerMgr.onBattery() &&
            !link.isDelivering()) {
            radio.sleep();
            powerMgr.deepSleepFor(cfg::SleepPolicy::kDeepSleepMs);  // reinicia ao acordar
        }

        platform::delayMs(kLoopPeriodMs);
    }
}

}  // namespace app
