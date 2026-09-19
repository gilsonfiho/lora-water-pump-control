// ============================================================================
//  AdcBatteryMonitor.cpp  -  Leitura de tensao da bateria via ADC oneshot.
// ============================================================================

#include "hal/AdcBatteryMonitor.h"

#include <driver/gpio.h>
#include <esp_idf_version.h>
#include <esp_log.h>

namespace hal {

namespace {

constexpr char kTag[] = "battery";

// Atenuacao maxima: estende a faixa util do ADC para ~0..3,1 V, necessaria
// porque o divisor entrega metade da tensao do pack (4,2 V -> 2,1 V).
//
// O IDF 5.2 renomeou ADC_ATTEN_DB_11 para ADC_ATTEN_DB_12 (mesmo valor) e
// marcou o nome antigo como deprecated. Como sao enumeradores, e nao macros, o
// teste precisa ser pela versao do IDF -- ESP_IDF_VERSION e macro de verdade.
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 2, 0)
constexpr adc_atten_t kAtten = ADC_ATTEN_DB_12;
#else
constexpr adc_atten_t kAtten = ADC_ATTEN_DB_11;
#endif

// Fundo de escala aproximado nessa atenuacao, usado apenas no caminho sem
// calibracao por eFuse.
constexpr uint32_t kFallbackFullScaleMv = 3100;
constexpr uint32_t kFallbackMaxRaw      = 4095;  // ADC_BITWIDTH_DEFAULT = 12 bits

}  // namespace

AdcBatteryMonitor::AdcBatteryMonitor(float dividerRatio)
    : dividerRatio_(dividerRatio) {}

AdcBatteryMonitor::~AdcBatteryMonitor() {
    if (cali_ != nullptr) {
        adc_cali_delete_scheme_curve_fitting(cali_);
    }
    if (unit_ != nullptr) {
        adc_oneshot_del_unit(unit_);
    }
}

bool AdcBatteryMonitor::begin() {
    // Linha "fonte externa presente": entrada simples, sem pull interno (o
    // divisor/optoacoplador da placa define o nivel).
    gpio_config_t io = {};
    io.pin_bit_mask = (1ULL << cfg::PowerPins::kExternalPresent);
    io.mode         = GPIO_MODE_INPUT;
    io.pull_up_en   = GPIO_PULLUP_DISABLE;
    io.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io.intr_type    = GPIO_INTR_DISABLE;
    if (gpio_config(&io) != ESP_OK) {
        return false;
    }

    // Descobre unidade e canal a partir do GPIO, em vez de fixar ADC1_CH0:
    // se o pino mudar em PinConfig, isto acompanha.
    if (adc_oneshot_io_to_channel(cfg::PowerPins::kBatteryAdc, &unitId_, &channel_)
            != ESP_OK) {
        ESP_LOGE(kTag, "GPIO %d nao e um pino de ADC", cfg::PowerPins::kBatteryAdc);
        return false;
    }

    adc_oneshot_unit_init_cfg_t init = {};
    init.unit_id = unitId_;
    if (adc_oneshot_new_unit(&init, &unit_) != ESP_OK) {
        ESP_LOGE(kTag, "adc_oneshot_new_unit falhou");
        return false;
    }

    adc_oneshot_chan_cfg_t chan = {};
    chan.atten    = kAtten;
    chan.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_oneshot_config_channel(unit_, channel_, &chan) != ESP_OK) {
        ESP_LOGE(kTag, "adc_oneshot_config_channel falhou");
        return false;
    }

    // Calibracao e opcional: depende dos eFuses gravados de fabrica. A falha
    // aqui nao e fatal, so reduz a exatidao da leitura.
    adc_cali_curve_fitting_config_t cali = {};
    cali.unit_id  = unitId_;
    cali.chan     = channel_;
    cali.atten    = kAtten;
    cali.bitwidth = ADC_BITWIDTH_DEFAULT;
    if (adc_cali_create_scheme_curve_fitting(&cali, &cali_) != ESP_OK) {
        cali_ = nullptr;
        ESP_LOGW(kTag, "sem calibracao de eFuse; tensao sera aproximada");
    }

    ready_ = true;
    return true;
}

BatteryTelemetry AdcBatteryMonitor::read() {
    BatteryTelemetry t;
    t.currentValid = false;

    if (!ready_) {
        // Sem ADC nao ha leitura confiavel; reporta 0 e deixa a politica de
        // energia tratar como bateria critica.
        t.milliVolts = 0;
        t.socPercent = 0;
        t.source     = PowerSource::kBattery;
        return t;
    }

    int raw = 0;
    if (adc_oneshot_read(unit_, channel_, &raw) != ESP_OK) {
        t.milliVolts = 0;
        t.socPercent = 0;
        t.source     = PowerSource::kBattery;
        return t;
    }

    uint32_t mvAdc = 0;
    if (cali_ != nullptr) {
        int mv = 0;
        if (adc_cali_raw_to_voltage(cali_, raw, &mv) == ESP_OK && mv > 0) {
            mvAdc = static_cast<uint32_t>(mv);
        }
    }
    if (mvAdc == 0) {
        mvAdc = (static_cast<uint32_t>(raw) * kFallbackFullScaleMv) / kFallbackMaxRaw;
    }

    t.milliVolts = static_cast<uint16_t>(mvAdc * dividerRatio_);
    t.socPercent = estimateSoc(t.milliVolts);
    t.source = (gpio_get_level(
                    static_cast<gpio_num_t>(cfg::PowerPins::kExternalPresent)) == 1)
                   ? PowerSource::kExternal
                   : PowerSource::kBattery;
    return t;
}

uint8_t AdcBatteryMonitor::estimateSoc(uint16_t milliVolts) {
    constexpr uint16_t kEmptyMv = 3300;
    constexpr uint16_t kFullMv  = 4200;
    if (milliVolts <= kEmptyMv) return 0;
    if (milliVolts >= kFullMv)  return 100;
    return static_cast<uint8_t>(
        (static_cast<uint32_t>(milliVolts - kEmptyMv) * 100) /
        (kFullMv - kEmptyMv));
}

}  // namespace hal
