#pragma once
// ============================================================================
//  ISignalIndicator.h  -  Indicador luminoso simples (um LED liga/desliga).
// ----------------------------------------------------------------------------
//  A camada core/ decide QUANDO acender; quem implementa decide COMO (GPIO).
//  Sempre inicia apagado.
// ============================================================================

namespace hw {

class ISignalIndicator {
public:
    virtual ~ISignalIndicator() = default;

    // Configura o pino e deixa o indicador apagado. false em caso de falha.
    virtual bool begin() = 0;

    virtual void set(bool on) = 0;
};

}  // namespace hw
