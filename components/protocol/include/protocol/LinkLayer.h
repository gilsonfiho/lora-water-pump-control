#pragma once
// ============================================================================
//  LinkLayer.h  -  Enlace confiavel ponto-a-ponto sobre ILoRaRadio.
// ----------------------------------------------------------------------------
//  Responsabilidades:
//    * Framing: encontra o byte SYNC no fluxo do radio e decodifica pacotes.
//    * Enderecamento: descarta pacotes que nao sao para este no.
//    * Confiabilidade: envio com ACK, retransmissao e timeout (nao bloqueante).
//    * Auto-ACK: responde automaticamente pacotes que pedem confirmacao.
//
//  Modelo de uma transacao confiavel em voo por vez -- suficiente para o
//  enlace ponto-a-ponto de 2 nos e mantem a maquina de estados simples.
// ============================================================================

#include <functional>

#include "protocol/Packet.h"
#include "config/NodeConfig.h"
#include "hw/ILoRaRadio.h"

namespace protocol {

// Evento de diagnostico do enlace. Emitido por std::function (nao por
// core::Signal) porque protocol/ nao pode depender de core/ -- core ja depende
// de protocol, e a seta inversa criaria dependencia circular. Quem loga (com
// ESP_LOG) e a camada de app (reservoir_app / pump_app); protocol/ segue sem
// conhecer o ESP-IDF.
struct LinkEvent {
    enum class Type : uint8_t {
        kTx,        // comando confiavel transmitido
        kAckOk,     // ACK recebido para o SEQ pendente
        kRetry,     // retransmissao por timeout
        kTimeout,   // esgotou as retransmissoes -> entrega falhou
        kCrcFail,   // achou SYNC mas o quadro nao decodificou (CRC/truncado)
        kNotForMe,  // pacote valido, mas endereçado a outro no
    };
    Type     type;
    uint8_t  seq         = 0;
    uint8_t  dst         = 0;   // kTx / kNotForMe
    uint8_t  bytes       = 0;   // kTx: tamanho do quadro no ar
    uint8_t  retriesLeft = 0;   // kRetry
    uint32_t rttMs       = 0;   // kAckOk
};

class LinkLayer {
public:
    using ReceiveHandler = std::function<void(const Packet&)>;
    using EventHandler   = std::function<void(const LinkEvent&)>;

    enum class DeliveryResult : uint8_t { kIdle, kInFlight, kAcked, kFailed };

    LinkLayer(hw::ILoRaRadio& radio, uint8_t localAddress,
              uint32_t ackTimeoutMs = cfg::Timing::kAckTimeoutMs,
              uint8_t maxRetries = cfg::Timing::kMaxRetries)
        : radio_(radio), localAddress_(localAddress),
          ackTimeoutMs_(ackTimeoutMs), maxRetries_(maxRetries) {}

    void onReceive(ReceiveHandler handler) { handler_ = std::move(handler); }

    // Assina os eventos de diagnostico do enlace (TX/ACK/retry/timeout/etc.).
    void onEvent(EventHandler handler) { eventHandler_ = std::move(handler); }

    // Envia exigindo ACK. Atribui o proximo SEQ, inicia o rastreamento de
    // retransmissao. Retorna false se ja ha uma transacao confiavel em voo.
    bool sendReliable(Packet pkt);

    // Envio "dispare e esqueca" (heartbeat, status). Sem ACK.
    bool sendOnce(Packet pkt);

    // Aciona radio.poll(), processa RX e cuida de timeouts/retransmissoes.
    void poll();

    bool isDelivering() const { return delivery_ == DeliveryResult::kInFlight; }
    DeliveryResult deliveryState() const { return delivery_; }

private:
    void processIncoming();
    void handleDecoded(const Packet& pkt);
    void transmit(const Packet& pkt);
    void emitEvent(const LinkEvent& event) const {
        if (eventHandler_) eventHandler_(event);
    }

    hw::ILoRaRadio& radio_;
    uint8_t localAddress_;
    uint32_t ackTimeoutMs_;
    uint8_t maxRetries_;

    ReceiveHandler handler_;
    EventHandler   eventHandler_;
    uint8_t nextSeq_ = 1;

    // Estado da transacao confiavel corrente.
    Packet pending_;
    bool hasPending_ = false;
    uint8_t retriesLeft_ = 0;
    uint32_t lastTxMs_ = 0;
    DeliveryResult delivery_ = DeliveryResult::kIdle;

    uint8_t rxScratch_[hw::kMaxRadioFrame];
};

}  // namespace protocol
