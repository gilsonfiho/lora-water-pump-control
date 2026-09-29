#pragma once
// ============================================================================
//  node_app.h  -  Ponto de entrada de cada papel.
// ----------------------------------------------------------------------------
//  Cada funcao monta os objetos do seu no e entra no laco principal. Nenhuma
//  delas retorna.
// ============================================================================

namespace app {

[[noreturn]] void runReservoir();
[[noreturn]] void runPump();

}  // namespace app
