#pragma once
// ============================================================================
//  ILoRaRadio.h  -  Interface de radio LoRa agnostica ao transporte (Strategy).
// ----------------------------------------------------------------------------
//  Estrategias concretas:
//    - E220Radio   : EBYTE E220-900T30D / T22D via UART (hardware principal)
//    - SX127xRadio : Semtech SX1276/RFM95 via SPI       (alternativa, esqueleto)
//
//  Modelo orientado a FreeRTOS: `send` e thread-safe e bloqueia ate o modulo
//  estar pronto; `read` bloqueia ate haver bytes ou expirar o timeout, no
//  estilo de uart_read_bytes. O enquadramento (framing) fica na camada
//  protocol/ (LinkLayer, na sua tarefa de RX), mantendo o radio como um
//  transporte de bytes.
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace hw {

// Maior payload de quadro unico que o transporte carrega de uma vez.
static constexpr size_t kMaxRadioFrame = 200;

class ILoRaRadio {
public:
    virtual ~ILoRaRadio() = default;

    // Inicializa o radio e envia o perfil de RF. false em falha de hardware.
    virtual bool begin() = 0;

    // Envia um quadro. Thread-safe (mutex interno). Bloqueia ate o modulo
    // indicar pronto (AUX) ou expirar o timeout interno; false se nao conseguiu.
    virtual bool send(const uint8_t* data, size_t length) = 0;

    // Le ate maxLen bytes do fluxo de recepcao, bloqueando ate haver dados ou
    // expirar timeoutMs. Retorna o numero de bytes lidos (0 no timeout).
    virtual int read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) = 0;

    // True enquanto o radio nao pode transmitir (ex.: AUX baixo no E220).
    virtual bool isBusy() = 0;

    // Estado de menor consumo suportado; o proximo wake() restaura a operacao.
    virtual void sleep() = 0;
    virtual void wake() = 0;
};

}  // namespace hw
