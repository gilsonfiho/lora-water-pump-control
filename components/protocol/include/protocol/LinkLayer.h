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
#include "hal/ILoRaRadio.h"

namespace protocol {

class LinkLayer {
public:
    using ReceiveHandler = std::function<void(const Packet&)>;

    enum class DeliveryResult : uint8_t { kIdle, kInFlight, kAcked, kFailed };

    LinkLayer(hal::ILoRaRadio& radio, uint8_t localAddress,
              uint32_t ackTimeoutMs = cfg::Timing::kAckTimeoutMs,
              uint8_t maxRetries = cfg::Timing::kMaxRetries)
        : radio_(radio), localAddress_(localAddress),
          ackTimeoutMs_(ackTimeoutMs), maxRetries_(maxRetries) {}

    void onReceive(ReceiveHandler handler) { handler_ = std::move(handler); }

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

    hal::ILoRaRadio& radio_;
    uint8_t localAddress_;
    uint32_t ackTimeoutMs_;
    uint8_t maxRetries_;

    ReceiveHandler handler_;
    uint8_t nextSeq_ = 1;

    // Estado da transacao confiavel corrente.
    Packet pending_;
    bool hasPending_ = false;
    uint8_t retriesLeft_ = 0;
    uint32_t lastTxMs_ = 0;
    DeliveryResult delivery_ = DeliveryResult::kIdle;

    uint8_t rxScratch_[hal::kMaxRadioFrame];
};

}  // namespace protocol
