#pragma once
// ============================================================================
//  PinConfig.h  -  Atribuicao dos pinos fisicos dos nos ESP32-C3.
// ----------------------------------------------------------------------------
//  O ESP32-C3 expoe poucos GPIOs, entao cada pino abaixo e uma escolha
//  deliberada. Todos os valores sao PROVISORIOS: confirme com a placa final e
//  a fiacao antes de gravar no hardware. Procure por "TODO(hw)" para revisar.
//
//  ---- Pinos do ESP32-C3 que NAO podem ser usados a esmo --------------------
//    GPIO 11..17  flash SPI interna -- indisponiveis.
//    GPIO 18, 19  USB D-/D+. Usa-los como GPIO DERRUBA o USB Serial/JTAG, que
//                 e como o devkit grava e abre o monitor. Evitar na bancada.
//    GPIO 20, 21  UART0 do console (logs).
//    GPIO 2, 8, 9 pinos de strap de boot. Um sinal externo segurando-os no
//                 nivel errado no reset impede o boot. GPIO 9 e o botao BOOT.
//
//  Sobram, livres de ressalva: GPIO 0, 1, 3, 4, 5, 6, 7 e 10 -- exatamente os
//  oito que o prototipo usa.
// ============================================================================

#include <cstdint>

namespace cfg {

// ---- Selecao do papel do no (firmware unico) ------------------------------
// Um so binario roda nos dois nos; o papel e lido no boot. O pino usa pull-up
// interno, entao DEIXAR ABERTO significa RESERVATORIO e LIGAR AO GND significa
// BOMBA.
//
// A atribuicao nao e arbitraria: o reservatorio e o papel que NAO aciona rele.
// Assim, um no mal strapado (ou com o fio solto) nunca assume por engano o
// papel que energiza a bomba.
struct RolePins {
    static constexpr int kRoleStrap = 3;  // aberto = reservatorio, GND = bomba  TODO(hw)
};

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
//
// ATENCAO: kFloatHigh cai no GPIO 2, que e pino de strap de boot. Uma boia
// fechada para o GND no momento do reset impede o ESP de subir. Ao voltar para
// as boias, remapear este pino.  TODO(hw)
struct LevelSensorPins {
    static constexpr int kFloatLow  = 1;  // fecha quando agua abaixo da marca baixa  TODO(hw)
    static constexpr int kFloatHigh = 2;  // fecha quando agua acima da marca alta    TODO(hw)

    // Alternativa ultrassonica (nao usada quando ha boias).
    static constexpr int kUltrasonicTrig = 1;  // TODO(hw)
    static constexpr int kUltrasonicEcho = 2;  // TODO(hw)
};

// ---- Chave de bancada (substitui a boia no prototipo) ---------------------
// Chave que TRAVA na posicao (gangorra/alavanca), nao botao momentaneo: ela
// precisa manter o estado entre as leituras do ciclo. Liga o pino ao GND de um
// lado; o pull-up interno cuida do resto.
//
//   fechada (GND)     -> caixa BAIXA  -> LIGAR bomba
//   aberta (pull-up)  -> caixa CHEIA  -> DESLIGAR bomba
struct BenchPins {
    static constexpr int kLevelSwitch = 1;
};

// ---- Atuador da bomba (no da bomba) ---------------------------------------
// Na bancada este pino aciona um LED (+ resistor de ~330R para o GND) em vez do
// contator: valida o caminho inteiro sem carga AC. Ativo alto.
struct PumpPins {
    static constexpr int kRelay       = 0;     // bancada: LED; campo: bobina do contator  TODO(hw)
    static constexpr bool kActiveHigh = true;  // nivel logico do modulo rele              TODO(hw)
};

// ---- Subsistema de energia (ambos os nos) ---------------------------------
// A tensao da bateria e lida por um divisor resistivo em um pino ADC. A linha
// de "fonte externa presente" permite detectar operacao em rede vs. bateria.
//
// NAO USADOS NA BANCADA: o prototipo roda com StubBatteryMonitor, entao nenhum
// destes pinos e configurado. Dois conflitos precisam ser resolvidos antes de
// reativar o AdcBatteryMonitor:  TODO(hw)
//   * kBatteryAdc (0) colide com PumpPins::kRelay. Hoje nao da problema porque
//     cada um vive num no diferente, mas e uma armadilha esperando acontecer.
//   * kExternalPresent (18) e o USB D-. Usa-lo derruba o USB Serial/JTAG.
struct PowerPins {
    static constexpr int kBatteryAdc      = 0;   // ADC1_CH0, via divisor        TODO(hw)
    static constexpr int kExternalPresent = 18;  // alto quando adaptador presente  TODO(hw)

    // Medidor INA219/INA226 via I2C (opcional, preferido sobre o ADC cru).
    static constexpr int kI2cSda = 8;   // TODO(hw)
    static constexpr int kI2cScl = 9;   // TODO(hw)
};

}  // namespace cfg
