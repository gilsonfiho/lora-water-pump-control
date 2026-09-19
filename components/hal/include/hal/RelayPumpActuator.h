#pragma once
// ============================================================================
//  RelayPumpActuator.h  -  Aciona a bomba por um rele/contator (fail-safe OFF).
// ----------------------------------------------------------------------------
//  Sempre inicia DESLIGADO. O nivel logico ativo vem de cfg::PumpPins para
//  acomodar tanto modulos rele "ativo alto" quanto "ativo baixo".
//
//  Seguranca: o pino so vira saida DEPOIS de ter o nivel inativo definido, para
//  que o rele nao receba um pulso espurio durante o boot.
// ============================================================================

#include <driver/gpio.h>

#include "hal/IPumpActuator.h"
#include "config/PinConfig.h"

namespace hal {

class RelayPumpActuator : public IPumpActuator {
public:
    bool begin() override {
        const gpio_num_t pin = static_cast<gpio_num_t>(cfg::PumpPins::kRelay);

        // Nivel inativo primeiro: gpio_set_level sobre um pino ainda nao
        // configurado ja grava no registrador de saida, entao ao habilitar a
        // saida o pino ja nasce no estado seguro.
        gpio_set_level(pin, inactiveLevel());

        gpio_config_t io = {};
        io.pin_bit_mask = (1ULL << cfg::PumpPins::kRelay);
        io.mode         = GPIO_MODE_OUTPUT;
        io.pull_up_en   = GPIO_PULLUP_DISABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type    = GPIO_INTR_DISABLE;
        if (gpio_config(&io) != ESP_OK) {
            return false;
        }

        turnOff();  // estado seguro antes de qualquer comando
        return true;
    }

    void turnOn() override {
        gpio_set_level(static_cast<gpio_num_t>(cfg::PumpPins::kRelay), activeLevel());
        on_ = true;
    }

    void turnOff() override {
        gpio_set_level(static_cast<gpio_num_t>(cfg::PumpPins::kRelay), inactiveLevel());
        on_ = false;
    }

    bool isOn() const override { return on_; }

private:
    static uint32_t activeLevel()   { return cfg::PumpPins::kActiveHigh ? 1 : 0; }
    static uint32_t inactiveLevel() { return cfg::PumpPins::kActiveHigh ? 0 : 1; }

    bool on_ = false;
};

}  // namespace hal
