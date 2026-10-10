// ============================================================================
//  LinkLayer.cpp  -  Maquina de enlace nao bloqueante (ACK/retry/timeout).
// ============================================================================

#include "protocol/LinkLayer.h"

#include "platform/Clock.h"

#include "protocol/Messages.h"

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

    LinkEvent ev{};
    ev.type = LinkEvent::Type::kTx;
    ev.seq = pending_.seq;
    ev.dst = pending_.dst;
    ev.bytes = static_cast<uint8_t>(kHeaderSize + pending_.payloadLen + kCrcSize);
    ev.retriesLeft = retriesLeft_;
    emitEvent(ev);
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
    lastTxMs_ = platform::millis();
}

void LinkLayer::poll() {
    radio_.poll();
    processIncoming();

    // Retransmissao / timeout da transacao confiavel corrente.
    if (hasPending_ && (platform::millis() - lastTxMs_) >= ackTimeoutMs_) {
        if (retriesLeft_ == 0) {
            hasPending_ = false;
            delivery_ = DeliveryResult::kFailed;
            LinkEvent ev{};
            ev.type = LinkEvent::Type::kTimeout;
            ev.seq = pending_.seq;
            emitEvent(ev);
            return;
        }
        --retriesLeft_;
        transmit(pending_);  // reenvia com o MESMO SEQ
        LinkEvent ev{};
        ev.type = LinkEvent::Type::kRetry;
        ev.seq = pending_.seq;
        ev.retriesLeft = retriesLeft_;
        emitEvent(ev);
    }
}

void LinkLayer::processIncoming() {
    if (!radio_.available()) return;

    const size_t n = radio_.receive(rxScratch_, sizeof(rxScratch_));
    if (n < kHeaderSize + kCrcSize) return;

    // Procura o SYNC e tenta decodificar a partir dele (ressincronizacao).
    bool sawSync = false;
    for (size_t i = 0; i + kHeaderSize + kCrcSize <= n; ++i) {
        if (rxScratch_[i] != kSyncByte) continue;
        sawSync = true;
        Packet pkt;
        if (decodePacket(rxScratch_ + i, n - i, pkt)) {
            handleDecoded(pkt);
            return;
        }
    }

    // Chegou algo com SYNC mas nada decodificou: CRC ruim ou quadro truncado
    // (tipico de saturacao do RX ou colisao), nao apenas ruido sem sincronismo.
    if (sawSync) {
        LinkEvent ev{};
        ev.type = LinkEvent::Type::kCrcFail;
        emitEvent(ev);
    }
}

void LinkLayer::handleDecoded(const Packet& pkt) {
    // Ignora o que nao e para este no (aceita broadcast).
    if (pkt.dst != localAddress_ && pkt.dst != cfg::kAddrBroadcast) {
        LinkEvent ev{};
        ev.type = LinkEvent::Type::kNotForMe;
        ev.seq = pkt.seq;
        ev.dst = pkt.dst;
        emitEvent(ev);
        return;
    }

    // Um ACK conclui a transacao pendente se o SEQ casar.
    if (pkt.isAck()) {
        if (hasPending_ && pkt.seq == pending_.seq) {
            hasPending_ = false;
            delivery_ = DeliveryResult::kAcked;
            LinkEvent ev{};
            ev.type = LinkEvent::Type::kAckOk;
            ev.seq = pkt.seq;
            ev.rttMs = platform::millis() - lastTxMs_;
            emitEvent(ev);
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
