#pragma once
// ============================================================================
//  UltrasonicLevelSensor.h  -  Sensor de nivel ultrassonico (ESQUELETO).
// ----------------------------------------------------------------------------
//  Alternativa as boias: mede a distancia da tampa ate a lamina d'agua e
//  converte em porcentagem e em estado discreto usando as mesmas marcas de
//  histerese. Preencha os TODOs conforme o sensor escolhido (ex.: JSN-SR04T).
// ============================================================================

#include <driver/gpio.h>

#include "hw/ILevelSensor.h"
#include "config/PinConfig.h"

namespace hw {

class UltrasonicLevelSensor : public ILevelSensor {
public:
    // Geometria da caixa: distancia do sensor ao fundo e alturas das marcas.
    struct Geometry {
        uint16_t emptyDistanceMm = 1500;  // sensor -> fundo (0%)   TODO(hw)
        uint16_t fullDistanceMm  = 200;   // sensor -> cheio (100%) TODO(hw)
        uint8_t  lowThresholdPct  = 20;   // abaixo disso -> kLow
        uint8_t  highThresholdPct = 90;   // acima disso  -> kHigh
    };

    explicit UltrasonicLevelSensor(const Geometry& geom) : geom_(geom) {}

    bool begin() override {
        gpio_config_t trig = {};
        trig.pin_bit_mask = (1ULL << cfg::LevelSensorPins::kUltrasonicTrig);
        trig.mode         = GPIO_MODE_OUTPUT;
        trig.intr_type    = GPIO_INTR_DISABLE;
        if (gpio_config(&trig) != ESP_OK) return false;

        gpio_config_t echo = {};
        echo.pin_bit_mask = (1ULL << cfg::LevelSensorPins::kUltrasonicEcho);
        echo.mode         = GPIO_MODE_INPUT;
        echo.intr_type    = GPIO_INTR_DISABLE;
        return gpio_config(&echo) == ESP_OK;
    }

    LevelState read() override {
        uint8_t pct = 0;
        if (!readPercent(pct)) return LevelState::kUnknown;
        if (pct <= geom_.lowThresholdPct)  return LevelState::kLow;
        if (pct >= geom_.highThresholdPct) return LevelState::kHigh;
        return LevelState::kMid;
    }

    bool readPercent(uint8_t& percentOut) override {
        // TODO(hw): disparar pulso, medir eco, converter distancia -> %.
        // Aplicar mediana de N leituras para rejeitar ruido de superficie.
        (void)percentOut;
        return false;  // ainda nao implementado
    }

private:
    Geometry geom_;
};

}  // namespace hw
