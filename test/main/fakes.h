#pragma once
// ============================================================================
//  fakes.h  -  Dubles de teste: relogio, radio, atuador, sensor e bateria.
// ----------------------------------------------------------------------------
//  Todos implementam as interfaces reais da hw/, entao os testes exercitam o
//  mesmo codigo de producao que roda na placa. Nada aqui entra no firmware.
// ============================================================================

#include <cstring>
#include <deque>
#include <vector>

#include "hw/IBatteryMonitor.h"
#include "hw/ILevelSensor.h"
#include "hw/ILoRaRadio.h"
#include "hw/IPumpActuator.h"
#include "platform/Clock.h"

namespace test {

// ---- Relogio controlado pelo teste ----------------------------------------
// Um unico contador global, porque platform::setTimeSource recebe um ponteiro
// de funcao livre. install() deve ser chamado no inicio de cada teste.
class FakeClock {
public:
    static void install(uint32_t startMs = 1000) {
        nowMs_ = startMs;
        platform::setTimeSource(&read);
    }
    static void uninstall() { platform::setTimeSource(nullptr); }

    static void advance(uint32_t ms) { nowMs_ += ms; }
    static uint32_t now() { return nowMs_; }

private:
    static uint32_t read() { return nowMs_; }
    static uint32_t nowMs_;
};

// ---- Radio falso -----------------------------------------------------------
// Guarda os quadros transmitidos e entrega os que o teste injetar. Dois podem
// ser ligados em loopback com connect(), formando um par de nos.
class FakeRadio : public hw::ILoRaRadio {
public:
    bool begin() override { return beginResult; }

    bool send(const uint8_t* data, size_t length) override {
        if (busy) return false;
        std::vector<uint8_t> frame(data, data + length);
        sent.push_back(frame);
        if (peer_ && deliverToPeer) peer_->inbox.push_back(frame);
        return true;
    }

    bool available() override { return !inbox.empty(); }

    size_t receive(uint8_t* buffer, size_t bufferSize) override {
        if (inbox.empty()) return 0;
        const std::vector<uint8_t> frame = inbox.front();
        inbox.pop_front();
        const size_t n = frame.size() < bufferSize ? frame.size() : bufferSize;
        std::memcpy(buffer, frame.data(), n);
        return n;
    }

    int16_t lastRssiDbm() override { return -70; }
    bool isBusy() override { return busy; }
    void poll() override { ++pollCount; }
    void sleep() override { sleeping = true; }
    void wake() override { sleeping = false; }

    // Liga dois radios: o que um envia cai na caixa de entrada do outro.
    void connect(FakeRadio& other) {
        peer_ = &other;
        other.peer_ = this;
    }

    // Injeta bytes crus como se tivessem chegado pelo ar.
    void inject(const uint8_t* data, size_t length) {
        inbox.push_back(std::vector<uint8_t>(data, data + length));
    }

    std::vector<std::vector<uint8_t>> sent;
    std::deque<std::vector<uint8_t>> inbox;
    bool beginResult = true;
    bool busy = false;
    bool deliverToPeer = true;  // false simula enlace cortado
    bool sleeping = false;
    int pollCount = 0;

private:
    FakeRadio* peer_ = nullptr;
};

// ---- Atuador falso ---------------------------------------------------------
class FakePump : public hw::IPumpActuator {
public:
    bool begin() override {
        ++beginCount;
        on_ = false;  // estado seguro, como o driver real
        return true;
    }
    void turnOn() override {
        if (!on_) ++onTransitions;
        on_ = true;
    }
    void turnOff() override {
        if (on_) ++offTransitions;
        on_ = false;
    }
    bool isOn() const override { return on_; }

    int beginCount = 0;
    int onTransitions = 0;
    int offTransitions = 0;

private:
    bool on_ = false;
};

// ---- Sensor de nivel falso -------------------------------------------------
class FakeLevelSensor : public hw::ILevelSensor {
public:
    bool begin() override { return true; }
    hw::LevelState read() override { return level; }
    bool readPercent(uint8_t& percentOut) override {
        if (!percentValid) return false;
        percentOut = percent;
        return true;
    }

    hw::LevelState level = hw::LevelState::kUnknown;
    uint8_t percent = 0;
    bool percentValid = false;
};

// ---- Monitor de bateria falso ---------------------------------------------
class FakeBattery : public hw::IBatteryMonitor {
public:
    bool begin() override { return true; }
    hw::BatteryTelemetry read() override { return telemetry; }

    hw::BatteryTelemetry telemetry{};
};

}  // namespace test
