#pragma once
// ============================================================================
//  StubBatteryMonitor.h  -  Telemetria de bateria fixa (SO BANCADA).
// ----------------------------------------------------------------------------
//  Reporta sempre "fonte externa, 100%". Dispensa o divisor resistivo e a linha
//  de fonte-presente na protoboard, que nao tem nada a ver com o que o
//  prototipo quer validar (o enlace e a logica de controle).
//
//  Efeitos colaterais desejados enquanto este stub estiver no lugar:
//    * PowerManager::onBattery() e sempre false -> o deep sleep nunca dispara;
//    * PowerManager::batteryCritical() e sempre false;
//    * os pacotes BATERIA_STATUS continuam saindo, com valores constantes, o
//      que ainda exercita esse caminho do protocolo.
//
//  Trocar por AdcBatteryMonitor quando o subsistema de energia for montado.
// ============================================================================

#include "hw/IBatteryMonitor.h"

namespace hw {

class StubBatteryMonitor : public IBatteryMonitor {
public:
    bool begin() override { return true; }

    BatteryTelemetry read() override {
        BatteryTelemetry t;
        t.milliVolts   = 4200;  // 1S Li-ion cheia
        t.milliAmps    = 0;
        t.socPercent   = 100;
        t.source       = PowerSource::kExternal;
        t.currentValid = false;
        return t;
    }
};

}  // namespace hw
