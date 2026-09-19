# ============================================================================
#  Projeto ESP-IDF  -  Controle de bomba d'agua a distancia (ESP32-C3 + LoRa).
# ----------------------------------------------------------------------------
#  Um unico binario atende os dois nos. O papel (reservatorio ou bomba) e lido
#  no boot a partir de cfg::RolePins::kRoleStrap -- ver main/main.cpp.
#
#  Build:  idf.py set-target esp32c3 && idf.py build
#  Gravar: idf.py -p COMx flash monitor
# ============================================================================

cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(lora-water-pump-control)
