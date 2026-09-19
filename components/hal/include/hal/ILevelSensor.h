#pragma once
// ============================================================================
//  ILevelSensor.h  -  Sensor de nivel de agua abstrato (Strategy + fonte de
//                     eventos para Observer).
// ----------------------------------------------------------------------------
//  Implementacoes: FloatSwitchLevelSensor (duas boias, histerese) e
//  UltrasonicLevelSensor (um sensor de distancia). O controlador consome o
//  LevelState discreto; a porcentagem e opcional e pode ser desconhecida.
// ============================================================================

#include <cstdint>

namespace hal {

// Nivel discreto visto pela logica de controle. Os dois estados de "faixa"
// guiam a decisao encher/parar com histerese embutida; kUnknown forca um
// padrao seguro.
enum class LevelState : uint8_t {
    kUnknown = 0,  // falha do sensor / ainda nao lido -> tratar como "nao bombear"
    kLow,          // abaixo da marca baixa   -> caixa precisa encher
    kMid,          // entre as marcas         -> mantem a acao atual
    kHigh,         // na/acima da marca alta  -> caixa cheia, para de encher
};

class ILevelSensor {
public:
    virtual ~ILevelSensor() = default;

    virtual bool begin() = 0;

    // Faz uma leitura nova. Barata e nao bloqueante para boias; a versao
    // ultrassonica cacheia a ultima medicao.
    virtual LevelState read() = 0;

    // Porcentagem de enchimento opcional [0..100]; retorna false se nao for
    // mensuravel (ex.: boias puras dao apenas estados discretos).
    virtual bool readPercent(uint8_t& percentOut) = 0;
};

}  // namespace hal
