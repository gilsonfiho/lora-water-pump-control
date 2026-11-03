#pragma once
// ============================================================================
//  SecurityConfig.h  -  Chave pre-compartilhada do enlace (MAC do pacote).
// ----------------------------------------------------------------------------
//  Os dois nos autenticam cada pacote com um MAC SipHash sob esta chave de
//  128 bits (issue #5). A chave NAO vai ao ar e so protege se for secreta.
//
//  TODO(hw): gerar uma chave aleatoria por par de nos (ou por produto) e
//  injeta-la fora do controle de versao -- Kconfig, eFuse ou provisionamento
//  de fabrica. O valor abaixo e um placeholder de bancada, publico, e NAO deve
//  ir para campo. Ambos os nos precisam da MESMA chave.
// ============================================================================

#include <cstdint>

namespace cfg {

struct Security {
    static constexpr uint8_t kLinkKey[16] = {
        // placeholder de bancada -- trocar antes de qualquer uso real
        0x9f, 0x2c, 0x74, 0x11, 0xe5, 0x8a, 0x63, 0xd0,
        0x4b, 0x1f, 0xa7, 0x36, 0xc2, 0x90, 0x5d, 0xe8,
    };
};

}  // namespace cfg
