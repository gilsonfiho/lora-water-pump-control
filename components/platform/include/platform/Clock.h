#pragma once
// ============================================================================
//  Clock.h  -  Tempo monotonico e espera cooperativa, sem Arduino.
// ----------------------------------------------------------------------------
//  As camadas protocol/ e core/ precisam apenas de "quantos ms se passaram".
//  Concentrar isso aqui mantem essas camadas livres de headers de plataforma,
//  o que permite compila-las e testa-las no host.
//
//  millis() reproduz a semantica do Arduino: contador monotonico de 32 bits em
//  milissegundos que transborda a cada ~49,7 dias. Todas as comparacoes de
//  prazo no projeto usam a forma (now - inicio) >= prazo, que continua correta
//  no transbordo por causa da aritmetica sem sinal.
// ============================================================================

#include <cstdint>

namespace platform {

// Milissegundos desde o boot. Monotonico; nao anda para tras em deep sleep.
uint32_t millis();

// Cede o processador por `ms`. Em FreeRTOS isso bloqueia apenas a tarefa
// chamadora; nao usar no caminho critico com valores grandes.
void delayMs(uint32_t ms);

// ---- Fonte de tempo substituivel (TESTES) ---------------------------------
// Troca de onde millis() le o tempo. Existe para os testes: um failsafe de
// 45 s ou um bloqueio de 30 min nao podem ser verificados esperando de
// verdade. O teste instala um contador que ele mesmo avanca e, com isso,
// exercita prazos longos em microssegundos de execucao.
//
// Em producao ninguem chama isto, e millis() usa o esp_timer. Passar nullptr
// restaura o relogio real.
using TimeSourceMs = uint32_t (*)();
void setTimeSource(TimeSourceMs source);

}  // namespace platform
