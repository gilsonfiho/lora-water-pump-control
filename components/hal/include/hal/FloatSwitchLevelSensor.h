#pragma once
// ============================================================================
//  FloatSwitchLevelSensor.h  -  Sensor de nivel com duas boias (histerese).
// ----------------------------------------------------------------------------
//  Boia baixa fecha quando a agua cai abaixo da marca inferior; boia alta fecha
//  quando a agua sobe acima da marca superior. Entre as marcas o estado e kMid,
//  o que da a histerese natural que evita o liga-desliga rapido da bomba.
//
//  Entradas com pull-up interno: boia fechada -> nivel logico 0. Ajuste o
//  parametro `closedIsLow` conforme a fiacao das suas boias.
// ============================================================================

#include <driver/gpio.h>

#include "hal/ILevelSensor.h"
#include "config/PinConfig.h"

namespace hal {

class FloatSwitchLevelSensor : public ILevelSensor {
public:
    explicit FloatSwitchLevelSensor(bool closedIsLow = true)
        : closedIsLow_(closedIsLow) {}

    bool begin() override {
        gpio_config_t io = {};
        io.pin_bit_mask = (1ULL << cfg::LevelSensorPins::kFloatLow) |
                          (1ULL << cfg::LevelSensorPins::kFloatHigh);
        io.mode         = GPIO_MODE_INPUT;
        // O pull-up interno so faz sentido com a boia levando o pino ao GND,
        // que e a ligacao assumida por closedIsLow = true.
        io.pull_up_en   = closedIsLow_ ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        io.pull_down_en = closedIsLow_ ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
        io.intr_type    = GPIO_INTR_DISABLE;
        return gpio_config(&io) == ESP_OK;
    }

    LevelState read() override {
        const bool lowClosed  = isClosed(cfg::LevelSensorPins::kFloatLow);
        const bool highClosed = isClosed(cfg::LevelSensorPins::kFloatHigh);

        // Combinacao invalida (alta fechada mas baixa aberta) indica boia presa
        // ou fiacao trocada -> reporta desconhecido para forcar estado seguro.
        if (highClosed && !lowClosed) return LevelState::kUnknown;

        if (highClosed) return LevelState::kHigh;   // cheia
        if (!lowClosed) return LevelState::kLow;    // vazia (boia baixa aberta)
        return LevelState::kMid;                    // entre as marcas
    }

    bool readPercent(uint8_t&) override {
        return false;  // boias dao apenas estados discretos
    }

private:
    bool isClosed(int pin) const {
        const int level = gpio_get_level(static_cast<gpio_num_t>(pin));
        return closedIsLow_ ? (level == 0) : (level == 1);
    }

    bool closedIsLow_;
};

}  // namespace hal
