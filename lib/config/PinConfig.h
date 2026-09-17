#pragma once
// ============================================================================
//  PinConfig.h  -  Atribuicao dos pinos fisicos dos nos ESP32-C3.
// ----------------------------------------------------------------------------
//  O ESP32-C3 expoe poucos GPIOs, entao cada pino abaixo e uma escolha
//  deliberada. Todos os valores sao PROVISORIOS: confirme com a placa final e
//  a fiacao antes de gravar no hardware. Procure por "TODO(hw)" para revisar.
// ============================================================================

#include <cstdint>

namespace cfg {

// ---- Modulo LoRa E220 (UART + GPIOs de modo/status) -----------------------
// O E220 fala UART TTL. AUX e uma linha de ocupado/pronto e deve permitir
// interrupcao, para reagirmos na borda de subida em vez de fazer polling.
struct LoraPins {
    static constexpr int kUartRx = 4;   // RX do ESP  <- TXD do E220   TODO(hw)
    static constexpr int kUartTx = 5;   // TX do ESP  -> RXD do E220   TODO(hw)
    static constexpr int kM0     = 6;   // selecao de modo, bit 0      TODO(hw)
    static constexpr int kM1     = 7;   // selecao de modo, bit 1      TODO(hw)
    static constexpr int kAux    = 10;  // ocupado/pronto (IRQ)        TODO(hw)
};

// ---- Sensor de nivel (no do reservatorio) ---------------------------------
// Duas boias dao histerese (baixo = comeca a encher, alto = para de encher).
// Um sensor ultrassonico pode substituir as duas; ver UltrasonicLevelSensor.
struct LevelSensorPins {
    static constexpr int kFloatLow  = 1;  // fecha quando agua abaixo da marca baixa  TODO(hw)
    static constexpr int kFloatHigh = 2;  // fecha quando agua acima da marca alta    TODO(hw)

    // Alternativa ultrassonica (nao usada quando ha boias).
    static constexpr int kUltrasonicTrig = 1;  // TODO(hw)
    static constexpr int kUltrasonicEcho = 2;  // TODO(hw)
};

// ---- Atuador da bomba (no da bomba) ---------------------------------------
struct PumpPins {
    static constexpr int kRelay       = 3;     // aciona a bobina do rele/contator  TODO(hw)
    static constexpr bool kActiveHigh = true;  // nivel logico do modulo rele       TODO(hw)
};

// ---- Subsistema de energia (ambos os nos) ---------------------------------
// A tensao da bateria e lida por um divisor resistivo em um pino ADC. A linha
// de "fonte externa presente" permite detectar operacao em rede vs. bateria.
struct PowerPins {
    static constexpr int kBatteryAdc      = 0;   // ADC1_CH0, via divisor        TODO(hw)
    static constexpr int kExternalPresent = 18;  // alto quando adaptador presente  TODO(hw)

    // Medidor INA219/INA226 via I2C (opcional, preferido sobre o ADC cru).
    static constexpr int kI2cSda = 8;   // TODO(hw)
    static constexpr int kI2cScl = 9;   // TODO(hw)
};

}  // namespace cfg
