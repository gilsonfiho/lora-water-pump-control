// ============================================================================
//  SX127xRadio.cpp  -  Esqueleto do driver SPI (estrategia alternativa).
//  Implementacao minima e segura para o projeto compilar com esta estrategia.
// ============================================================================

#include "hw/SX127xRadio.h"

namespace hw {

bool SX127xRadio::begin() {
    // TODO(hw): spi_bus_initialize, spi_bus_add_device, reset, checar RegVersion
    // (0x42), modo LoRa sleep->standby, frequencia e parametros de modem, ISR
    // em dio0. Retornar false enquanto nao implementado.
    return false;
}

bool SX127xRadio::send(const uint8_t*, size_t) {
    // TODO(hw): escrever FIFO, modo TX, aguardar TxDone (DIO0).
    return false;
}

int SX127xRadio::read(uint8_t*, size_t, uint32_t) {
    // TODO(hw): ao receber RxDone, ler RegRxNbBytes e drenar FIFO.
    return 0;
}

bool SX127xRadio::isBusy() { return false; }
void SX127xRadio::sleep() { /* TODO(hw): RegOpMode = LoRa sleep */ }
void SX127xRadio::wake()  { /* TODO(hw): RegOpMode = LoRa standby */ }

}  // namespace hw
