#pragma once
// ============================================================================
//  ILoRaRadio.h  -  Interface de radio LoRa agnostica ao transporte (Strategy).
// ----------------------------------------------------------------------------
//  Estrategias concretas:
//    - E220Radio   : EBYTE E220-900T30D / T22D via UART (hardware principal)
//    - SX127xRadio : Semtech SX1276/RFM95 via SPI       (alternativa)
//
//  A interface e propositalmente orientada a bytes e nao bloqueante. As
//  camadas superiores (protocol/) fazem framing, enderecamento e confirmacao;
//  o radio apenas move bytes opacos e reporta eventos de enlace.
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace hal {

// Maior payload de quadro unico que o transporte carrega de uma vez. Quadros
// maiores devem ser divididos pelo chamador (nossos pacotes sao bem menores).
static constexpr size_t kMaxRadioFrame = 200;

class ILoRaRadio {
public:
    virtual ~ILoRaRadio() = default;

    // Inicializa o radio e envia o perfil de RF. Retorna false em qualquer
    // falha de hardware/handshake, para o chamador entrar em estado seguro.
    virtual bool begin() = 0;

    // Enfileira um quadro para transmissao. Nao bloqueante: retorna true se o
    // quadro foi aceito no caminho de TX, false se o radio esta ocupado.
    virtual bool send(const uint8_t* data, size_t length) = 0;

    // Indica se ha um quadro completo recebido aguardando leitura.
    virtual bool available() = 0;

    // Copia o proximo quadro recebido para buffer (ate bufferSize). Retorna o
    // numero de bytes escritos, ou 0 se nada estava disponivel.
    virtual size_t receive(uint8_t* buffer, size_t bufferSize) = 0;

    // Intensidade do sinal do quadro recebido mais recente, em dBm.
    virtual int16_t lastRssiDbm() = 0;

    // True enquanto o radio nao pode aceitar novo TX (ex.: AUX do E220 baixo).
    virtual bool isBusy() = 0;

    // Aciona as maquinas de estado internas. Deve ser chamado com frequencia
    // no loop(); nunca bloqueia. Separado de receive() para que os drivers
    // possam atender buffers UART, transicoes de AUX e conclusao de TX de
    // forma independente do chamador.
    virtual void poll() = 0;

    // Entra no estado de menor consumo que o transporte suporta (deep sleep do
    // E220, sleep do SX127x). O proximo begin()/wake() restaura a operacao.
    virtual void sleep() = 0;
    virtual void wake() = 0;
};

}  // namespace hal
