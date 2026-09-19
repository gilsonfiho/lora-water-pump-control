#pragma once
// ============================================================================
//  LinkLayer.h  -  Enlace confiavel ponto-a-ponto sobre ILoRaRadio (FreeRTOS).
// ----------------------------------------------------------------------------
//  Concorrencia:
//    * Uma TASK de RX dedicada le o fluxo do radio, faz o enquadramento
//      (framing) por SYNC + comprimento, decodifica e trata cada pacote.
//    * ACKs recebidos liberam um SEMAFORO que destrava sendReliable().
//    * Pacotes de dados sao entregues por uma FILA (dataQueue) consumida pela
//      tarefa de controle do no.
//    * Pacotes que pedem ACK sao auto-confirmados pela task de RX.
//
//  Modelo de uma transacao confiavel em voo por vez (suficiente p/ 2 nos).
// ============================================================================

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "config/NodeConfig.h"
#include "hw/ILoRaRadio.h"
#include "protocol/Packet.h"

namespace protocol {

class LinkLayer {
public:
    LinkLayer(hw::ILoRaRadio& radio, uint8_t localAddress,
              uint32_t ackTimeoutMs = cfg::Timing::kAckTimeoutMs,
              uint8_t maxRetries = cfg::Timing::kMaxRetries)
        : radio_(radio), localAddress_(localAddress),
          ackTimeoutMs_(ackTimeoutMs), maxRetries_(maxRetries) {}

    // Cria filas/semaforos e sobe a task de RX. false em falha de alocacao.
    bool begin();

    // Envia exigindo ACK; bloqueia ate confirmar ou esgotar as retransmissoes.
    // Deve ser chamado a partir de uma tarefa (nao de ISR).
    bool sendReliable(Packet pkt);

    // Envio "dispare e esqueca" (heartbeat, status). Sem ACK.
    bool sendOnce(Packet pkt);

    // Fila de pacotes de dados recebidos (consumida pela tarefa de controle).
    QueueHandle_t dataQueue() const { return dataQueue_; }

private:
    static void rxTaskTrampoline(void* arg);
    void   rxTaskLoop();
    size_t readFrame(uint8_t* buffer);        // enquadra 1 pacote do radio
    void   handleDecoded(const Packet& pkt);
    void   transmit(const Packet& pkt);

    hw::ILoRaRadio& radio_;
    uint8_t  localAddress_;
    uint32_t ackTimeoutMs_;
    uint8_t  maxRetries_;

    uint8_t  nextSeq_ = 1;
    volatile uint8_t pendingSeq_ = 0;
    volatile bool    hasPending_ = false;

    SemaphoreHandle_t ackSem_    = nullptr;   // liberado quando o ACK chega
    QueueHandle_t     dataQueue_ = nullptr;   // pacotes de dados p/ o controle
    TaskHandle_t      rxTask_    = nullptr;
};

}  // namespace protocol
