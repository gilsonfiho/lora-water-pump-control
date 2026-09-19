#pragma once
// ============================================================================
//  BenchSwitchLevelSensor.h  -  Chave manual no lugar da boia (SO BANCADA).
// ----------------------------------------------------------------------------
//  Substituto da boia para validar o sistema na mesa: uma chave que TRAVA na
//  posicao faz o papel do flutuador. A semantica e a mesma da boia, entao
//  ReservoirController, protocolo e no da bomba nao mudam em nada -- e
//  exatamente para isso que ILevelSensor existe.
//
//      fechada (pino no GND) -> kLow   -> LIGAR bomba
//      aberta  (pull-up)     -> kHigh  -> DESLIGAR bomba
//
//  Nao ha kMid: com uma chave so nao existe faixa intermediaria, entao tambem
//  nao existe a histerese que duas boias dao. Isso e aceitavel na bancada
//  porque quem manda na transicao e a mao do operador, nao a agua.
//
//  Por que chave que trava e nao botao: read() so e chamado uma vez por ciclo
//  do reservatorio. Um botao momentaneo quase nunca estaria pressionado no
//  instante da leitura.
// ============================================================================

#include <driver/gpio.h>

#include "config/PinConfig.h"
#include "hal/ILevelSensor.h"

namespace hal {

class BenchSwitchLevelSensor : public ILevelSensor {
public:
    // closedIsLow = true assume a ligacao comum: um lado da chave no pino, o
    // outro no GND, com o pull-up interno ligado.
    explicit BenchSwitchLevelSensor(bool closedIsLow = true)
        : closedIsLow_(closedIsLow) {}

    bool begin() override {
        gpio_config_t io = {};
        io.pin_bit_mask = (1ULL << cfg::BenchPins::kLevelSwitch);
        io.mode         = GPIO_MODE_INPUT;
        io.pull_up_en   = closedIsLow_ ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE;
        io.pull_down_en = closedIsLow_ ? GPIO_PULLDOWN_DISABLE : GPIO_PULLDOWN_ENABLE;
        io.intr_type    = GPIO_INTR_DISABLE;
        return gpio_config(&io) == ESP_OK;
    }

    LevelState read() override {
        return isClosed() ? LevelState::kLow    // pedindo agua
                          : LevelState::kHigh;  // satisfeito
    }

    bool readPercent(uint8_t&) override {
        return false;  // uma chave da apenas dois estados
    }

private:
    bool isClosed() const {
        const gpio_num_t pin = static_cast<gpio_num_t>(cfg::BenchPins::kLevelSwitch);
        const int level = gpio_get_level(pin);
        return closedIsLow_ ? (level == 0) : (level == 1);
    }

    bool closedIsLow_;
};

}  // namespace hal
