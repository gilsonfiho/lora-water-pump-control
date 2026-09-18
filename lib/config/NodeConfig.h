#pragma once
// ============================================================================
//  NodeConfig.h  -  Enderecos logicos, papeis e temporizacao dos dois nos.
// ----------------------------------------------------------------------------
//  Estes sao valores da camada de aplicacao carregados dentro do nosso proprio
//  cabecalho de pacote; sao independentes dos registradores de endereco do
//  E220 (o radio roda em modo transparente e o enderecamento e feito por nos).
// ============================================================================

#include <cstdint>

namespace cfg {

// Enderecos logicos dos nos (1 byte cada, 0x00 e 0xFF reservados).
enum NodeAddress : uint8_t {
    kAddrReservoir = 0x01,
    kAddrPump      = 0x02,
    kAddrBroadcast = 0xFF,
};

// ---- Temporizacao (milissegundos) -----------------------------------------
struct Timing {
    // Reservatorio: com que frequencia reavaliar o nivel e reenviar o comando.
    static constexpr uint32_t kReservoirCyclePeriodMs = 10'000;

    // Reservatorio: com que frequencia reportar o status da bateria.
    static constexpr uint32_t kBatteryReportPeriodMs = 60'000;

    // Camada de enlace: espera por um ACK antes de retransmitir.
    static constexpr uint32_t kAckTimeoutMs = 800;
    static constexpr uint8_t  kMaxRetries   = 4;

    // Bomba: se nenhum comando/heartbeat valido chegar dentro desta janela,
    // o enlace e considerado perdido e a bomba e DESLIGADA (failsafe).
    static constexpr uint32_t kLinkLostTimeoutMs = 45'000;

    // Cadencia do heartbeat do reservatorio -> bomba para manter o enlace vivo
    // mesmo quando o estado desejado da bomba nao mudou.
    static constexpr uint32_t kHeartbeatPeriodMs = 15'000;
};

// ---- Deep sleep (reservatorio, alimentado por bateria) --------------------
struct SleepPolicy {
    // Dorme entre ciclos apenas quando em bateria. Quando 0, o no permanece
    // acordado (usado em fonte externa ou durante o comissionamento).
    static constexpr uint32_t kDeepSleepMs = 0;  // TODO: habilitar apos validar
};

}  // namespace cfg
