#include "core/RssiIndicator.h"

#include "platform/Clock.h"

namespace core {

uint32_t RssiIndicator::blinkPeriodMs(int16_t rssiDbm) {
    if (rssiDbm >= cfg::Indicator::kStrongDbm) return cfg::Indicator::kStrongPeriodMs;
    if (rssiDbm >= cfg::Indicator::kMediumDbm) return cfg::Indicator::kMediumPeriodMs;
    return cfg::Indicator::kWeakPeriodMs;
}

void RssiIndicator::onValidPacket(int16_t rssiDbm) {
    const uint32_t now = platform::millis();
    if (!hasSignal_) periodStart_ = now;  // primeiro pacote: pisca na hora
    hasSignal_ = true;
    lastRxMs_  = now;
    periodMs_  = blinkPeriodMs(rssiDbm);
}

void RssiIndicator::poll() {
    const uint32_t now = platform::millis();

    if (hasSignal_ && now - lastRxMs_ >= cfg::Indicator::kNoSignalTimeoutMs) {
        hasSignal_ = false;
    }

    bool on = false;
    if (hasSignal_) {
        if (now - periodStart_ >= periodMs_) periodStart_ = now;
        on = (now - periodStart_) < cfg::Indicator::kPulseMs;
    }

    if (on != ledOn_) {
        ledOn_ = on;
        led_.set(on);
    }
}

}  // namespace core
