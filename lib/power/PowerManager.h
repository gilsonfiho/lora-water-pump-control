#pragma once
// ============================================================================
//  PowerManager.h  -  Failover fonte/bateria + politica de deep sleep.
// ----------------------------------------------------------------------------
//  O chaveamento fisico fonte<->bateria e feito em hardware (diodos/ideal-diode
//  ou modulo de carga com passagem). Este gerenciador apenas OBSERVA qual fonte
//  esta ativa (via IBatteryMonitor) e decide, para o no do reservatorio, quando
//  vale a pena dormir para poupar bateria.
//
//  A bomba normalmente NAO dorme (precisa ouvir comandos e aplicar o failsafe),
//  entao a politica de sleep so e usada pelo no do reservatorio.
// ============================================================================

#include <cstdint>

#include "hal/IBatteryMonitor.h"

namespace power {

class PowerManager {
public:
    explicit PowerManager(hal::IBatteryMonitor& battery) : battery_(battery) {}

    void begin() { last_ = battery_.read(); }

    // Atualiza a leitura de telemetria. Chamar no loop.
    void update() { last_ = battery_.read(); }

    hal::PowerSource source() const { return last_.source; }
    const hal::BatteryTelemetry& telemetry() const { return last_; }

    bool onBattery() const { return last_.source == hal::PowerSource::kBattery; }

    // Bateria abaixo de um limiar critico -> o chamador pode reduzir a cadencia
    // de TX ou entrar em sleep mais agressivo.
    bool batteryCritical(uint8_t thresholdPct = 15) const {
        return onBattery() && last_.socPercent > 0 &&
               last_.socPercent <= thresholdPct;
    }

    // Deep sleep por `ms`. No-op quando ms == 0 (usado em fonte externa ou
    // durante comissionamento). Nao retorna: o ESP reinicia ao acordar.
    void deepSleepFor(uint32_t ms);

private:
    hal::IBatteryMonitor& battery_;
    hal::BatteryTelemetry last_;
};

}  // namespace power
