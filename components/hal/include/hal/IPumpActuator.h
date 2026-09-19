#pragma once
// ============================================================================
//  IPumpActuator.h  -  Atuador liga/desliga da bomba (rele ou contator).
// ----------------------------------------------------------------------------
//  O estado padrao e DESLIGADO: qualquer construcao, falha ou perda de enlace
//  deve deixar a bomba desenergizada. begin() e obrigatorio para estabelecer
//  esse estado seguro antes do primeiro comando ser honrado.
// ============================================================================

namespace hal {

class IPumpActuator {
public:
    virtual ~IPumpActuator() = default;

    // Configura o pino de acionamento e forca a bomba DESLIGADA. Retorna false
    // em caso de falha.
    virtual bool begin() = 0;

    virtual void turnOn() = 0;
    virtual void turnOff() = 0;

    // Conveniencia para chamadores orientados a estado.
    void set(bool on) { on ? turnOn() : turnOff(); }

    virtual bool isOn() const = 0;
};

}  // namespace hal
