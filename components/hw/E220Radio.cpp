// ============================================================================
//  E220Radio.cpp  -  Implementacao ESP-IDF do driver UART do E220 (bloqueante).
// ============================================================================

#include "hw/E220Radio.h"

#include <cstring>

#include <driver/gpio.h>
#include <esp_log.h>

#include "config/PinConfig.h"

namespace hw {

namespace {
constexpr char TAG[] = "E220";

constexpr int      kUartRxBuf    = 1024;  // > 2x FIFO de hardware
constexpr int      kUartTxBuf    = 256;
constexpr uint32_t kAuxSettleMs  = 3;     // datasheet 6.1: ~2 ms apos AUX subir

// cfg::LoraPins usa int; o ESP-IDF 6 exige gpio_num_t nas chamadas de GPIO.
inline gpio_num_t pin(int p) { return static_cast<gpio_num_t>(p); }

uint32_t uartBaudToBps(cfg::LoraUartBaud baud) {
    switch (baud) {
        case cfg::LoraUartBaud::k9600:   return 9600;
        case cfg::LoraUartBaud::k19200:  return 19200;
        case cfg::LoraUartBaud::k38400:  return 38400;
        case cfg::LoraUartBaud::k57600:  return 57600;
        case cfg::LoraUartBaud::k115200: return 115200;
    }
    return 9600;
}
}  // namespace

E220Radio::~E220Radio() {
    if (uartInstalled_) uart_driver_delete(port_);
    if (txMutex_) vSemaphoreDelete(txMutex_);
}

bool E220Radio::configureGpios() {
    gpio_config_t mode_io = {};
    mode_io.pin_bit_mask = (1ULL << cfg::LoraPins::kM0) | (1ULL << cfg::LoraPins::kM1);
    mode_io.mode = GPIO_MODE_OUTPUT;
    if (gpio_config(&mode_io) != ESP_OK) return false;

    gpio_config_t aux_io = {};
    aux_io.pin_bit_mask = (1ULL << cfg::LoraPins::kAux);
    aux_io.mode = GPIO_MODE_INPUT;
    aux_io.pull_up_en = GPIO_PULLUP_ENABLE;
    return gpio_config(&aux_io) == ESP_OK;
}

bool E220Radio::installUart(uint32_t baud) {
    if (uartInstalled_) {
        return uart_set_baudrate(port_, baud) == ESP_OK;
    }
    uart_config_t uc = {};
    uc.baud_rate  = static_cast<int>(baud);
    uc.data_bits  = UART_DATA_8_BITS;
    uc.parity     = UART_PARITY_DISABLE;
    uc.stop_bits  = UART_STOP_BITS_1;
    uc.flow_ctrl  = UART_HW_FLOWCTRL_DISABLE;
    uc.source_clk = UART_SCLK_DEFAULT;

    if (uart_driver_install(port_, kUartRxBuf, kUartTxBuf, 0, nullptr, 0) != ESP_OK)
        return false;
    if (uart_param_config(port_, &uc) != ESP_OK) return false;
    if (uart_set_pin(port_, cfg::LoraPins::kUartTx, cfg::LoraPins::kUartRx,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK)
        return false;
    uartInstalled_ = true;
    return true;
}

bool E220Radio::begin() {
    txMutex_ = xSemaphoreCreateMutex();
    if (txMutex_ == nullptr) return false;
    if (!configureGpios()) return false;

    // Modo de configuracao a 9600 8N1 para gravar os registradores.
    if (!installUart(cfg::Radio::kConfigBaud)) return false;
    applyMode(Mode::kConfig);
    if (!waitForReady(1000)) {
        ESP_LOGE(TAG, "modulo nao respondeu ao power-on self-check");
        return false;
    }
    if (!writeConfigRegisters()) {
        ESP_LOGE(TAG, "falha ao gravar registradores");
        return false;
    }

    // Baud de operacao e volta ao modo transparente.
    installUart(uartBaudToBps(profile_.uartBaud));
    applyMode(Mode::kNormal);
    const bool ok = waitForReady(1000);
    ESP_LOGI(TAG, "iniciado (canal=%u, potencia=%u)",
             static_cast<unsigned>(profile_.channel),
             static_cast<unsigned>(profile_.power));
    return ok;
}

void E220Radio::applyMode(Mode mode) {
    const uint8_t bits = static_cast<uint8_t>(mode);
    gpio_set_level(pin(cfg::LoraPins::kM1), (bits & 0b10) ? 1 : 0);
    gpio_set_level(pin(cfg::LoraPins::kM0), (bits & 0b01) ? 1 : 0);
    currentMode_ = mode;
    vTaskDelay(pdMS_TO_TICKS(kAuxSettleMs));
}

bool E220Radio::waitForReady(uint32_t timeoutMs) {
    const TickType_t deadline = xTaskGetTickCount() + pdMS_TO_TICKS(timeoutMs);
    while (gpio_get_level(pin(cfg::LoraPins::kAux)) == 0) {
        if (xTaskGetTickCount() >= deadline) return false;
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    vTaskDelay(pdMS_TO_TICKS(kAuxSettleMs));
    return true;
}

bool E220Radio::writeConfigRegisters() {
    const uint8_t reg0 =
        (static_cast<uint8_t>(profile_.uartBaud) << 5) |
        (0b00 << 3) |                                      // 8N1
        (static_cast<uint8_t>(profile_.airRate) & 0b111);
    const uint8_t reg1 =
        (0b00 << 6) |                                      // subpacote 200 B
        (0 << 5) |                                         // RSSI ambiente off
        (static_cast<uint8_t>(profile_.power) & 0b11);
    const uint8_t reg3 =
        ((profile_.appendRssi ? 1 : 0) << 7) |
        (0 << 6) |                                         // transparente
        ((profile_.enableLbt ? 1 : 0) << 4) |
        (0b000);                                           // ciclo WOR 500 ms

    const uint8_t payload[] = {
        0xC0, 0x00, 0x06,
        static_cast<uint8_t>(profile_.address >> 8),
        static_cast<uint8_t>(profile_.address & 0xFF),
        reg0, reg1, profile_.channel, reg3,
    };

    uart_flush_input(port_);
    uart_write_bytes(port_, payload, sizeof(payload));
    uart_wait_tx_done(port_, pdMS_TO_TICKS(200));

    uint8_t resp[3] = {0};
    const int n = uart_read_bytes(port_, resp, sizeof(resp), pdMS_TO_TICKS(300));
    return n >= 1 && resp[0] == 0xC1;
}

bool E220Radio::send(const uint8_t* data, size_t length) {
    if (length == 0 || length > kMaxRadioFrame) return false;
    if (xSemaphoreTake(txMutex_, pdMS_TO_TICKS(1000)) != pdTRUE) return false;

    bool ok = false;
    if (waitForReady(1000)) {
        ok = uart_write_bytes(port_, data, length) == static_cast<int>(length);
    }
    xSemaphoreGive(txMutex_);
    return ok;
}

int E220Radio::read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) {
    const int n = uart_read_bytes(port_, buffer, maxLen, pdMS_TO_TICKS(timeoutMs));
    return n < 0 ? 0 : n;
}

bool E220Radio::isBusy() {
    return gpio_get_level(pin(cfg::LoraPins::kAux)) == 0;
}

void E220Radio::sleep() {
    // Modo 3 tambem e o modo de sleep profundo (~5 uA).
    applyMode(Mode::kConfig);
}

void E220Radio::wake() {
    applyMode(Mode::kNormal);
    waitForReady(1000);
}

}  // namespace hw
