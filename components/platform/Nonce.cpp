// ============================================================================
//  Nonce.cpp  -  Fonte de nonce (default em RAM; substituivel).
// ============================================================================

#include "platform/Nonce.h"

namespace platform {

namespace {
NonceSource g_source = nullptr;
uint32_t    g_counter = 0;  // contador em RAM (default)
}  // namespace

uint32_t nextNonce() {
    if (g_source != nullptr) return g_source();
    return ++g_counter;  // 1, 2, 3, ... estritamente crescente (wrap em 2^32)
}

void setNonceSource(NonceSource source) { g_source = source; }

}  // namespace platform
