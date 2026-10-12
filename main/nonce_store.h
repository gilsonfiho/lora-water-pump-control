#pragma once
// ============================================================================
//  nonce_store.h  -  Fonte de nonce persistida em NVS (anti-replay, #7).
// ----------------------------------------------------------------------------
//  Instala em platform::setNonceSource um contador que SOBREVIVE a reboots.
//  Sem isso, o contador voltaria a zero a cada reset do transmissor e o
//  receptor passaria a rejeitar tudo como "valor <= ultimo visto".
// ============================================================================

namespace app {

// Chame uma vez no boot, antes de subir o no. Se o NVS falhar, loga e deixa o
// default em RAM (a bancada ainda funciona; so nao persiste entre reboots).
void installPersistentNonce();

}  // namespace app
