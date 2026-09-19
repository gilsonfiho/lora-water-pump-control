// ============================================================================
//  E220Radio.cpp  -  Implementacao do driver UART do E220 (secoes 5, 6 e 7 do
//                    datasheet EBYTE E220-900T30D / T22D).
// ============================================================================

#include "hal/E220Radio.h"

#include <cstring>

#include <driver/gpio.h>
#include <esp_log.h>

#include "platform/Clock.h"

namespace hal {

namespace {

constexpr char kTag[] = "e220";

// Folga de ociosidade entre bytes que fecha um quadro RX. Em 9600 8N1 um byte
// leva ~1 ms; 12 ms de silencio indica que o modulo terminou de entregar o
// quadro atual pela UART.
constexpr uint32_t kFrameGapMs = 12;

// Apos AUX subir, o datasheet pede ~2 ms de estabilidade antes de operar no
// novo modo (secao 6.1, nota 3).
constexpr uint32_t kAuxSettleMs = 3;

// O driver da UART do IDF exige buffer de RX maior que a FIFO de hardware
// (128 B). O E220 tem buffer interno de 400 B, entao 512 cobre um lote inteiro.
constexpr int kUartRxBufferBytes = 512;
constexpr int kUartTxBufferBytes = 256;

// Baud do ESP em cada modo. Configuracao e sempre 9600 8N1 (secao 7.1).
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

E220Radio::E220Radio(uart_port_t port, const cfg::LoraProfile& profile)
    : port_(port), profile_(profile) {}

E220Radio::~E220Radio() {
    if (uartInstalled_) {
        uart_driver_delete(port_);
    }
}

bool E220Radio::configureGpios() {
    // M0/M1 sao saidas; AUX e entrada. O modulo tem pull-up fraco interno em
    // M0/M1, mas nao confiamos nele: eles sao dirigidos explicitamente.
    gpio_config_t out = {};
    out.pin_bit_mask = (1ULL << cfg::LoraPins::kM0) | (1ULL << cfg::LoraPins::kM1);
    out.mode         = GPIO_MODE_OUTPUT;
    out.pull_up_en   = GPIO_PULLUP_DISABLE;
    out.pull_down_en = GPIO_PULLDOWN_DISABLE;
    out.intr_type    = GPIO_INTR_DISABLE;
    if (gpio_config(&out) != ESP_OK) {
        ESP_LOGE(kTag, "falha ao configurar M0/M1");
        return false;
    }

    // AUX com pull-up: se o modulo estiver ausente ou desconectado, a linha le
    // ALTO (pronto) em vez de flutuar. O begin() ainda falha adiante, no
    // handshake de registradores, que e a deteccao confiavel.
    gpio_config_t in = {};
    in.pin_bit_mask = (1ULL << cfg::LoraPins::kAux);
    in.mode         = GPIO_MODE_INPUT;
    in.pull_up_en   = GPIO_PULLUP_ENABLE;
    in.pull_down_en = GPIO_PULLDOWN_DISABLE;
    in.intr_type    = GPIO_INTR_DISABLE;
    if (gpio_config(&in) != ESP_OK) {
        ESP_LOGE(kTag, "falha ao configurar AUX");
        return false;
    }
    return true;
}

bool E220Radio::installUart(uint32_t baud) {
    uart_config_t uc = {};
    uc.baud_rate = static_cast<int>(baud);
    uc.data_bits = UART_DATA_8_BITS;
    uc.parity    = UART_PARITY_DISABLE;   // 8N1, como exige o modo de config
    uc.stop_bits = UART_STOP_BITS_1;
    uc.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uc.source_clk = UART_SCLK_DEFAULT;

    if (uart_driver_install(port_, kUartRxBufferBytes, kUartTxBufferBytes,
                            0, nullptr, 0) != ESP_OK) {
        ESP_LOGE(kTag, "uart_driver_install falhou");
        return false;
    }
    uartInstalled_ = true;

    if (uart_param_config(port_, &uc) != ESP_OK) {
        ESP_LOGE(kTag, "uart_param_config falhou");
        return false;
    }
    // Ordem dos pinos no IDF: (porta, TX, RX, RTS, CTS). O TX do ESP vai ao
    // RXD do modulo e vice-versa.
    if (uart_set_pin(port_, cfg::LoraPins::kUartTx, cfg::LoraPins::kUartRx,
                     UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE) != ESP_OK) {
        ESP_LOGE(kTag, "uart_set_pin falhou");
        return false;
    }
    return true;
}

bool E220Radio::begin() {
    if (!configureGpios()) {
        return false;
    }

    // Entra em CONFIG a 9600 8N1 para escrever os registradores.
    if (!installUart(cfg::Radio::kConfigBaud)) {
        return false;
    }
    applyMode(Mode::kConfig);
    if (!waitForReady(1000)) {
        ESP_LOGE(kTag, "AUX nao subiu: modulo ausente ou sem alimentacao");
        return false;  // modulo nao respondeu ao power-on self-check
    }

    if (!writeConfigRegisters()) {
        ESP_LOGE(kTag, "modulo nao confirmou a escrita de registradores");
        return false;
    }

    // Volta a UART para a baud de operacao e retorna ao modo transparente.
    const uint32_t opBaud = uartBaudToBps(profile_.uartBaud);
    if (uart_set_baudrate(port_, static_cast<int>(opBaud)) != ESP_OK) {
        ESP_LOGE(kTag, "uart_set_baudrate falhou");
        return false;
    }
    applyMode(Mode::kNormal);
    return waitForReady(1000);
}

void E220Radio::applyMode(Mode mode) {
    // M1 = bit alto, M0 = bit baixo do valor de modo.
    const uint8_t bits = static_cast<uint8_t>(mode);
    gpio_set_level(static_cast<gpio_num_t>(cfg::LoraPins::kM1), (bits & 0b10) ? 1 : 0);
    gpio_set_level(static_cast<gpio_num_t>(cfg::LoraPins::kM0), (bits & 0b01) ? 1 : 0);
    currentMode_ = mode;
}

bool E220Radio::waitForReady(uint32_t timeoutMs) {
    // Espera cooperativa pela borda de subida do AUX (secao 5.5 / 5.6). Cede o
    // processador a cada iteracao em vez de girar em busy-loop.
    const uint32_t start = platform::millis();
    while (gpio_get_level(static_cast<gpio_num_t>(cfg::LoraPins::kAux)) == 0) {
        if (platform::millis() - start >= timeoutMs) {
            return false;
        }
        platform::delayMs(1);
    }
    platform::delayMs(kAuxSettleMs);
    return true;
}

bool E220Radio::writeConfigRegisters() {
    // Monta o bloco de 6 registradores (00H..05H) e escreve com C0 (secao 7.2).
    //   00H ADDH, 01H ADDL, 02H REG0, 03H REG1, 04H REG2(canal), 05H REG3
    const uint8_t reg0 =
        (static_cast<uint8_t>(profile_.uartBaud) << 5) |  // bits[7:5] baud UART
        (0b00 << 3) |                                     // bits[4:3] 8N1
        (static_cast<uint8_t>(profile_.airRate) & 0b111); // bits[2:0] taxa aerea

    const uint8_t reg1 =
        (0b00 << 6) |                                      // subpacote 200 bytes
        (0 << 5) |                                         // RSSI ambiente off
        (static_cast<uint8_t>(profile_.power) & 0b11);     // bits[1:0] potencia

    const uint8_t reg3 =
        ((profile_.appendRssi ? 1 : 0) << 7) |             // byte RSSI no RX
        (0 << 6) |                                         // transmissao transparente
        ((profile_.enableLbt ? 1 : 0) << 4) |              // LBT
        (0b000);                                           // ciclo WOR 500 ms

    const uint8_t payload[] = {
        0xC0, 0x00, 0x06,                                  // set, addr inicial 00H, 6 regs
        static_cast<uint8_t>(profile_.address >> 8),       // 00H ADDH
        static_cast<uint8_t>(profile_.address & 0xFF),     // 01H ADDL
        reg0, reg1,
        profile_.channel,                                  // 04H canal
        reg3,
    };

    uart_flush_input(port_);  // limpa eco anterior
    if (uart_write_bytes(port_, payload, sizeof(payload)) < 0) {
        return false;
    }
    uart_wait_tx_done(port_, pdMS_TO_TICKS(200));

    // Resposta esperada: C1 + mesmo bloco (secao 7.1). Validamos so o primeiro
    // byte; FF FF FF indica formato invalido.
    uint8_t reply = 0;
    const uint32_t start = platform::millis();
    while (platform::millis() - start < 200) {
        if (uart_read_bytes(port_, &reply, 1, 0) == 1) {
            return reply == 0xC1;
        }
        platform::delayMs(1);
    }
    return false;
}

bool E220Radio::send(const uint8_t* data, size_t length) {
    if (length == 0 || length > kMaxRadioFrame) return false;
    if (isBusy()) return false;  // AUX baixo: buffer cheio / TX em andamento
    return uart_write_bytes(port_, data, length) == static_cast<int>(length);
}

void E220Radio::poll() {
    drainUartToRxBuffer();
}

void E220Radio::drainUartToRxBuffer() {
    // Le tudo que ja esta no buffer do driver, sem bloquear (timeout 0).
    size_t pending = 0;
    if (uart_get_buffered_data_len(port_, &pending) != ESP_OK) {
        pending = 0;
    }
    while (pending > 0) {
        uint8_t byte = 0;
        if (uart_read_bytes(port_, &byte, 1, 0) != 1) break;
        --pending;
        if (rxLength_ < sizeof(rxBuffer_)) {
            rxBuffer_[rxLength_++] = byte;
        }
        // Acima da capacidade o byte e descartado; o parser do LinkLayer
        // ressincroniza no proximo SYNC.
        lastByteMs_ = platform::millis();
    }

    // Fecha o quadro quando a UART fica ociosa por kFrameGapMs.
    if (rxLength_ > 0 && !frameReady_ &&
        (platform::millis() - lastByteMs_) >= kFrameGapMs) {
        // Se o byte RSSI foi anexado (REG3 bit7), o ultimo byte e a forca do
        // sinal: RSSI_dBm = -(256 - valor).
        if (profile_.appendRssi && rxLength_ >= 2) {
            const uint8_t raw = rxBuffer_[rxLength_ - 1];
            lastRssiDbm_ = static_cast<int16_t>(raw) - 256;
            rxLength_--;  // remove o byte de RSSI do payload
        }
        frameReady_ = true;
    }
}

bool E220Radio::available() {
    return frameReady_;
}

size_t E220Radio::receive(uint8_t* buffer, size_t bufferSize) {
    if (!frameReady_) return 0;
    const size_t n = (rxLength_ < bufferSize) ? rxLength_ : bufferSize;
    memcpy(buffer, rxBuffer_, n);
    rxLength_ = 0;
    frameReady_ = false;
    return n;
}

int16_t E220Radio::lastRssiDbm() {
    return lastRssiDbm_;
}

bool E220Radio::isBusy() {
    return gpio_get_level(static_cast<gpio_num_t>(cfg::LoraPins::kAux)) == 0;
}

void E220Radio::sleep() {
    // Modo 3 tambem e o modo de sleep profundo (~5 uA). Espera o TX pendente
    // esvaziar; o modulo entra em sleep sozinho em ate 1 ms (secao 6.1, nota 3).
    applyMode(Mode::kConfig);
}

void E220Radio::wake() {
    applyMode(Mode::kNormal);
    waitForReady(1000);
}

}  // namespace hal
