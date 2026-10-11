#pragma once
// ============================================================================
//  SipHash.h  -  SipHash-2-4: PRF com chave para o MAC do pacote.
// ----------------------------------------------------------------------------
//  Por que SipHash e nao HMAC-SHA256: e uma funcao com chave desenhada para
//  mensagens curtas, rapida e com ~40 linhas, sem dependencia externa -- cabe
//  na camada protocol/, que nao pode usar o ESP-IDF (mbedTLS). Dá um MAC de
//  64 bits, suficiente para autenticar comandos neste enlace ponto-a-ponto.
//
//  Referencia: Aumasson & Bernstein, "SipHash: a fast short-input PRF" (2012).
//  Chave de 128 bits (16 bytes). Little-endian, como a especificacao.
// ============================================================================

#include <cstddef>
#include <cstdint>

namespace protocol {

inline uint64_t siphashRotl(uint64_t x, int b) {
    return (x << b) | (x >> (64 - b));
}

// MAC de 64 bits de `data[0..len)` sob a chave de 16 bytes.
inline uint64_t siphash24(const uint8_t key[16], const uint8_t* data, size_t len) {
    uint64_t k0 = 0, k1 = 0;
    for (int i = 0; i < 8; ++i) {
        k0 |= static_cast<uint64_t>(key[i]) << (8 * i);
        k1 |= static_cast<uint64_t>(key[8 + i]) << (8 * i);
    }

    uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
    uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
    uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
    uint64_t v3 = 0x7465646279746573ULL ^ k1;

    auto round = [&]() {
        v0 += v1; v1 = siphashRotl(v1, 13); v1 ^= v0; v0 = siphashRotl(v0, 32);
        v2 += v3; v3 = siphashRotl(v3, 16); v3 ^= v2;
        v0 += v3; v3 = siphashRotl(v3, 21); v3 ^= v0;
        v2 += v1; v1 = siphashRotl(v1, 17); v1 ^= v2; v2 = siphashRotl(v2, 32);
    };

    const size_t endFullBlocks = len - (len % 8);
    size_t i = 0;
    for (; i < endFullBlocks; i += 8) {
        uint64_t m = 0;
        for (int j = 0; j < 8; ++j) m |= static_cast<uint64_t>(data[i + j]) << (8 * j);
        v3 ^= m;
        round(); round();  // c = 2 rodadas de compressao
        v0 ^= m;
    }

    // Ultimo bloco: bytes que sobraram + o comprimento no byte mais alto.
    uint64_t b = static_cast<uint64_t>(len) << 56;
    for (size_t j = 0; i + j < len; ++j) {
        b |= static_cast<uint64_t>(data[i + j]) << (8 * j);
    }
    v3 ^= b;
    round(); round();
    v0 ^= b;

    v2 ^= 0xff;
    round(); round(); round(); round();  // d = 4 rodadas de finalizacao
    return v0 ^ v1 ^ v2 ^ v3;
}

}  // namespace protocol
