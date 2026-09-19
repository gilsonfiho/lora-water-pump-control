#pragma once
// ============================================================================
//  E220Radio.h  -  Driver EBYTE E220-900T30D / T22D via UART (principal).
// ----------------------------------------------------------------------------
//  Implementa ILoRaRadio para os modulos E220 baseados no LLCC68 usados neste
//  projeto. Roda o modulo em modo NORMAL / transparente (M0=0, M1=0) durante a
//  operacao e cai para o modo CONFIG (M0=1, M1=1) somente no begin() para
//  enviar o perfil de RF via escrita de registradores C0.
//
//  Contrato nao bloqueante:
//    * poll() drena a UART do modulo para o buffer de quadro RX e acompanha AUX.
//    * send() rejeita enquanto AUX indica ocupado, em vez de ficar girando.
//    * trocas de modo esperam a borda de subida do AUX com timeout cooperativo
//      e limitado (sem esperas longas no caminho critico).
//
//  Nota sobre framing: o E220 e um "cano de bytes". Ele nao delimita quadros,
//  entao a nossa camada protocol/ prefixa um byte SYNC + comprimento; este
//  driver entrega os bytes que chegaram dentro de uma folga entre bytes como um
//  "quadro" e deixa o parser ressincronizar. Ver LinkLayer para o framing real.
//
//  Plataforma: ESP-IDF (driver/uart + driver/gpio). Nao depende do Arduino.
// ============================================================================

#include <driver/uart.h>

#include "hal/ILoRaRadio.h"
#include "config/PinConfig.h"
#include "config/RadioConfig.h"

namespace hal {

class E220Radio : public ILoRaRadio {
public:
    // `port` deve ser uma UART dedicada (ex.: UART_NUM_1), nao a do console.
    // Os pinos vem de cfg::LoraPins. O driver da UART e instalado em begin().
    E220Radio(uart_port_t port, const cfg::LoraProfile& profile);
    ~E220Radio() override;

    bool begin() override;
    bool send(const uint8_t* data, size_t length) override;
    bool available() override;
    size_t receive(uint8_t* buffer, size_t bufferSize) override;
    int16_t lastRssiDbm() override;
    bool isBusy() override;
    void poll() override;
    void sleep() override;
    void wake() override;

private:
    // Combinacoes M1 M0 do datasheet, secao 6 (a tabela da secao 6; os textos
    // das secoes 6.3 e 6.4 do manual se contradizem e estao errados).
    enum class Mode : uint8_t {
        kNormal  = 0,  // M1=0 M0=0  TX/RX transparente
        kWorSend = 1,  // M1=0 M0=1
        kWorRecv = 2,  // M1=1 M0=0
        kConfig  = 3,  // M1=1 M0=1  deep sleep / acesso a registradores
    };

    bool configureGpios();
    bool installUart(uint32_t baud);
    void applyMode(Mode mode);
    bool waitForReady(uint32_t timeoutMs);  // AUX alto == pronto
    bool writeConfigRegisters();            // comando C0, secao 7
    void drainUartToRxBuffer();

    uart_port_t port_;
    cfg::LoraProfile profile_;
    Mode currentMode_ = Mode::kConfig;
    bool uartInstalled_ = false;

    // Montagem de quadro RX unico. Um quadro fecha numa folga de ociosidade
    // entre bytes.
    uint8_t rxBuffer_[kMaxRadioFrame];
    size_t rxLength_ = 0;
    uint32_t lastByteMs_ = 0;
    bool frameReady_ = false;

    int16_t lastRssiDbm_ = 0;
};

}  // namespace hal
