#pragma once
// ============================================================================
//  E220Radio.h  -  Driver EBYTE E220-900T30D / T22D via UART (principal).
// ----------------------------------------------------------------------------
//  Implementa ILoRaRadio para os modulos E220 (LLCC68). Roda em modo NORMAL /
//  transparente na operacao e cai para o modo CONFIG (M0=1, M1=1) somente no
//  begin() para gravar o perfil de RF via comando C0.
//
//  Modelo FreeRTOS: `send` e protegido por mutex (thread-safe) e espera a linha
//  AUX ficar pronta; `read` embrulha uart_read_bytes (bloqueante com timeout),
//  e chamado pela tarefa de RX do LinkLayer. O framing e feito la, de forma
//  estruturada (SYNC + comprimento).
//
//  Plataforma: ESP-IDF (driver/uart + driver/gpio). Nao depende do Arduino.
// ============================================================================

#include <driver/uart.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "config/RadioConfig.h"
#include "hw/ILoRaRadio.h"

namespace hw {

class E220Radio : public ILoRaRadio {
public:
    // `port` deve ser uma UART dedicada (ex.: UART_NUM_1), nao a do console.
    E220Radio(uart_port_t port, const cfg::LoraProfile& profile)
        : port_(port), profile_(profile) {}
    ~E220Radio() override;

    bool begin() override;
    bool send(const uint8_t* data, size_t length) override;
    int  read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) override;
    bool isBusy() override;
    void sleep() override;
    void wake() override;

private:
    // Combinacoes M1 M0 (datasheet, secao 6).
    enum class Mode : uint8_t {
        kNormal  = 0,  // M1=0 M0=0  TX/RX transparente
        kWorSend = 1,  // M1=0 M0=1
        kWorRecv = 2,  // M1=1 M0=0
        kConfig  = 3,  // M1=1 M0=1  deep sleep / registradores
    };

    bool configureGpios();
    bool installUart(uint32_t baud);
    void applyMode(Mode mode);
    bool waitForReady(uint32_t timeoutMs);  // AUX alto == pronto
    bool writeConfigRegisters();            // comando C0, secao 7

    uart_port_t       port_;
    cfg::LoraProfile  profile_;
    Mode              currentMode_ = Mode::kConfig;
    bool              uartInstalled_ = false;
    SemaphoreHandle_t txMutex_ = nullptr;
};

}  // namespace hw
