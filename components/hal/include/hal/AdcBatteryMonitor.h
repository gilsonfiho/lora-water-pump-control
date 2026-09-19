#pragma once
// ============================================================================
//  AdcBatteryMonitor.h  -  Monitor de bateria so por tensao (divisor + ADC).
// ----------------------------------------------------------------------------
//  Le a tensao do pack por um divisor resistivo no ADC do ESP32-C3 e detecta a
//  fonte ativa pela linha "fonte externa presente". Nao mede corrente; para
//  isso use Ina219BatteryMonitor. SoC e estimado por uma curva simples de
//  tensao (aproximacao para Li-ion; refine conforme sua celula).
//
//  Usa a API oneshot do ESP-IDF (esp_adc), com calibracao por curve fitting
//  quando os eFuses da placa a suportam. Sem calibracao, cai para uma
//  conversao linear pela referencia nominal -- menos exata, mas utilizavel.
// ============================================================================

#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_oneshot.h>

#include "hal/IBatteryMonitor.h"
#include "config/PinConfig.h"

namespace hal {

class AdcBatteryMonitor : public IBatteryMonitor {
public:
    // dividerRatio = (R1 + R2) / R2. Ex.: 100k/100k -> 2.0.
    explicit AdcBatteryMonitor(float dividerRatio = 2.0f);
    ~AdcBatteryMonitor() override;

    bool begin() override;
    BatteryTelemetry read() override;

private:
    // Curva grosseira 1S Li-ion: 3.30 V ~ 0%, 4.20 V ~ 100%. Aproximacao
    // linear; substitua por tabela de descarga se precisar de precisao.
    static uint8_t estimateSoc(uint16_t milliVolts);

    float dividerRatio_;
    adc_oneshot_unit_handle_t unit_ = nullptr;
    adc_cali_handle_t         cali_ = nullptr;
    adc_unit_t                unitId_{};
    adc_channel_t             channel_{};
    bool                      ready_ = false;
};

}  // namespace hal
