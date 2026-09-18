// ============================================================================
//  main.cpp  -  No da BOMBA (captacao) / RECEPTOR / ATUADOR
// ----------------------------------------------------------------------------
//  Recebe comandos do reservatorio via LoRa, aciona o rele/contator e aplica o
//  FAILSAFE: se o enlace ficar em silencio por mais de kLinkLostTimeoutMs, a
//  bomba e DESLIGADA por seguranca ate o enlace voltar.
// ============================================================================

#include <Arduino.h>

#include "config/NodeConfig.h"
#include "config/RadioConfig.h"
#include "core/PumpController.h"
#include "hal/E220Radio.h"
#include "hal/RelayPumpActuator.h"
#include "protocol/LinkLayer.h"

namespace {

cfg::LoraProfile makeProfile() {
    cfg::LoraProfile p;
    p.variant = cfg::E220Variant::kT30D;  // TODO(hw): T30D ou T22D
    p.power   = cfg::LoraPowerLevel::kMax;
    p.airRate = cfg::LoraAirRate::k2_4k;   // DEVE casar com o reservatorio
    return p;
}

hal::E220Radio       radio(Serial1, makeProfile());
hal::RelayPumpActuator pump;
protocol::LinkLayer  link(radio, cfg::kAddrPump);
core::PumpController controller(link, pump);

}  // namespace

void setup() {
    Serial.begin(115200);
    Serial.println(F("[bomba] iniciando..."));

    // Estado seguro primeiro: o controlador ja forca a bomba DESLIGADA.
    controller.begin();

    if (!radio.begin()) {
        Serial.println(F("[bomba] FALHA ao iniciar o radio E220"));
        // Sem radio, o failsafe manda: a bomba permanece desligada.
    }

    Serial.println(F("[bomba] pronto"));
}

void loop() {
    controller.loop();

    // Log leve de transicao de estado (opcional).
    static core::PumpController::State last = core::PumpController::State::kBoot;
    if (controller.state() != last) {
        last = controller.state();
        Serial.printf("[bomba] estado -> %d (rele=%s)\n",
                      static_cast<int>(last), pump.isOn() ? "ON" : "OFF");
    }
}
