// ============================================================================
//  test_main.cpp  -  Entrada do app de testes (Unity).
// ----------------------------------------------------------------------------
//  Roda todos os TEST_CASE registrados e imprime o resumo. Sem menu
//  interativo: o QEMU roda sem console de entrada.
// ============================================================================

#include <unity.h>
#include <unity_test_runner.h>

#include "fakes.h"

namespace test {
uint32_t FakeClock::nowMs_ = 0;
}  // namespace test

extern "C" void app_main() {
    printf("\n==== testes do firmware de controle de bomba ====\n");
    unity_run_all_tests();
}
