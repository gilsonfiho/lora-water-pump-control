// ============================================================================
//  SX127xRadio.cpp  -  Esqueleto do driver SPI (estrategia alternativa).
// ----------------------------------------------------------------------------
//  Todos os metodos tem uma implementacao minima e segura (no-op / retorno
//  falso) para que o projeto compile com esta estrategia selecionada. Os
//  pontos de extensao estao marcados com TODO(hw).
// ============================================================================

#include "hw/SX127xRadio.h"

namespace hw {

SX127xRadio::SX127xRadio(const Sx127xPins& pins, uint32_t frequencyHz)
    : pins_(pins), frequencyHz_(frequencyHz) {}

bool SX127xRadio::begin() {
    // TODO(hw): inicializar SPI, resetar o chip, checar RegVersion (0x42),
    // colocar em modo LoRa sleep->standby, programar frequencia e parametros
    // de modem, anexar ISR em dio0. Retornar false se o chip nao responder.
    return false;  // ainda nao implementado -> chamador entra em estado seguro
}

bool SX127xRadio::send(const uint8_t*, size_t) {
    // TODO(hw): escrever FIFO, setar modo TX, aguardar TxDone via DIO0.
    return false;
}

bool SX127xRadio::available() { return rxDone_; }

size_t SX127xRadio::receive(uint8_t*, size_t) {
    // TODO(hw): ler RegRxNbBytes e drenar FIFO.
    rxDone_ = false;
    return 0;
}

int16_t SX127xRadio::lastRssiDbm() {
    // TODO(hw): ler RegPktRssiValue e converter.
    return 0;
}

bool SX127xRadio::isBusy() { return false; }

void SX127xRadio::poll() {
    // TODO(hw): se nao usar IRQ, sondar RegIrqFlags aqui.
}

void SX127xRadio::sleep() { /* TODO(hw): RegOpMode = LoRa sleep */ }
void SX127xRadio::wake()  { /* TODO(hw): RegOpMode = LoRa standby */ }

}  // namespace hw
