#pragma once
// ============================================================================
//  ReservoirController.h  -  Logica do no do reservatorio (transmissor).
// ----------------------------------------------------------------------------
//  Le o nivel (boia/chave), decide com histerese e envia o comando ao no da
//  bomba de forma confiavel (ACK/retry). Tambem envia heartbeat e reporta a
//  bateria. Metodos chamados pela tarefa de controle a partir de eventos/timers.
//
//  Decisao: kLow->LIGAR, kHigh->DESLIGAR, kMid->mantem, kUnknown->DESLIGAR.
//  Emite evento (Observer) a cada mudanca de LevelState.
// ============================================================================

#include <cstdint>

#include "core/Observer.h"
#include "hw/IBatteryMonitor.h"
#include "hw/ILevelSensor.h"
#include "protocol/LinkLayer.h"
#include "protocol/Messages.h"

namespace core {

struct LevelChangedEvent {
    hw::LevelState previous;
    hw::LevelState current;
};

class ReservoirController {
public:
    ReservoirController(protocol::LinkLayer& link, hw::ILevelSensor& sensor,
                        hw::IBatteryMonitor& battery, uint8_t pumpNodeAddress)
        : link_(link), sensor_(sensor), battery_(battery),
          pumpNodeAddress_(pumpNodeAddress) {}

    void begin();

    // Le o nivel, atualiza a decisao e envia o comando (confiavel) ao no bomba.
    void evaluateAndSend();
    void sendHeartbeat();
    void reportBattery();

    Signal<LevelChangedEvent>& onLevelChanged() { return levelChanged_; }
    protocol::PumpCommand desiredCommand() const { return desired_; }

private:
    protocol::PumpCommand decide(hw::LevelState level) const;

    protocol::LinkLayer&  link_;
    hw::ILevelSensor&     sensor_;
    hw::IBatteryMonitor&  battery_;
    uint8_t               pumpNodeAddress_;

    Signal<LevelChangedEvent> levelChanged_;
    hw::LevelState            lastLevel_ = hw::LevelState::kUnknown;
    protocol::PumpCommand     desired_ = protocol::PumpCommand::kOff;
};

}  // namespace core
