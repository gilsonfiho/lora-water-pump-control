#pragma once
// ============================================================================
//  SX127xRadio.h  -  Estrategia alternativa: Semtech SX1276/RFM95 via SPI.
// ----------------------------------------------------------------------------
//  ESQUELETO. O hardware principal e o E220 (UART). Esta classe prova que a
//  arquitetura Strategy permite trocar o transporte sem tocar em protocol/ e
//  core/. Preencha os TODOs com o driver spi_master do ESP-IDF.
// ============================================================================

#include <cstdint>

#include "hw/ILoRaRadio.h"

namespace hw {

// Pinos SPI do SX127x (especificos desta estrategia; ajuste conforme a placa).
struct Sx127xPins {
    int sck  = -1;
    int miso = -1;
    int mosi = -1;
    int nss  = -1;   // chip select
    int rst  = -1;
    int dio0 = -1;   // IRQ de TX done / RX done
};

class SX127xRadio : public ILoRaRadio {
public:
    SX127xRadio(const Sx127xPins& pins, uint32_t frequencyHz)
        : pins_(pins), frequencyHz_(frequencyHz) {}

    bool begin() override;
    bool send(const uint8_t* data, size_t length) override;
    int  read(uint8_t* buffer, size_t maxLen, uint32_t timeoutMs) override;
    bool isBusy() override;
    void sleep() override;
    void wake() override;

private:
    Sx127xPins pins_;
    uint32_t   frequencyHz_;
    // TODO(hw): spi_device_handle_t, config de modem (BW/SF/CR), IRQ DIO0.
};

}  // namespace hw
