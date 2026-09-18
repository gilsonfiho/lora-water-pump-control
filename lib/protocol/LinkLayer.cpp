// ============================================================================
//  LinkLayer.cpp  -  Maquina de enlace nao bloqueante (ACK/retry/timeout).
// ============================================================================

#include "LinkLayer.h"

#include <Arduino.h>

#include "Messages.h"

namespace protocol {

bool LinkLayer::sendReliable(Packet pkt) {
    if (hasPending_) return false;  // uma transacao confiavel por vez

    pkt.src = localAddress_;
    pkt.seq = nextSeq_++;
    pkt.flags |= kFlagAckRequested;
    if (nextSeq_ == 0) nextSeq_ = 1;  // 0 reservado

    pending_ = pkt;
    hasPending_ = true;
    retriesLeft_ = maxRetries_;
    delivery_ = DeliveryResult::kInFlight;
    transmit(pending_);
    return true;
}

bool LinkLayer::sendOnce(Packet pkt) {
    pkt.src = localAddress_;
    pkt.seq = nextSeq_++;
    if (nextSeq_ == 0) nextSeq_ = 1;
    pkt.flags &= ~kFlagAckRequested;
    transmit(pkt);
    return true;
}

void LinkLayer::transmit(const Packet& pkt) {
    uint8_t buffer[kMaxPacketSize];
    const size_t n = encodePacket(pkt, buffer, sizeof(buffer));
    if (n == 0) return;
    radio_.send(buffer, n);
    lastTxMs_ = millis();
}

void LinkLayer::poll() {
    radio_.poll();
    processIncoming();

    // Retransmissao / timeout da transacao confiavel corrente.
    if (hasPending_ && (millis() - lastTxMs_) >= ackTimeoutMs_) {
        if (retriesLeft_ == 0) {
            hasPending_ = false;
            delivery_ = DeliveryResult::kFailed;
            return;
        }
        --retriesLeft_;
        transmit(pending_);  // reenvia com o MESMO SEQ
    }
}

void LinkLayer::processIncoming() {
    if (!radio_.available()) return;

    const size_t n = radio_.receive(rxScratch_, sizeof(rxScratch_));
    if (n < kHeaderSize + kCrcSize) return;

    // Procura o SYNC e tenta decodificar a partir dele (ressincronizacao).
    for (size_t i = 0; i + kHeaderSize + kCrcSize <= n; ++i) {
        if (rxScratch_[i] != kSyncByte) continue;
        Packet pkt;
        if (decodePacket(rxScratch_ + i, n - i, pkt)) {
            handleDecoded(pkt);
            return;
        }
    }
}

void LinkLayer::handleDecoded(const Packet& pkt) {
    // Ignora o que nao e para este no (aceita broadcast).
    if (pkt.dst != localAddress_ && pkt.dst != cfg::kAddrBroadcast) return;

    // Um ACK conclui a transacao pendente se o SEQ casar.
    if (pkt.isAck()) {
        if (hasPending_ && pkt.seq == pending_.seq) {
            hasPending_ = false;
            delivery_ = DeliveryResult::kAcked;
        }
        return;
    }

    // Pacote de dados que pede ACK -> responde automaticamente.
    if (pkt.wantsAck()) {
        transmit(makeAck(localAddress_, pkt.src, pkt.seq));
    }

    if (handler_) handler_(pkt);
}

}  // namespace protocol
