#pragma once
// ============================================================================
//  ReservoirController.h  -  Maquina de estados do no do reservatorio.
// ----------------------------------------------------------------------------
//  Le o nivel da caixa, decide (com histerese) se a bomba deve encher, e envia
//  o comando ao no da bomba de forma confiavel (com ACK/retry). Tambem emite
//  heartbeats para manter o enlace vivo e reporta a telemetria de bateria.
//
//  Decisao de bomba (histerese vem do proprio sensor de boias):
//    kLow     -> LIGAR  (caixa baixa, precisa encher)
//    kHigh    -> DESLIGAR (caixa cheia)
//    kMid     -> mantem o comando anterior
//    kUnknown -> DESLIGAR (falha do sensor -> seguro)
//
//  Emite um evento (Observer) a cada mudanca de LevelState, para acoplar
//  facilmente log, display ou LED sem tocar na logica de controle.
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
    void loop();

    // Ponto de extensao Observer para eventos de mudanca de nivel.
    Signal<LevelChangedEvent>& onLevelChanged() { return levelChanged_; }

    protocol::PumpCommand desiredCommand() const { return desired_; }

private:
    void evaluateLevel();
    void maybeSendCommand(bool forceResend);
    void maybeSendHeartbeat();
    void maybeReportBattery();
    protocol::PumpCommand decide(hw::LevelState level) const;

    protocol::LinkLayer& link_;
    hw::ILevelSensor& sensor_;
    hw::IBatteryMonitor& battery_;
    uint8_t pumpNodeAddress_;

    Signal<LevelChangedEvent> levelChanged_;

    hw::LevelState lastLevel_ = hw::LevelState::kUnknown;
    protocol::PumpCommand desired_ = protocol::PumpCommand::kOff;

    uint32_t lastCycleMs_ = 0;
    uint32_t lastHeartbeatMs_ = 0;
    uint32_t lastBatteryMs_ = 0;
};

}  // namespace core
