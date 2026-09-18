// ============================================================================
//  main.cpp  -  No do RESERVATORIO (caixa d'agua) / TRANSMISSOR
// ----------------------------------------------------------------------------
//  Le o nivel (boia por padrao), decide encher/parar com histerese e comanda
//  o no da bomba via LoRa com ACK/retry. Reporta bateria e envia heartbeat.
//
//  Sensor: BOIA (FloatSwitchLevelSensor) e o padrao do projeto. O sensor
//  ultrassonico e OPCIONAL e so entra se USE_ULTRASONIC_SENSOR for definido
//  (ver platformio.ini).
// ============================================================================

#include <Arduino.h>

#include "config/NodeConfig.h"
#include "config/RadioConfig.h"
#include "core/ReservoirController.h"
#include "hal/AdcBatteryMonitor.h"
#include "hal/E220Radio.h"
#include "protocol/LinkLayer.h"
#include "power/PowerManager.h"

#ifdef USE_ULTRASONIC_SENSOR
#include "hal/UltrasonicLevelSensor.h"
#else
#include "hal/FloatSwitchLevelSensor.h"
#endif

namespace {

// ---- Perfil de RF deste no (ajuste a variante conforme o modulo instalado) --
cfg::LoraProfile makeProfile() {
    cfg::LoraProfile p;
    p.variant = cfg::E220Variant::kT30D;  // TODO(hw): T30D ou T22D
    p.power   = cfg::LoraPowerLevel::kMax;
    p.airRate = cfg::LoraAirRate::k2_4k;   // maior alcance (5 km+)
    return p;
}

// ---- Objetos (HAL -> protocolo -> core) ------------------------------------
hal::E220Radio          radio(Serial1, makeProfile());
hal::AdcBatteryMonitor  battery(/*dividerRatio=*/2.0f);

#ifdef USE_ULTRASONIC_SENSOR
hal::UltrasonicLevelSensor::Geometry kGeom;  // ajuste as medidas da caixa
hal::UltrasonicLevelSensor sensor(kGeom);
#else
hal::FloatSwitchLevelSensor sensor(/*closedIsLow=*/true);
#endif

protocol::LinkLayer         link(radio, cfg::kAddrReservoir);
core::ReservoirController   controller(link, sensor, battery, cfg::kAddrPump);
power::PowerManager         powerMgr(battery);

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println(F("[reservatorio] iniciando..."));

    if (!radio.begin()) {
        Serial.println(F("[reservatorio] FALHA ao iniciar o radio E220"));
        // Sem radio nao ha o que fazer com seguranca; segue tentando no loop.
    }

    powerMgr.begin();
    controller.begin();

    // Observer: exemplo de reacao a mudanca de nivel (log). Substitua/estenda
    // por LED, display, etc., sem tocar na logica de controle.
    controller.onLevelChanged().subscribe([](const core::LevelChangedEvent& e) {
        Serial.printf("[reservatorio] nivel %d -> %d\n",
                      static_cast<int>(e.previous), static_cast<int>(e.current));
    });

    Serial.println(F("[reservatorio] pronto"));
}

void loop() {
    controller.loop();
    powerMgr.update();

    // Politica de sleep (desabilitada por padrao). Em bateria e sem envio
    // pendente, poderia dormir kDeepSleepMs entre ciclos. Ver SleepPolicy.
    if (cfg::SleepPolicy::kDeepSleepMs > 0 && powerMgr.onBattery() &&
        !link.isDelivering()) {
        radio.sleep();
        powerMgr.deepSleepFor(cfg::SleepPolicy::kDeepSleepMs);  // reinicia ao acordar
    }
}
