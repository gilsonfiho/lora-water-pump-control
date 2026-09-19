#pragma once
// ============================================================================
//  IBatteryMonitor.h  -  Telemetria abstrata de bateria/fonte.
// ----------------------------------------------------------------------------
//  Implementacoes: AdcBatteryMonitor (apenas tensao, via divisor) e
//  Ina219BatteryMonitor (tensao + corrente + potencia via I2C).
// ============================================================================

#include <cstdint>

namespace hw {

// De onde o no esta drenando energia no momento. Guia a logica de failover e e
// reportado nos pacotes BATERIA_STATUS.
enum class PowerSource : uint8_t {
    kUnknown = 0,
    kExternal,   // rede/adaptador presente
    kBattery,    // rodando no pack 18650
};

struct BatteryTelemetry {
    uint16_t    milliVolts   = 0;      // tensao do pack
    int16_t     milliAmps    = 0;      // + descarga / - carga (0 se desconhecido)
    uint8_t     socPercent   = 0;      // estado de carga estimado [0..100]
    PowerSource source       = PowerSource::kUnknown;
    bool        currentValid = false;  // false para monitores so de tensao
};

class IBatteryMonitor {
public:
    virtual ~IBatteryMonitor() = default;

    virtual bool begin() = 0;

    // Amostra a telemetria mais recente. Nao bloqueante.
    virtual BatteryTelemetry read() = 0;
};

}  // namespace hw
