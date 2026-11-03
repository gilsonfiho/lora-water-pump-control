// ============================================================================
//  test_rssi_indicator.cpp  -  Faixas RSSI -> cadencia do LED e timeout de
//                              "sem sinal", com relogio controlado.
// ============================================================================

#include <unity.h>

#include "config/NodeConfig.h"
#include "core/RssiIndicator.h"
#include "fakes.h"

using core::RssiIndicator;
using test::FakeClock;
using test::FakeIndicator;

namespace {

// Avanca o tempo em passos de 10 ms chamando poll(), como o laco principal.
void run(RssiIndicator& ind, uint32_t ms) {
    for (uint32_t t = 0; t < ms; t += 10) {
        FakeClock::advance(10);
        ind.poll();
    }
}

}  // namespace

TEST_CASE("RSSI forte pisca mais rapido que medio e fraco", "[core]")
{
    const uint32_t strong = RssiIndicator::blinkPeriodMs(-60);
    const uint32_t medium = RssiIndicator::blinkPeriodMs(-90);
    const uint32_t weak   = RssiIndicator::blinkPeriodMs(-115);
    TEST_ASSERT_LESS_THAN_UINT32(medium, strong);
    TEST_ASSERT_LESS_THAN_UINT32(weak, medium);

    // Bordas dos limiares: o limiar pertence a faixa de cima.
    TEST_ASSERT_EQUAL_UINT32(strong, RssiIndicator::blinkPeriodMs(cfg::Indicator::kStrongDbm));
    TEST_ASSERT_EQUAL_UINT32(medium, RssiIndicator::blinkPeriodMs(cfg::Indicator::kStrongDbm - 1));
    TEST_ASSERT_EQUAL_UINT32(medium, RssiIndicator::blinkPeriodMs(cfg::Indicator::kMediumDbm));
    TEST_ASSERT_EQUAL_UINT32(weak, RssiIndicator::blinkPeriodMs(cfg::Indicator::kMediumDbm - 1));
}

TEST_CASE("sem pacote valido o LED fica apagado", "[core]")
{
    FakeClock::install();
    FakeIndicator led;
    RssiIndicator ind(led);

    run(ind, 5000);
    TEST_ASSERT_FALSE(led.on);
    TEST_ASSERT_EQUAL_INT(0, led.risingEdges);
}

TEST_CASE("pacote forte faz o LED piscar na cadencia da faixa", "[core]")
{
    FakeClock::install();
    FakeIndicator led;
    RssiIndicator ind(led);

    ind.onValidPacket(-60);
    run(ind, 3000);
    // Um pisca por periodo; uma tolerancia de 1 para as bordas da janela.
    const int expected = 3000 / cfg::Indicator::kStrongPeriodMs;
    TEST_ASSERT_INT_WITHIN(1, expected, led.risingEdges);
}

TEST_CASE("sinal fraco pisca menos vezes que o forte no mesmo tempo", "[core]")
{
    FakeClock::install();
    FakeIndicator strongLed, weakLed;
    RssiIndicator strong(strongLed), weak(weakLed);

    strong.onValidPacket(-60);
    weak.onValidPacket(-115);
    for (int i = 0; i < 300; ++i) {  // 3 s
        FakeClock::advance(10);
        strong.poll();
        weak.poll();
    }
    TEST_ASSERT_GREATER_THAN_INT(weakLed.risingEdges, strongLed.risingEdges);
}

TEST_CASE("o LED apaga apos o timeout sem pacote valido", "[core]")
{
    FakeClock::install();
    FakeIndicator led;
    RssiIndicator ind(led);

    ind.onValidPacket(-60);
    run(ind, cfg::Indicator::kNoSignalTimeoutMs - 100);
    TEST_ASSERT_GREATER_THAN_INT(0, led.risingEdges);

    run(ind, 200);  // passa do timeout
    const int edges = led.risingEdges;
    run(ind, 3000);
    TEST_ASSERT_FALSE(led.on);
    TEST_ASSERT_EQUAL_INT(edges, led.risingEdges);  // nao piscou mais
}

TEST_CASE("novo pacote apos o timeout reinicia o pisca", "[core]")
{
    FakeClock::install();
    FakeIndicator led;
    RssiIndicator ind(led);

    ind.onValidPacket(-60);
    run(ind, cfg::Indicator::kNoSignalTimeoutMs + 500);
    const int before = led.risingEdges;

    ind.onValidPacket(-60);
    run(ind, 1000);
    TEST_ASSERT_GREATER_THAN_INT(before, led.risingEdges);
}

TEST_CASE("o pulso acende por kPulseMs e depois apaga", "[core]")
{
    FakeClock::install();
    FakeIndicator led;
    RssiIndicator ind(led);

    ind.onValidPacket(-60);
    FakeClock::advance(10);
    ind.poll();
    TEST_ASSERT_TRUE(led.on);

    run(ind, cfg::Indicator::kPulseMs + 20);
    TEST_ASSERT_FALSE(led.on);
}
