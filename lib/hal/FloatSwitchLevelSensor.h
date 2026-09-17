#pragma once
// ============================================================================
//  FloatSwitchLevelSensor.h  -  Sensor de nivel com duas boias (histerese).
// ----------------------------------------------------------------------------
//  Boia baixa fecha quando a agua cai abaixo da marca inferior; boia alta fecha
//  quando a agua sobe acima da marca superior. Entre as marcas o estado e kMid,
//  o que da a histerese natural que evita o liga-desliga rapido da bomba.
//
//  Entradas com INPUT_PULLUP: boia fechada -> nivel logico LOW. Ajuste o
//  parametro `closedIsLow` conforme a fiacao das suas boias.
// ============================================================================

#include <Arduino.h>

#include "ILevelSensor.h"
#include "config/PinConfig.h"

namespace hal {

class FloatSwitchLevelSensor : public ILevelSensor {
public:
    explicit FloatSwitchLevelSensor(bool closedIsLow = true)
        : closedIsLow_(closedIsLow) {}

    bool begin() override {
        pinMode(cfg::LevelSensorPins::kFloatLow, INPUT_PULLUP);
        pinMode(cfg::LevelSensorPins::kFloatHigh, INPUT_PULLUP);
        return true;
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
        const int level = digitalRead(pin);
        return closedIsLow_ ? (level == LOW) : (level == HIGH);
    }

    bool closedIsLow_;
};

}  // namespace hal
