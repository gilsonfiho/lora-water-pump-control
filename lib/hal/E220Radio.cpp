// ============================================================================
//  E220Radio.cpp  -  Implementacao do driver UART do E220 (secoes 5, 6 e 7 do
//                    datasheet EBYTE E220-900T30D / T22D).
// ============================================================================

#include "E220Radio.h"

#include <cstring>

namespace hal {

namespace {
// Folga de ociosidade entre bytes que fecha um quadro RX. Em 9600 8N1 um byte
// leva ~1 ms; 12 ms de silencio indica que o modulo terminou de entregar o
// quadro atual pela UART.
constexpr uint32_t kFrameGapMs = 12;

// Apos AUX subir, o datasheet pede ~2 ms de estabilidade antes de operar no
// novo modo (secao 6.1, nota 3).
constexpr uint32_t kAuxSettleMs = 3;

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

E220Radio::E220Radio(HardwareSerial& serial, const cfg::LoraProfile& profile)
    : serial_(serial), profile_(profile) {}

bool E220Radio::begin() {
    pinMode(cfg::LoraPins::kM0, OUTPUT);
    pinMode(cfg::LoraPins::kM1, OUTPUT);
    pinMode(cfg::LoraPins::kAux, INPUT_PULLUP);

    // Entra em CONFIG a 9600 8N1 para escrever os registradores.
    serial_.begin(cfg::Radio::kConfigBaud, SERIAL_8N1,
                  cfg::LoraPins::kUartRx, cfg::LoraPins::kUartTx);
    applyMode(Mode::kConfig);
    if (!waitForReady(1000)) {
        return false;  // modulo nao respondeu ao power-on self-check
    }

    if (!writeConfigRegisters()) {
        return false;
    }

    // Reabre a UART na baud de operacao e volta ao modo transparente.
    serial_.updateBaudRate(uartBaudToBps(profile_.uartBaud));
    applyMode(Mode::kNormal);
    return waitForReady(1000);
}

void E220Radio::applyMode(Mode mode) {
    // M1 = bit alto, M0 = bit baixo do valor de modo.
    const uint8_t bits = static_cast<uint8_t>(mode);
    digitalWrite(cfg::LoraPins::kM1, (bits & 0b10) ? HIGH : LOW);
    digitalWrite(cfg::LoraPins::kM0, (bits & 0b01) ? HIGH : LOW);
    currentMode_ = mode;
}

bool E220Radio::waitForReady(uint32_t timeoutMs) {
    // Espera cooperativa pela borda de subida do AUX (secao 5.5 / 5.6). Nao ha
    // yield/delay longo: apenas cede o processador brevemente.
    const uint32_t start = millis();
    while (digitalRead(cfg::LoraPins::kAux) == LOW) {
        if (millis() - start >= timeoutMs) {
            return false;
        }
        delay(1);
    }
    delay(kAuxSettleMs);
    return true;
}

bool E220Radio::writeConfigRegisters() {
    // Monta o bloco de 7 registradores (00H..05H) e escreve com C0 (secao 7.2).
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

    while (serial_.available()) serial_.read();  // limpa eco anterior
    serial_.write(payload, sizeof(payload));
    serial_.flush();

    // Resposta esperada: C1 + mesmo bloco (secao 7.1). Validamos so o cabecalho.
    const uint32_t start = millis();
    while (serial_.available() < 3) {
        if (millis() - start >= 200) return false;
        delay(1);
    }
    return serial_.read() == 0xC1;
}

bool E220Radio::send(const uint8_t* data, size_t length) {
    if (length == 0 || length > kMaxRadioFrame) return false;
    if (isBusy()) return false;  // AUX baixo: buffer cheio / TX em andamento
    serial_.write(data, length);
    return true;
}

void E220Radio::poll() {
    drainUartToRxBuffer();
}

void E220Radio::drainUartToRxBuffer() {
    while (serial_.available() > 0) {
        if (rxLength_ < sizeof(rxBuffer_)) {
            rxBuffer_[rxLength_++] = static_cast<uint8_t>(serial_.read());
        } else {
            serial_.read();  // descarta overflow; parser ressincroniza no SYNC
        }
        lastByteMs_ = millis();
    }

    // Fecha o quadro quando a UART fica ociosa por kFrameGapMs.
    if (rxLength_ > 0 && !frameReady_ &&
        (millis() - lastByteMs_) >= kFrameGapMs) {
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
    return digitalRead(cfg::LoraPins::kAux) == LOW;
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
