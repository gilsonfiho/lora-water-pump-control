// ============================================================================
//  test_link_layer.cpp  -  ACK, retransmissao, timeout, auto-ACK e filtro de
//                          endereco, com radio falso e relogio controlado.
// ============================================================================

#include <unity.h>

#include <vector>

#include "config/NodeConfig.h"
#include "config/SecurityConfig.h"
#include "fakes.h"
#include "protocol/LinkLayer.h"
#include "protocol/Messages.h"

using namespace protocol;
using test::FakeClock;
using test::FakeRadio;

namespace {

constexpr uint8_t kMe   = cfg::kAddrPump;        // 0x02
constexpr uint8_t kPeer = cfg::kAddrReservoir;   // 0x01

const uint8_t* key() { return cfg::Security::kLinkKey; }

// Decodifica o quadro que o radio transmitiu, para inspecionar o que foi ao ar.
Packet lastSent(const FakeRadio& radio) {
    Packet p;
    const std::vector<uint8_t>& frame = radio.sent.back();
    TEST_ASSERT_TRUE(decodePacket(frame.data(), frame.size(), key(), p));
    return p;
}

// Injeta um pacote autenticado (chave real), com um contador explicito para os
// testes de frescor/replay.
void injectPacket(FakeRadio& radio, const Packet& pkt, uint32_t counter = 1) {
    Packet p = pkt;
    p.counter = counter;
    uint8_t wire[kMaxPacketSize];
    const size_t n = encodePacket(p, key(), wire, sizeof(wire));
    radio.inject(wire, n);
}

}  // namespace

TEST_CASE("sendReliable transmite, numera o SEQ e fica em voo", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    std::vector<LinkEvent> events;
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    TEST_ASSERT_TRUE(link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn)));
    TEST_ASSERT_EQUAL_UINT32(1, radio.sent.size());
    TEST_ASSERT_TRUE(link.isDelivering());

    const Packet onAir = lastSent(radio);
    TEST_ASSERT_EQUAL_HEX8(kMe, onAir.src);      // src vem do endereco local
    TEST_ASSERT_NOT_EQUAL(0, onAir.seq);         // SEQ 0 e reservado
    TEST_ASSERT_TRUE(onAir.wantsAck());

    TEST_ASSERT_EQUAL_UINT32(1, events.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kTx),
                            static_cast<uint8_t>(events[0].type));
    FakeClock::uninstall();
}

TEST_CASE("so uma transacao confiavel em voo por vez", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    TEST_ASSERT_TRUE(link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn)));
    TEST_ASSERT_FALSE(link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOff)));
    TEST_ASSERT_EQUAL_UINT32(1, radio.sent.size());
    FakeClock::uninstall();
}

TEST_CASE("ACK com o SEQ certo conclui a entrega", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    std::vector<LinkEvent> events;
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));
    const uint8_t seq = lastSent(radio).seq;

    FakeClock::advance(120);  // tempo de ar de ida e volta
    injectPacket(radio, makeAck(kPeer, kMe, seq));
    link.poll();

    TEST_ASSERT_FALSE(link.isDelivering());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkLayer::DeliveryResult::kAcked),
                            static_cast<uint8_t>(link.deliveryState()));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kAckOk),
                            static_cast<uint8_t>(events.back().type));
    TEST_ASSERT_EQUAL_UINT32(120, events.back().rttMs);
    FakeClock::uninstall();
}

TEST_CASE("ACK com SEQ errado nao conclui a entrega", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));
    const uint8_t seq = lastSent(radio).seq;

    injectPacket(radio, makeAck(kPeer, kMe, static_cast<uint8_t>(seq + 7)));
    link.poll();

    TEST_ASSERT_TRUE(link.isDelivering());
    FakeClock::uninstall();
}

TEST_CASE("sem ACK: retransmite com o mesmo SEQ e falha ao esgotar", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    std::vector<LinkEvent> events;
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));
    const uint8_t seq = lastSent(radio).seq;

    // Cada estouro do timeout gera uma retransmissao, ate kMaxRetries.
    for (uint8_t i = 0; i < cfg::Timing::kMaxRetries; ++i) {
        FakeClock::advance(cfg::Timing::kAckTimeoutMs);
        link.poll();
        TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kRetry),
                                static_cast<uint8_t>(events.back().type));
        TEST_ASSERT_EQUAL_UINT8(seq, events.back().seq);   // mesmo SEQ
        TEST_ASSERT_EQUAL_UINT8(seq, lastSent(radio).seq);
        TEST_ASSERT_TRUE(link.isDelivering());
    }

    // 1 envio original + kMaxRetries retransmissoes.
    TEST_ASSERT_EQUAL_UINT32(1u + cfg::Timing::kMaxRetries, radio.sent.size());

    FakeClock::advance(cfg::Timing::kAckTimeoutMs);
    link.poll();

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kTimeout),
                            static_cast<uint8_t>(events.back().type));
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkLayer::DeliveryResult::kFailed),
                            static_cast<uint8_t>(link.deliveryState()));
    TEST_ASSERT_FALSE(link.isDelivering());
    FakeClock::uninstall();
}

TEST_CASE("nao retransmite antes do timeout", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));
    FakeClock::advance(cfg::Timing::kAckTimeoutMs - 1);
    link.poll();

    TEST_ASSERT_EQUAL_UINT32(1, radio.sent.size());
    FakeClock::uninstall();
}

TEST_CASE("pacote que pede ACK e confirmado automaticamente", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    int received = 0;
    link.onReceive([&](const Packet&) { ++received; });

    Packet cmd = makeCommand(kPeer, kMe, 33, PumpCommand::kOn);
    injectPacket(radio, cmd);
    link.poll();

    TEST_ASSERT_EQUAL_INT(1, received);
    TEST_ASSERT_EQUAL_UINT32(1, radio.sent.size());

    const Packet ack = lastSent(radio);
    TEST_ASSERT_TRUE(ack.isAck());
    TEST_ASSERT_EQUAL_UINT8(33, ack.seq);     // confirma o SEQ recebido
    TEST_ASSERT_EQUAL_HEX8(kPeer, ack.dst);
    FakeClock::uninstall();
}

TEST_CASE("heartbeat nao gera ACK", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    int received = 0;
    link.onReceive([&](const Packet&) { ++received; });

    injectPacket(radio, makeHeartbeat(kPeer, kMe, 4));
    link.poll();

    TEST_ASSERT_EQUAL_INT(1, received);
    TEST_ASSERT_EQUAL_UINT32(0, radio.sent.size());
    FakeClock::uninstall();
}

TEST_CASE("pacote para outro no e descartado", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    int received = 0;
    std::vector<LinkEvent> events;
    link.onReceive([&](const Packet&) { ++received; });
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    injectPacket(radio, makeHeartbeat(kPeer, 0x7E, 1));  // destino alheio
    link.poll();

    TEST_ASSERT_EQUAL_INT(0, received);
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kNotForMe),
                            static_cast<uint8_t>(events.back().type));
    FakeClock::uninstall();
}

TEST_CASE("broadcast e aceito", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    int received = 0;
    link.onReceive([&](const Packet&) { ++received; });

    injectPacket(radio, makeHeartbeat(kPeer, cfg::kAddrBroadcast, 1));
    link.poll();

    TEST_ASSERT_EQUAL_INT(1, received);
    FakeClock::uninstall();
}

TEST_CASE("quadro com SYNC e CRC ruim emite kCrcFail", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    std::vector<LinkEvent> events;
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    uint8_t wire[kMaxPacketSize];
    const size_t n = encodePacket(makeHeartbeat(kPeer, kMe, 1), key(), wire, sizeof(wire));
    wire[n - 1] ^= 0xFF;  // estraga o CRC
    radio.inject(wire, n);
    link.poll();

    TEST_ASSERT_EQUAL_UINT32(1, events.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kCrcFail),
                            static_cast<uint8_t>(events[0].type));
    FakeClock::uninstall();
}

TEST_CASE("ruido sem SYNC e ignorado em silencio", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    std::vector<LinkEvent> events;
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    const uint8_t noise[12] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55,
                               0x66, 0x77, 0x88, 0x99, 0xAA, 0xBB};
    radio.inject(noise, sizeof(noise));
    link.poll();

    TEST_ASSERT_EQUAL_UINT32(0, events.size());
    FakeClock::uninstall();
}

TEST_CASE("lixo antes do SYNC nao impede a decodificacao", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    int received = 0;
    link.onReceive([&](const Packet&) { ++received; });

    uint8_t wire[kMaxPacketSize + 3];
    wire[0] = 0xDE; wire[1] = 0xAD; wire[2] = 0xBE;  // ressincronizacao
    const size_t n = encodePacket(makeHeartbeat(kPeer, kMe, 9), key(), wire + 3,
                                  sizeof(wire) - 3);
    radio.inject(wire, n + 3);
    link.poll();

    TEST_ASSERT_EQUAL_INT(1, received);
    FakeClock::uninstall();
}

TEST_CASE("par de nos em loopback: comando entregue e confirmado", "[protocol]")
{
    FakeClock::install();
    FakeRadio radioA, radioB;
    radioA.connect(radioB);

    LinkLayer linkA(radioA, kPeer);  // reservatorio
    LinkLayer linkB(radioB, kMe);    // bomba

    PumpCommand got = PumpCommand::kOff;
    linkB.onReceive([&](const Packet& p) { parseCommand(p, got); });

    TEST_ASSERT_TRUE(linkA.sendReliable(makeCommand(0, kMe, 0, PumpCommand::kOn)));

    FakeClock::advance(50);
    linkB.poll();  // recebe o comando e auto-confirma
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(PumpCommand::kOn),
                            static_cast<uint8_t>(got));

    FakeClock::advance(50);
    linkA.poll();  // recebe o ACK
    TEST_ASSERT_FALSE(linkA.isDelivering());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkLayer::DeliveryResult::kAcked),
                            static_cast<uint8_t>(linkA.deliveryState()));
    FakeClock::uninstall();
}

TEST_CASE("enlace cortado no loopback: entrega falha apos as retransmissoes",
          "[protocol]")
{
    FakeClock::install();
    FakeRadio radioA, radioB;
    radioA.connect(radioB);
    radioA.deliverToPeer = false;  // nada chega do lado A

    LinkLayer linkA(radioA, kPeer);

    linkA.sendReliable(makeCommand(0, kMe, 0, PumpCommand::kOn));
    for (uint8_t i = 0; i <= cfg::Timing::kMaxRetries; ++i) {
        FakeClock::advance(cfg::Timing::kAckTimeoutMs);
        linkA.poll();
    }

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkLayer::DeliveryResult::kFailed),
                            static_cast<uint8_t>(linkA.deliveryState()));
    FakeClock::uninstall();
}

// ---- Regressao: o relogio do ACK pertence a transacao pendente -------------
// transmit() nao pode atualizar lastTxMs_, senao qualquer outro TX (heartbeat
// por sendOnce, auto-ACK de um pacote recebido) adia a retransmissao do
// comando em voo -- no limite, indefinidamente.

TEST_CASE("heartbeat em voo nao adia a retransmissao do comando", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));
    const size_t afterCommand = radio.sent.size();

    // Quase no fim da janela de ACK, o no manda um heartbeat qualquer.
    FakeClock::advance(cfg::Timing::kAckTimeoutMs - 50);
    link.sendOnce(makeHeartbeat(0, kPeer, 0));
    TEST_ASSERT_EQUAL_UINT32(afterCommand + 1, radio.sent.size());

    // Passada a janela, a retransmissao tem de sair na hora.
    FakeClock::advance(50);
    link.poll();

    TEST_ASSERT_EQUAL_UINT32(afterCommand + 2, radio.sent.size());
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MessageType::kComandoBomba),
                            static_cast<uint8_t>(lastSent(radio).type));
    FakeClock::uninstall();
}

TEST_CASE("auto-ACK nao adia a retransmissao do comando", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));
    const uint8_t seq = lastSent(radio).seq;

    // Chega um pacote que pede ACK: o auto-ACK vai ao ar no meio da espera.
    FakeClock::advance(cfg::Timing::kAckTimeoutMs - 50);
    injectPacket(radio, makeCommand(kPeer, kMe, 77, PumpCommand::kOff));
    link.poll();
    TEST_ASSERT_TRUE(lastSent(radio).isAck());

    // O prazo do comando pendente continua contando desde o envio dele.
    FakeClock::advance(50);
    link.poll();

    TEST_ASSERT_EQUAL_UINT8(seq, lastSent(radio).seq);  // a retransmissao saiu
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(MessageType::kComandoBomba),
                            static_cast<uint8_t>(lastSent(radio).type));
    FakeClock::uninstall();
}

TEST_CASE("trafego constante nao impede o timeout de falhar", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    std::vector<LinkEvent> events;
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));

    // Heartbeats a cada 100 ms, sem nunca chegar o ACK: a entrega tem de
    // falhar depois das retransmissoes, nao ficar presa para sempre.
    for (uint32_t t = 0; t < cfg::Timing::kAckTimeoutMs *
                                 (cfg::Timing::kMaxRetries + 2); t += 100) {
        FakeClock::advance(100);
        link.sendOnce(makeHeartbeat(0, kPeer, 0));
        link.poll();
    }

    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkLayer::DeliveryResult::kFailed),
                            static_cast<uint8_t>(link.deliveryState()));
    FakeClock::uninstall();
}

TEST_CASE("radio ocupado nao aceita o quadro", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    radio.busy = true;
    LinkLayer link(radio, kMe);

    link.sendReliable(makeCommand(0, kPeer, 0, PumpCommand::kOn));
    TEST_ASSERT_EQUAL_UINT32(0, radio.sent.size());  // AUX baixo: nada foi ao ar
    TEST_ASSERT_TRUE(link.isDelivering());           // segue pendente para retry
    FakeClock::uninstall();
}

// ---- Seguranca: autenticacao (#5) e anti-replay (#7) -----------------------

TEST_CASE("MAC invalido (chave errada) e rejeitado e nao gera ACK", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    int received = 0;
    std::vector<LinkEvent> events;
    link.onReceive([&](const Packet&) { ++received; });
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    // Codifica com uma chave ERRADA, como faria um atacante sem a chave.
    uint8_t wrongKey[16];
    for (int i = 0; i < 16; ++i) wrongKey[i] = cfg::Security::kLinkKey[i] ^ 0xFF;
    Packet p = makeCommand(kPeer, kMe, 5, PumpCommand::kOn);
    p.counter = 123;
    uint8_t wire[kMaxPacketSize];
    const size_t n = encodePacket(p, wrongKey, wire, sizeof(wire));
    radio.inject(wire, n);
    link.poll();

    TEST_ASSERT_EQUAL_INT(0, received);              // nao entregue a aplicacao
    TEST_ASSERT_EQUAL_UINT32(0, radio.sent.size());  // nem auto-ACK ao atacante
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kAuthFail),
                            static_cast<uint8_t>(events.back().type));
    FakeClock::uninstall();
}

TEST_CASE("replay: contador repetido ou antigo e rejeitado", "[protocol]")
{
    FakeClock::install();
    FakeRadio radio;
    LinkLayer link(radio, kMe);

    int received = 0;
    std::vector<LinkEvent> events;
    link.onReceive([&](const Packet&) { ++received; });
    link.onEvent([&](const LinkEvent& e) { events.push_back(e); });

    injectPacket(radio, makeHeartbeat(kPeer, kMe, 1), 100);
    link.poll();
    TEST_ASSERT_EQUAL_INT(1, received);  // fresco: entregue

    injectPacket(radio, makeHeartbeat(kPeer, kMe, 1), 100);  // mesmo contador
    link.poll();
    TEST_ASSERT_EQUAL_INT(1, received);  // replay: descartado
    TEST_ASSERT_EQUAL_UINT8(static_cast<uint8_t>(LinkEvent::Type::kReplay),
                            static_cast<uint8_t>(events.back().type));

    injectPacket(radio, makeHeartbeat(kPeer, kMe, 1), 50);  // mais antigo
    link.poll();
    TEST_ASSERT_EQUAL_INT(1, received);  // replay: descartado

    injectPacket(radio, makeHeartbeat(kPeer, kMe, 1), 101);  // avanca
    link.poll();
    TEST_ASSERT_EQUAL_INT(2, received);  // fresco: entregue
    FakeClock::uninstall();
}

TEST_CASE("retransmissao nao cai como replay (nonce novo por TX)", "[protocol]")
{
    FakeClock::install();
    FakeRadio radioA, radioB;
    radioA.connect(radioB);

    LinkLayer linkA(radioA, kPeer);  // reservatorio
    LinkLayer linkB(radioB, kMe);    // bomba

    int deliveredB = 0;
    linkB.onReceive([&](const Packet&) { ++deliveredB; });

    linkA.sendReliable(makeCommand(0, kMe, 0, PumpCommand::kOn));

    // O ACK do B vai se perder: cortamos B->A antes de B responder.
    radioB.deliverToPeer = false;
    FakeClock::advance(50);
    linkB.poll();  // recebe e entrega (1); auto-ACK se perde
    TEST_ASSERT_EQUAL_INT(1, deliveredB);

    // A nao recebeu ACK -> retransmite, com um contador NOVO (nonce por TX).
    FakeClock::advance(cfg::Timing::kAckTimeoutMs);
    linkA.poll();
    FakeClock::advance(50);
    linkB.poll();  // retransmissao: fresca, reentregue (comando e idempotente)
    TEST_ASSERT_EQUAL_INT(2, deliveredB);
    FakeClock::uninstall();
}
