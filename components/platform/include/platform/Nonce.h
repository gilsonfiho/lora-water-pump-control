#pragma once
// ============================================================================
//  Nonce.h  -  Nonce monotonico de 32 bits para o contador anti-replay.
// ----------------------------------------------------------------------------
//  Cada pacote leva um contador estritamente crescente (issue #7). O receptor
//  rejeita valores <= ao ultimo visto daquele remetente, barrando reproducao
//  de pacotes capturados.
//
//  A fonte vive aqui, na platform/, porque protocol/ nao pode depender do
//  ESP-IDF. O default e um contador em RAM (serve aos testes e a bancada). Em
//  campo, o app instala uma fonte PERSISTIDA (NVS): sem isso, um reboot do
//  transmissor zeraria o contador e o receptor passaria a rejeitar tudo.
// ============================================================================

#include <cstdint>

namespace platform {

// Proximo nonce. Estritamente crescente dentro de uma execucao.
uint32_t nextNonce();

// Troca a fonte do nonce. Em producao, uma fonte persistida (NVS). nullptr
// restaura o contador em RAM. Mesma ideia do setTimeSource.
using NonceSource = uint32_t (*)();
void setNonceSource(NonceSource source);

}  // namespace platform
