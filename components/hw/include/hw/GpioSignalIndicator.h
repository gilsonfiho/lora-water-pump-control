#pragma once
// ============================================================================
//  GpioSignalIndicator.h  -  LED de RSSI num GPIO (ativo alto por padrao).
// ============================================================================

#include <driver/gpio.h>

#include "config/PinConfig.h"
#include "hw/ISignalIndicator.h"

namespace hw {

class GpioSignalIndicator : public ISignalIndicator {
public:
    bool begin() override {
        const gpio_num_t pin = static_cast<gpio_num_t>(cfg::IndicatorPins::kRssiLed);

        gpio_set_level(pin, inactiveLevel());  // nasce apagado

        gpio_config_t io = {};
        io.pin_bit_mask = (1ULL << cfg::IndicatorPins::kRssiLed);
        io.mode         = GPIO_MODE_OUTPUT;
        io.pull_up_en   = GPIO_PULLUP_DISABLE;
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.intr_type    = GPIO_INTR_DISABLE;
        if (gpio_config(&io) != ESP_OK) return false;

        set(false);
        return true;
    }

    void set(bool on) override {
        gpio_set_level(static_cast<gpio_num_t>(cfg::IndicatorPins::kRssiLed),
                       on ? activeLevel() : inactiveLevel());
    }

private:
    static uint32_t activeLevel()   { return cfg::IndicatorPins::kActiveHigh ? 1 : 0; }
    static uint32_t inactiveLevel() { return cfg::IndicatorPins::kActiveHigh ? 0 : 1; }
};

}  // namespace hw
