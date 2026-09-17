#pragma once
// ============================================================================
//  RelayPumpActuator.h  -  Aciona a bomba por um rele/contator (fail-safe OFF).
// ----------------------------------------------------------------------------
//  Sempre inicia DESLIGADO. O nivel logico ativo vem de cfg::PumpPins para
//  acomodar tanto modulos rele "ativo alto" quanto "ativo baixo".
// ============================================================================

#include <Arduino.h>

#include "IPumpActuator.h"
#include "config/PinConfig.h"

namespace hal {

class RelayPumpActuator : public IPumpActuator {
public:
    bool begin() override {
        pinMode(cfg::PumpPins::kRelay, OUTPUT);
        turnOff();  // estado seguro antes de qualquer comando
        return true;
    }

    void turnOn() override {
        digitalWrite(cfg::PumpPins::kRelay, activeLevel());
        on_ = true;
    }

    void turnOff() override {
        digitalWrite(cfg::PumpPins::kRelay, inactiveLevel());
        on_ = false;
    }

    bool isOn() const override { return on_; }

private:
    static int activeLevel()   { return cfg::PumpPins::kActiveHigh ? HIGH : LOW; }
    static int inactiveLevel() { return cfg::PumpPins::kActiveHigh ? LOW : HIGH; }

    bool on_ = false;
};

}  // namespace hal
