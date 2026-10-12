// ============================================================================
//  LinkLayer.cpp  -  Maquina de enlace nao bloqueante (ACK/retry/timeout).
//  Autentica cada pacote (MAC) e rejeita reproducao (contador). Ver #5 e #7.
// ============================================================================

#include "protocol/LinkLayer.h"

#include "platform/Clock.h"
#include "platform/Nonce.h"

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
    lastTxMs_ = platform::millis();  // marca o inicio da espera pelo ACK

    LinkEvent ev{};
    ev.type = LinkEvent::Type::kTx;
    ev.seq = pending_.seq;
    ev.dst = pending_.dst;
    ev.bytes = static_cast<uint8_t>(kHeaderSize + pending_.payloadLen + kMacSize + kCrcSize);
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

void LinkLayer::transmit(Packet pkt) {
    // Nonce FRESCO a cada transmissao fisica (inclusive retransmissao e
    // auto-ACK): assim toda saida legitima tem contador estritamente crescente
    // e nunca cai como falso replay no receptor. Comandos sao idempotentes
    // (ligar/desligar), entao reentregar uma retransmissao e inofensivo.
    pkt.counter = platform::nextNonce();

    uint8_t buffer[kMaxPacketSize];
    const size_t n = encodePacket(pkt, key_, buffer, sizeof(buffer));
    if (n == 0) return;
    radio_.send(buffer, n);

    // NAO toca em lastTxMs_: esse relogio pertence a transacao confiavel em
    // voo. Quem o atualiza e sendReliable() e a retransmissao em poll(). Se
    // qualquer TX o atualizasse, um heartbeat (sendOnce) ou um auto-ACK
    // adiaria a retransmissao do comando pendente -- no limite, indefinidamente.
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
        transmit(pending_);  // reenvia com o MESMO SEQ (contador novo)
        lastTxMs_ = platform::millis();
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
    const size_t minFrame = kHeaderSize + kMacSize + kCrcSize;
    if (n < minFrame) return;

    // Procura o SYNC e tenta decodificar a partir dele (ressincronizacao).
    bool sawSync = false;
    DecodeError lastErr = DecodeError::kFraming;
    for (size_t i = 0; i + minFrame <= n; ++i) {
        if (rxScratch_[i] != kSyncByte) continue;
        sawSync = true;
        Packet pkt;
        DecodeError err = DecodeError::kNone;
        if (decodePacket(rxScratch_ + i, n - i, key_, pkt, &err)) {
            handleDecoded(pkt);
            return;
        }
        lastErr = err;
        // CRC ou MAC falhando significa que achamos um quadro plausivel que
        // nao confere -- continuar varrendo bytes dentro dele nao acha um
        // valido. So seguimos se foi erro de framing (SYNC falso no ruido).
        if (err == DecodeError::kCrc || err == DecodeError::kMac) break;
    }

    if (!sawSync) return;
    LinkEvent ev{};
    ev.type = (lastErr == DecodeError::kMac) ? LinkEvent::Type::kAuthFail
                                             : LinkEvent::Type::kCrcFail;
    emitEvent(ev);
}

bool LinkLayer::acceptCounter(uint8_t src, uint32_t counter) {
    for (auto& e : rxCounters_) {
        if (e.used && e.src == src) {
            if (counter <= e.counter) return false;  // reproducao
            e.counter = counter;
            return true;
        }
    }
    for (auto& e : rxCounters_) {
        if (!e.used) {  // primeira vez vendo esta origem
            e.used = true;
            e.src = src;
            e.counter = counter;
            return true;
        }
    }
    return true;  // tabela cheia (nao ocorre em ponto a ponto): nao bloqueia
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

    // O MAC ja provou autenticidade/integridade (decodePacket). Falta o
    // frescor: contador <= ultimo visto desta origem = reproducao, descarta.
    if (!acceptCounter(pkt.src, pkt.counter)) {
        LinkEvent ev{};
        ev.type = LinkEvent::Type::kReplay;
        ev.src = pkt.src;
        ev.seq = pkt.seq;
        ev.counter = pkt.counter;
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
