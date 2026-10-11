// ============================================================================
//  test_main.cpp  -  Entrada do app de testes (Unity).
// ----------------------------------------------------------------------------
//  Roda todos os TEST_CASE registrados e imprime o resumo do Unity. Sem menu
//  interativo: o QEMU roda sem console de entrada.
//
//  A ultima linha ("==== FIM DOS TESTES") e o marcador que run-tests.ps1 usa
//  para saber que acabou, encerrar o QEMU e devolver o codigo de saida. Sem
//  ela, o emulador ficaria rodando para sempre depois do ultimo caso.
// ============================================================================

#include <stdio.h>

#include <unity.h>
#include <unity_test_runner.h>

#include "fakes.h"

namespace test {
uint32_t FakeClock::nowMs_ = 0;
}  // namespace test

extern "C" void app_main() {
    printf("\n==== testes do firmware de controle de bomba ====\n");

    UNITY_BEGIN();
    unity_run_all_tests();
    const int failures = UNITY_END();  // imprime "N Tests M Failures ..."

    printf("\n==== FIM DOS TESTES: %d falha(s) ====\n", failures);
    fflush(stdout);
}
