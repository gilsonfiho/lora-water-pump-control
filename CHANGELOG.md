# Changelog

Todas as mudanças notáveis deste projeto são documentadas neste arquivo.

O formato segue o [Keep a Changelog](https://keepachangelog.com/pt-BR/1.1.0/)
e o projeto adota o [Versionamento Semântico](https://semver.org/lang/pt-BR/).

> **Fase `0.0.x` — estruturação (pré-alpha).** Ainda não é uma versão de uso; a
> API interna (interfaces da HAL, formato de pacote, protocolo) pode mudar
> livremente. Escada planejada:
> - `0.0.x` — estruturação e esqueleto (aqui);
> - `0.1.0` — primeiro **alpha**: comunicação validada em bancada;
> - `0.x.0-beta` — testes de campo;
> - `1.0.0` — estável, **validado em hardware real**.

## [Não lançado]

Migração para ESP-IDF e preparação do protótipo de bancada. **Compila** com
ESP-IDF v6.1 para o ESP32-C3, mas ainda **não foi validado em hardware**.

### Alterado (incompatível)
- **Build migrado de PlatformIO/Arduino para ESP-IDF** (v6.1, alvo `esp32c3`).
  As camadas viraram componentes do IDF (`components/<camada>/include/<camada>/`),
  com as dependências declaradas em `REQUIRES`. `platformio.ini` e `src/` foram
  removidos.
- **Firmware único para os dois nós.** O papel vem do strap no GPIO 3 (aberto =
  reservatório, GND = bomba). Os dois ambientes de build deixaram de existir.
- **Camada `hal` renomeada para `hw`** (diretório, `#include "hw/..."` e
  namespace `hw::`). Com o nome `hal`, o componente substituía o `hal` interno do
  ESP-IDF e quebrava o build do framework.
- **Canal de RF: 23 (873,125 MHz) → 55 (905,125 MHz).** O canal de fábrica do
  E220 cai no downlink celular de 850 MHz e não é permitido pela ANATEL. Um
  `static_assert` impede canais fora das faixas de 902–907,5 e 915–928 MHz.
  **Os dois nós precisam ser regravados juntos.** Em canais diferentes, eles não
  se comunicam.

### Adicionado
- Componente `platform/` (`platform::millis()` / `platform::delayMs()`), que
  mantém `protocol/` e `core/` independentes do ESP-IDF.
- Drivers reescritos sobre `esp_driver_uart`, `esp_driver_gpio` e `esp_adc`:
  `E220Radio`, `RelayPumpActuator`, `FloatSwitchLevelSensor`, `AdcBatteryMonitor`.
- **Perfil de bancada (`BENCH_PROFILE`, ligado por padrão):**
  - `BenchSwitchLevelSensor`: uma chave que trava no lugar das boias;
  - `StubBatteryMonitor`: bateria fixa em fonte externa, 100%;
  - tempos curtos: ciclo de 2 s, heartbeat de 5 s, failsafe de 15 s. A proporção
    failsafe ÷ heartbeat de 3:1 é garantida por `static_assert`;
  - potência de TX `kLow` (10 dBm no T22D), via `cfg::kTxPower`. Evita saturar o
    LLCC68 com os módulos próximos. Em campo volta a `kMax`.
- `sdkconfig.defaults` do projeto.
- Documentação: `docs/context/` (status, decisões, hardware, protocolo e
  bancada), datasheets do E220 em `docs/datasheets/` e a análise regulatória da
  ANATEL (Ato 14.448/2017) em `docs/context/hardware.md`.

### Pendências conhecidas
- **Regulatório:** em canal fixo de 125 kHz, o E220 só se enquadra no limite
  genérico da ANATEL (≈ −1 dBm EIRP). Alta potência exige salto de frequência,
  e isso afeta a escolha do rádio e o SKU T30D.
- `kFloatHigh` (GPIO 2, strap) e `kExternalPresent` (GPIO 18, USB D−) precisam
  de outros pinos antes de voltar às boias e ao monitor de bateria real.
- Roteiro de bancada (`docs/context/bancada.md`) ainda não executado.

## [0.0.1] - 2026-09-17

Estruturação inicial do projeto: esqueleto funcional dos dois nós (reservatório
e bomba), arquitetura em camadas, protocolo de comunicação próprio e
documentação. Compila via PlatformIO, porém ainda **não validado em hardware**
(pinagem e modelo dos módulos marcados como `TODO(hw)`).

### Adicionado

#### Build e configuração
- Projeto PlatformIO com dois ambientes (`reservoir_node`, `pump_node`) para
  ESP32-C3 no framework Arduino.
- Camada `config/`: pinos (`PinConfig`), endereços/timeouts (`NodeConfig`) e
  perfil de RF (`RadioConfig`, canal/taxa/potência/variante T30D-T22D).
- Flag de compilação `USE_ULTRASONIC_SENSOR` (boia é o padrão).

#### HAL — abstração de hardware (padrão Strategy)
- Interfaces: `ILoRaRadio`, `ILevelSensor`, `IPumpActuator`, `IBatteryMonitor`.
- `E220Radio`: driver UART do EBYTE E220-900T30D/T22D (modos M0/M1, linha AUX,
  configuração via comando `C0`, modo transparente) — implementado do datasheet.
- `SX127xRadio`: esqueleto do driver SPI (SX1276/RFM95) como segunda estratégia.
- `FloatSwitchLevelSensor` (boia com histerese) e `UltrasonicLevelSensor`
  (opcional, esqueleto).
- `RelayPumpActuator`: acionamento por relé/contator, sempre iniciando desligado.
- `AdcBatteryMonitor` (tensão via divisor) e `Ina219BatteryMonitor` (esqueleto I2C).

#### Protocolo de comunicação
- `Packet` + `Crc16`: quadro com SYNC, cabeçalho de 8 bytes, payload e CRC-16.
- `Messages`: tipos `STATUS_NIVEL`, `COMANDO_BOMBA`, `ACK`, `HEARTBEAT`,
  `BATERIA_STATUS` com builders/parsers tipados.
- `LinkLayer`: enlace ponto-a-ponto não bloqueante com ACK, retransmissão,
  timeout e auto-ACK.

#### Core — lógica da aplicação (State Machine + Observer)
- `ReservoirController`: leitura de nível, decisão com histerese e envio
  confiável do comando; heartbeat e telemetria de bateria.
- `PumpController`: aplicação do comando no relé com **failsafe** (desliga a
  bomba se o enlace ficar em silêncio além do timeout).
- `Observer`/`Signal`: eventos desacoplados (ex.: mudança de nível).

#### Energia
- `PowerManager`: observação da fonte ativa (failover fonte/bateria), leitura
  de telemetria e deep sleep do ESP32-C3.

#### Firmware dos nós
- `src/reservoir_node/main.cpp` e `src/pump_node/main.cpp` integrando as camadas.

#### Documentação
- `README.md`: visão geral, hardware, build, configuração e TODOs.
- `docs/ARCHITECTURE.md`: decisões, camadas, protocolo e máquinas de estado.
- `assets/`: diagrama de blocos e esquemáticos elétricos (energia/TP4056,
  reservatório e bomba) em SVG + fonte Mermaid.

### Pendências conhecidas (`TODO(hw)`)
- Pinagem exata do ESP32-C3 e modelo do módulo por nó (T30D ou T22D).
- Nível lógico do relé e das boias; razão do divisor do ADC (ou uso do INA219).
- Validação em hardware, deep sleep e wake-up por evento.

<!-- Links de comparação: adicionar quando houver repositório remoto, ex.:
[Não lançado]: https://github.com/<org>/<repo>/compare/v0.0.1...HEAD
[0.0.1]: https://github.com/<org>/<repo>/releases/tag/v0.0.1
-->
