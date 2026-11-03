// ============================================================================
//  nonce_store.cpp  -  Contador anti-replay persistido em NVS.
// ----------------------------------------------------------------------------
//  Estrategia (estilo frame counter do LoRaWAN): a cada boot pula uma RESERVA
//  e grava a nova base de imediato; durante a execucao, grava a cada RESERVA
//  nonces. Assim o contador e sempre crescente entre reboots (nenhum valor se
//  repete) gastando no maximo 1 escrita por boot + 1 a cada RESERVA pacotes --
//  desgaste de flash desprezivel.
// ============================================================================

#include "nonce_store.h"

#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>

#include "platform/Nonce.h"

namespace app {

namespace {

constexpr char     kTag[]  = "nonce";
constexpr char     kNs[]   = "lwpc";
constexpr char     kKey[]  = "txnonce";
constexpr uint32_t kReserve = 64;

nvs_handle_t g_nvs = 0;
uint32_t     g_counter = 0;
uint32_t     g_lastSaved = 0;

void persist(uint32_t value) {
    if (nvs_set_u32(g_nvs, kKey, value) == ESP_OK) nvs_commit(g_nvs);
}

uint32_t nextPersistentNonce() {
    ++g_counter;
    if (g_counter - g_lastSaved >= kReserve) {
        persist(g_counter);
        g_lastSaved = g_counter;
    }
    return g_counter;
}

}  // namespace

void installPersistentNonce() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK || nvs_open(kNs, NVS_READWRITE, &g_nvs) != ESP_OK) {
        ESP_LOGE(kTag, "NVS indisponivel; nonce fica em RAM (nao persiste reboot)");
        return;
    }

    uint32_t saved = 0;
    nvs_get_u32(g_nvs, kKey, &saved);  // ESP_ERR_NVS_NOT_FOUND -> fica em 0
    g_counter = saved + kReserve;      // pula a reserva da sessao anterior
    g_lastSaved = g_counter;
    persist(g_counter);                // reserva ja gravada

    platform::setNonceSource(&nextPersistentNonce);
    ESP_LOGI(kTag, "nonce persistido ativo (base=%lu)",
             static_cast<unsigned long>(g_counter));
}

}  // namespace app
