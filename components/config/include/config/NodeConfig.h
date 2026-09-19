#pragma once
// ============================================================================
//  NodeConfig.h  -  Enderecos logicos, papeis e temporizacao dos dois nós.
// ----------------------------------------------------------------------------
//  Estes sao valores da camada de aplicacao carregados dentro do nosso proprio
//  cabecalho de pacote; sao independentes dos registradores de endereco do
//  E220 (o radio roda em modo transparente e o enderecamento e feito por nós).
// ============================================================================

#include <cstdint>

namespace cfg {

// Enderecos logicos dos nós (1 byte cada, 0x00 e 0xFF reservados).
enum NodeAddress : uint8_t {
    kAddrReservoir = 0x01,
    kAddrPump      = 0x02,
    kAddrBroadcast = 0xFF,
};

// Perfil de BANCADA: tempos curtos para o operador ver a reacao em segundos, em
// vez de esperar quase um minuto. Zere esta macro (ou defina -DBENCH_PROFILE=0)
// para voltar aos tempos de campo.
#ifndef BENCH_PROFILE
#define BENCH_PROFILE 1
#endif

// ---- Temporizacao (milissegundos) -----------------------------------------
struct Timing {
#if BENCH_PROFILE
    // Reservatorio: com que frequencia reavaliar o nivel e reenviar o comando.
    static constexpr uint32_t kReservoirCyclePeriodMs = 2'000;   // campo: 10'000

    // Reservatorio: com que frequencia reportar o status da bateria.
    static constexpr uint32_t kBatteryReportPeriodMs = 15'000;   // campo: 60'000

    // Bomba: se nenhum comando/heartbeat valido chegar dentro desta janela,
    // o enlace e considerado perdido e a bomba e DESLIGADA (failsafe).
    // Encurtado para que desligar o transmissor mostre o failsafe em ~15 s.
    static constexpr uint32_t kLinkLostTimeoutMs = 15'000;       // campo: 45'000

    // Cadencia do heartbeat do reservatorio -> bomba para manter o enlace vivo
    // mesmo quando o estado desejado da bomba nao mudou.
    static constexpr uint32_t kHeartbeatPeriodMs = 5'000;        // campo: 15'000
#else
    static constexpr uint32_t kReservoirCyclePeriodMs = 10'000;
    static constexpr uint32_t kBatteryReportPeriodMs  = 60'000;
    static constexpr uint32_t kLinkLostTimeoutMs      = 45'000;
    static constexpr uint32_t kHeartbeatPeriodMs      = 15'000;
#endif

    // Camada de enlace: espera por um ACK antes de retransmitir. Igual nos dois
    // perfis -- depende do tempo de ar, nao da conveniencia do operador.
    // A 2,4 kbps um quadro pequeno leva algumas centenas de ms ida e volta;
    // se o teste de bancada mostrar retransmissao sem perda real, este e o
    // primeiro numero a revisar.
    static constexpr uint32_t kAckTimeoutMs = 800;
    static constexpr uint8_t  kMaxRetries   = 4;

    // O failsafe so faz sentido se a bomba tiver varias chances de ouvir um
    // heartbeat antes de desistir do enlace.
    static_assert(kLinkLostTimeoutMs >= 3 * kHeartbeatPeriodMs,
                  "janela de failsafe curta demais para a cadencia do heartbeat");
};

// ---- Deep sleep (reservatorio, alimentado por bateria) --------------------
struct SleepPolicy {
    // Dorme entre ciclos apenas quando em bateria. Quando 0, o no permanece
    // acordado (usado em fonte externa ou durante o comissionamento).
    static constexpr uint32_t kDeepSleepMs = 0;  // TODO: habilitar apos validar
};

}  // namespace cfg
