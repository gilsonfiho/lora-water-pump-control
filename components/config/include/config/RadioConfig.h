#pragma once
// ============================================================================
//  RadioConfig.h  -  Parametros de RF compartilhados pelos dois modulos E220.
// ----------------------------------------------------------------------------
//  Ambos os lados DEVEM concordar em canal e taxa aerea (air data rate). A
//  unica diferenca por unidade entre E220-900T30D e E220-900T22D e a tabela de
//  potencia de transmissao, tratada por LoraPowerLevel + a variante do driver.
// ============================================================================

#include <cstdint>

#include "config/NodeConfig.h"  // BENCH_PROFILE

namespace cfg {

// Frequencia = 850,125 MHz + canal * 1 MHz  (serie E220-900, 0..80).
// O padrao de fabrica do modulo (canal 23 -> 873,125 MHz) cai no downlink
// celular de 850 MHz e NAO e permitido no Brasil. A ANATEL (Ato 14.448/2017)
// libera 902-907,5 MHz (CH 52..57) e 915-928 MHz (CH 65..77); ver
// docs/context/hardware.md. O canal 55 fica no meio da primeira faixa, fora da
// 915-928 MHz, onde operam as redes LoRaWAN AU915.
struct Radio {
    static constexpr uint8_t  kChannel        = 55;      // 905,125 MHz
    static constexpr uint16_t kModuleAddress  = 0x0000;  // modo transparente
    static constexpr uint16_t kEncryptionKey  = 0x0000;  // TODO: definir chave real
    static constexpr uint32_t kConfigBaud     = 9600;    // fixo pelo datasheet

    static_assert((kChannel >= 52 && kChannel <= 57) || (kChannel >= 65 && kChannel <= 77),
                  "canal fora das faixas ANATEL (902-907,5 / 915-928 MHz)");
};

// Valor do registrador de taxa aerea (REG0 bits[2:0]). Menor = maior alcance.
// 2,4 kbps e a referencia do datasheet para a figura de ~10 km / 5 dBi.
enum class LoraAirRate : uint8_t {
    k2_4k  = 0b010,  // padrao, melhor alcance
    k4_8k  = 0b011,
    k9_6k  = 0b100,
    k19_2k = 0b101,
    k38_4k = 0b110,
    k62_5k = 0b111,
};

// Baud UART entre ESP32-C3 e modulo durante operacao NORMAL (REG0 bits[7:5]).
// Independente dos 9600 fixos usados no modo de configuracao.
enum class LoraUartBaud : uint8_t {
    k9600   = 0b011,  // padrao
    k19200  = 0b100,
    k38400  = 0b101,
    k57600  = 0b110,
    k115200 = 0b111,
};

// Potencia de transmissao (REG1 bits[1:0]). O significado em dBm depende da
// variante:
//   T30D: Max=30 High=27 Med=24 Low=21 dBm
//   T22D: Max=22 High=17 Med=13 Low=10 dBm
enum class LoraPowerLevel : uint8_t {
    kMax  = 0b00,  // padrao
    kHigh = 0b01,
    kMed  = 0b10,
    kLow  = 0b11,
};

// Potencia usada pelos dois nos. Na BANCADA os modulos ficam a centimetros um
// do outro: a 22 dBm e ~30 cm de antena a antena (FSPL ~21 dB a 900 MHz)
// chegam de +1 a +5 dBm no receptor, perto do maximo de entrada do LLCC68
// (~+10 dBm). O front-end satura e aparecem erros de CRC e perdas que nao vem
// do protocolo. 10 dBm (T22D kLow) resolve isso. Em campo volta ao maximo.
#if BENCH_PROFILE
inline constexpr LoraPowerLevel kTxPower = LoraPowerLevel::kLow;  // T22D: 10 dBm
#else
inline constexpr LoraPowerLevel kTxPower = LoraPowerLevel::kMax;  // T22D: 22 dBm
#endif

// Qual modulo fisico esta instalado; afeta apenas a documentacao da tabela de
// potencia e o orcamento de corrente, nao o protocolo de comandos.
enum class E220Variant : uint8_t {
    kT30D,  // 30 dBm / 1 W
    kT22D,  // 22 dBm / 160 mW
};

// Um unico lugar reunindo o perfil enviado ao modulo no boot.
struct LoraProfile {
    uint16_t       address    = Radio::kModuleAddress;
    uint8_t        channel    = Radio::kChannel;
    LoraAirRate    airRate    = LoraAirRate::k2_4k;
    LoraUartBaud   uartBaud   = LoraUartBaud::k9600;
    LoraPowerLevel power      = kTxPower;
    E220Variant    variant    = E220Variant::kT30D;  // TODO(hw): por no
    bool           enableLbt  = true;   // listen-before-talk (LBT)
    bool           appendRssi = false;  // anexa byte de RSSI ao payload RX
};

}  // namespace cfg
