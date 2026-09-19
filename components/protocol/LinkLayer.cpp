// ============================================================================
//  LinkLayer.cpp  -  Enlace nao bloqueante com task de RX, fila e semaforo.
// ============================================================================

#include "protocol/LinkLayer.h"

#include <esp_log.h>

#include "protocol/Messages.h"

namespace protocol {

namespace {
constexpr char TAG[] = "LINK";
constexpr uint32_t kHuntMs  = 200;   // espera por bytes ao procurar o SYNC
constexpr uint32_t kFrameMs = 200;   // espera pelo restante do quadro
constexpr int      kDataQueueDepth = 8;
}  // namespace

bool LinkLayer::begin() {
    ackSem_ = xSemaphoreCreateBinary();
    dataQueue_ = xQueueCreate(kDataQueueDepth, sizeof(Packet));
    if (ackSem_ == nullptr || dataQueue_ == nullptr) return false;

    const BaseType_t ok = xTaskCreate(
        &LinkLayer::rxTaskTrampoline, "link_rx",
        cfg::Tasks::kRadioRxStack, this, cfg::Tasks::kRadioRxPrio, &rxTask_);
    return ok == pdPASS;
}

void LinkLayer::rxTaskTrampoline(void* arg) {
    static_cast<LinkLayer*>(arg)->rxTaskLoop();
}

void LinkLayer::rxTaskLoop() {
    uint8_t buffer[kMaxPacketSize];
    for (;;) {
        const size_t n = readFrame(buffer);
        if (n == 0) continue;  // sem quadro / timeout / desalinhado

        Packet pkt;
        if (decodePacket(buffer, n, pkt)) {
            handleDecoded(pkt);
        }
    }
}

size_t LinkLayer::readFrame(uint8_t* buffer) {
    // 1) Caca o byte de SYNC no fluxo.
    uint8_t b = 0;
    bool synced = false;
    for (int i = 0; i < static_cast<int>(hw::kMaxRadioFrame); ++i) {
        if (radio_.read(&b, 1, kHuntMs) != 1) return 0;  // sem dados
        if (b == kSyncByte) { synced = true; break; }
    }
    if (!synced) return 0;
    buffer[0] = kSyncByte;

    // 2) Restante do cabecalho (VER..LEN).
    if (radio_.read(buffer + 1, kHeaderSize - 1, kFrameMs) !=
        static_cast<int>(kHeaderSize - 1)) {
        return 0;
    }
    if (buffer[1] != kProtocolVer) return 0;

    const uint8_t payloadLen = buffer[7];
    if (payloadLen > kMaxPayload) return 0;

    // 3) Payload + CRC.
    const size_t rest = payloadLen + kCrcSize;
    if (radio_.read(buffer + kHeaderSize, rest, kFrameMs) !=
        static_cast<int>(rest)) {
        return 0;
    }
    return kHeaderSize + rest;
}

void LinkLayer::handleDecoded(const Packet& pkt) {
    // Ignora o que nao e para este no (aceita broadcast).
    if (pkt.dst != localAddress_ && pkt.dst != cfg::kAddrBroadcast) return;

    // Um ACK conclui a transacao pendente.
    if (pkt.isAck()) {
        if (hasPending_ && pkt.seq == pendingSeq_) {
            xSemaphoreGive(ackSem_);
        }
        return;
    }

    // Pacote de dados que pede ACK -> confirma automaticamente.
    if (pkt.wantsAck()) {
        transmit(makeAck(localAddress_, pkt.src, pkt.seq));
    }

    if (xQueueSend(dataQueue_, &pkt, 0) != pdTRUE) {
        ESP_LOGW(TAG, "fila de dados cheia; pacote descartado");
    }
}

void LinkLayer::transmit(const Packet& pkt) {
    uint8_t buffer[kMaxPacketSize];
    const size_t n = encodePacket(pkt, buffer, sizeof(buffer));
    if (n == 0) return;
    radio_.send(buffer, n);
}

bool LinkLayer::sendReliable(Packet pkt) {
    pkt.src = localAddress_;
    pkt.seq = nextSeq_++;
    if (nextSeq_ == 0) nextSeq_ = 1;  // 0 reservado
    pkt.flags |= kFlagAckRequested;

    pendingSeq_ = pkt.seq;
    hasPending_ = true;
    xSemaphoreTake(ackSem_, 0);  // drena ACK antigo

    bool acked = false;
    for (uint8_t attempt = 0; attempt <= maxRetries_ && !acked; ++attempt) {
        transmit(pkt);
        if (xSemaphoreTake(ackSem_, pdMS_TO_TICKS(ackTimeoutMs_)) == pdTRUE) {
            acked = true;
        }
    }

    hasPending_ = false;
    if (!acked) ESP_LOGW(TAG, "sem ACK apos %d tentativas", maxRetries_ + 1);
    return acked;
}

bool LinkLayer::sendOnce(Packet pkt) {
    pkt.src = localAddress_;
    pkt.seq = nextSeq_++;
    if (nextSeq_ == 0) nextSeq_ = 1;
    pkt.flags &= ~kFlagAckRequested;
    transmit(pkt);
    return true;
}

}  // namespace protocol
