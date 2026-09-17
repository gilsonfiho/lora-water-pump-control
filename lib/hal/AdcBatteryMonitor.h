#pragma once
// ============================================================================
//  AdcBatteryMonitor.h  -  Monitor de bateria so por tensao (divisor + ADC).
// ----------------------------------------------------------------------------
//  Le a tensao do pack por um divisor resistivo no ADC do ESP32-C3 e detecta a
//  fonte ativa pela linha "fonte externa presente". Nao mede corrente; para
//  isso use Ina219BatteryMonitor. SoC e estimado por uma curva simples de
//  tensao (aproximacao para Li-ion; refine conforme sua celula).
// ============================================================================

#include <Arduino.h>

#include "IBatteryMonitor.h"
#include "config/PinConfig.h"

namespace hal {

class AdcBatteryMonitor : public IBatteryMonitor {
public:
    // dividerRatio = (R1 + R2) / R2. Ex.: 100k/100k -> 2.0.
    explicit AdcBatteryMonitor(float dividerRatio = 2.0f)
        : dividerRatio_(dividerRatio) {}

    bool begin() override {
        pinMode(cfg::PowerPins::kExternalPresent, INPUT);
        analogReadResolution(12);  // 0..4095
        return true;
    }

    BatteryTelemetry read() override {
        BatteryTelemetry t;
        const uint32_t mvAdc = analogReadMilliVolts(cfg::PowerPins::kBatteryAdc);
        t.milliVolts   = static_cast<uint16_t>(mvAdc * dividerRatio_);
        t.currentValid = false;
        t.socPercent   = estimateSoc(t.milliVolts);
        t.source = (digitalRead(cfg::PowerPins::kExternalPresent) == HIGH)
                       ? PowerSource::kExternal
                       : PowerSource::kBattery;
        return t;
    }

private:
    // Curva grosseira 1S Li-ion: 3.30 V ~ 0%, 4.20 V ~ 100%. Aproximacao
    // linear; substitua por tabela de descarga se precisar de precisao.
    static uint8_t estimateSoc(uint16_t milliVolts) {
        constexpr uint16_t kEmptyMv = 3300;
        constexpr uint16_t kFullMv  = 4200;
        if (milliVolts <= kEmptyMv) return 0;
        if (milliVolts >= kFullMv)  return 100;
        return static_cast<uint8_t>(
            (static_cast<uint32_t>(milliVolts - kEmptyMv) * 100) /
            (kFullMv - kEmptyMv));
    }

    float dividerRatio_;
};

}  // namespace hal
