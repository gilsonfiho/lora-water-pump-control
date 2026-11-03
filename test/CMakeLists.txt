# ============================================================================
#  Projeto de TESTES (Unity). Roda no QEMU, sem placa:
#
#      idf.py -C test set-target esp32c3
#      idf.py -C test build qemu --qemu-extra-args "-nographic"
#
#  Reaproveita os componentes de producao em ../components. O relogio e
#  substituido por um contador controlado pelo teste (platform::setTimeSource),
#  entao prazos de minutos sao verificados em microssegundos.
# ============================================================================

cmake_minimum_required(VERSION 3.16)

set(EXTRA_COMPONENT_DIRS "../components")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(lora-water-pump-tests)
