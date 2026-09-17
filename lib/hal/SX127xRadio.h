#pragma once
// ============================================================================
//  SX127xRadio.h  -  Estrategia alternativa: Semtech SX1276/RFM95 via SPI.
// ----------------------------------------------------------------------------
//  ESQUELETO. O hardware principal deste projeto e o E220 (UART). Esta classe
//  existe para provar que a arquitetura Strategy permite trocar o transporte
//  sem tocar nas camadas protocol/ e core/. Preencha os TODOs se/quando um
//  modulo SX127x for adotado.
// ============================================================================

#include <Arduino.h>

#include "ILoRaRadio.h"

namespace hal {

// Pinos SPI do SX127x. Distintos dos pinos UART do E220; ajuste conforme a
// placa. Mantidos aqui (nao em PinConfig) porque sao especificos desta
// estrategia alternativa.
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
    SX127xRadio(const Sx127xPins& pins, uint32_t frequencyHz);

    bool begin() override;
    bool send(const uint8_t* data, size_t length) override;
    bool available() override;
    size_t receive(uint8_t* buffer, size_t bufferSize) override;
    int16_t lastRssiDbm() override;
    bool isBusy() override;
    void poll() override;
    void sleep() override;
    void wake() override;

private:
    Sx127xPins pins_;
    uint32_t frequencyHz_;
    volatile bool rxDone_ = false;  // setado no ISR de DIO0
    // TODO(hw): registradores SPI, mapeamento FIFO, configuracao de modem
    // (BW/SF/CR), tratamento de IRQ DIO0. Ver AN1200.xx da Semtech.
};

}  // namespace hal
