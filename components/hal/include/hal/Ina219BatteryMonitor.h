#pragma once
// ============================================================================
//  Ina219BatteryMonitor.h  -  Monitor de bateria via INA219/INA226 (ESQUELETO).
// ----------------------------------------------------------------------------
//  Mede tensao E corrente (carga/descarga) via I2C, dando telemetria muito mais
//  rica que o ADC. Preferido quando o hardware o incluir. Preencha os TODOs
//  usando driver/i2c_master do ESP-IDF.
// ============================================================================

#include <cstdint>

#include "hal/IBatteryMonitor.h"
#include "config/PinConfig.h"

namespace hal {

class Ina219BatteryMonitor : public IBatteryMonitor {
public:
    explicit Ina219BatteryMonitor(uint8_t i2cAddress = 0x40)
        : i2cAddress_(i2cAddress) {}

    bool begin() override {
        // TODO(hw): i2c_new_master_bus() em kI2cSda/kI2cScl; configurar a
        // calibracao do shunt conforme o resistor da placa. Retornar false se o
        // IC nao der ACK.
        (void)i2cAddress_;
        return false;  // ainda nao implementado -> caller pode cair no ADC
    }

    BatteryTelemetry read() override {
        BatteryTelemetry t;
        // TODO(hw): ler tensao de barramento, corrente do shunt e sinal.
        //   t.milliVolts = ...;  t.milliAmps = ...;  t.currentValid = true;
        //   t.source = (t.milliAmps < 0) ? kExternal : kBattery;  // carregando?
        t.currentValid = false;
        return t;
    }

private:
    uint8_t i2cAddress_;
};

}  // namespace hal
