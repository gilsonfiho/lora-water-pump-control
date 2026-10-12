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

### Hardware (hw-v0.1, em desenvolvimento)
- Projeto KiCad 10 das duas placas, **emissor** e **receptor**, em `hardware/`:
  esquemático hierárquico (MCU, rádio E220-900T30D, alimentação, E/S), footprints e
  modelos 3D próprios, posicionamento em placa de 100 x 76 mm e renders/STEP em
  `hardware/fab/`. Ainda sem roteamento e sem tag `hw-v0.1`.

Nada mergeado aguardando versão. O backlog aberto está abaixo.

### Pendências conhecidas
- **Regulatório:** em canal fixo de 125 kHz, o E220 só se enquadra no limite
  genérico da ANATEL (≈ −1 dBm EIRP). Alta potência exige salto de frequência,
  e isso afeta a escolha do rádio e o SKU T30D.
- **Gestão/rotação de chave do MAC ([#6]).** Hoje a chave é única e estática em
  `config/SecurityConfig.h`. Falta provisão por nó/SKU e rotação.
- `kFloatHigh` (GPIO 2, strap) e `kExternalPresent` (GPIO 18, USB D−) precisam
  de outros pinos antes de voltar às boias e ao monitor de bateria real.
- Roteiro de bancada (`docs/context/bancada.md`) ainda não executado.
- **Validação em hardware** pendente em todas as versões `0.0.x`: até aqui tudo
  foi verificado por compilação (ESP-IDF v6.1) e pela suíte Unity no QEMU.

## [0.0.5] - 2026-10-11

Autenticação e anti-replay do enlace. Compila com ESP-IDF v6.1 para o ESP32-C3;
68/68 testes no QEMU. **Não validado em hardware.**

### Segurança
- **Autenticação e anti-replay no enlace** ([#5], [#7]). Todo quadro passa a
  levar um **MAC de 8 bytes** (SipHash-2-4 com chave pré-compartilhada de 128
  bits) e um **contador monotônico de 4 bytes**. Quem recebe só aceita o quadro
  se o MAC confere e se o contador é maior que o último visto daquele `src` —
  rejeita, então, tanto tráfego forjado (não tem a chave) quanto a **repetição**
  de um quadro capturado no ar. O `PumpController` também passou a descartar
  qualquer pacote cujo `src` não seja o reservatório esperado. O contador é
  atribuído por transmissão física (não por transação), via
  `platform::nextNonce()`, de modo que uma **retransmissão legítima** do mesmo
  comando usa um contador novo e nunca é confundida com replay; os comandos são
  idempotentes. No app, o contador é persistido em NVS ao estilo *frame counter*
  do LoRaWAN, dando salto de reserva a cada boot para nunca reusar valor após
  reset. Fecha a lacuna que o failsafe #9 só **limitava**: agora um comando
  forjado ou repetido é **rejeitado**, não apenas contido. Chave de exemplo em
  `config/SecurityConfig.h` com `TODO(hw)` para injeção fora do controle de
  versão. Não cobre a gestão/rotação de chave (#6).

### Alterado (incompatível)
- **Formato do quadro `VER 0x01` → `0x02`.** O cabeçalho ganhou o contador
  monotônico de 4 bytes e o quadro ganhou o MAC de 8 bytes (entre o payload e o
  CRC-16, que continua cobrindo tudo para enquadramento). `encodePacket()` e
  `decodePacket()` agora exigem a chave de 128 bits, e `decodePacket()` devolve
  o motivo da recusa (`DecodeError`: enquadramento, CRC, MAC). Os dois nós
  precisam da mesma chave e da mesma versão de firmware; com versões diferentes,
  não se comunicam.

### Adicionado
- `platform::nextNonce()` / `setNonceSource()`: fonte de contador substituível
  (default em RAM para os testes; o app instala a versão persistida em NVS).
- `main/nonce_store`: contador anti-replay persistido em NVS com salto de
  reserva por boot.
- **Testes de segurança** na suíte: vetores oficiais do SipHash-2-4, MAC
  (adulteração com CRC recalculado e chave errada), anti-replay (contador
  repetido/antigo rejeitado, retransmissão legítima aceita) e descarte de `src`
  inesperado no `PumpController`. A suíte foi de 60 para **68 testes**.

## [0.0.4] - 2026-10-10

Failsafe de tempo máximo de bombeamento, mensagem de status da bomba e a
primeira suíte de testes automatizados. Quadro ainda em `VER 0x01`.

### Segurança
- **Failsafe de tempo máximo de bombeamento no nó da bomba** ([#9]). A bomba
  desliga sozinha ao passar de `kMaxPumpRunMs` ligada de forma contínua (60 s na
  bancada, 30 min em campo), **mesmo com o enlace perfeito e o reservatório
  insistindo em LIGAR**. Cobre boia travada, comando `ON` preso por bug e
  comando forjado. O bloqueio é um latch: sobrevive à perda de enlace e só
  rearma com um `DESLIGAR` do reservatório. Limita o dano de um comando forjado,
  mas não substitui autenticação (resolvida em 0.0.5; ver #5, #6, #7).

### Adicionado (protocolo)
- **`BOMBA_STATUS` (0x06), bomba → reservatório:** estado do nó, bloqueio por
  tempo máximo, estado da saída e segundos ligada. Informativo, sem ACK. O
  formato do quadro não mudou nesta versão: `VER` seguiu em `0x01` e um nó antigo
  apenas ignora o tipo desconhecido. O reservatório apenas loga esse status.

### Adicionado (testes)
- **Suíte de 60 testes (Unity) rodando no QEMU, sem hardware**, em `test/`.
  Cobre `protocol/` (CRC, quadro, mensagens, ACK/retry/timeout do `LinkLayer`,
  par de nós em loopback) e `core/` (os dois failsafes da bomba, o latch da
  issue #9 e a histerese do reservatório). Dublês em `test/main/fakes.h`. Rodar
  com `.\test\run-tests.ps1` (runner com resumo final e parada automática).
- **`platform::setTimeSource()`**: troca a fonte de tempo de `platform::millis()`.
  Existe para os testes verificarem prazos de minutos sem esperar; em produção
  `millis()` continua no `esp_timer`.

### Corrigido
- **`LinkLayer`: heartbeat e auto-ACK adiavam a retransmissão do comando.**
  `transmit()` atualizava o `lastTxMs_`, que é o relógio da transação confiável
  em voo, e não de qualquer TX. Com isso, um `sendOnce()` (heartbeat,
  telemetria) ou o auto-ACK de um pacote recebido reiniciavam a espera do ACK —
  na cadência da bancada, o comando podia nunca ser retransmitido nem falhar.
  Agora só `sendReliable()` e a retransmissão mexem nesse relógio. Achado pelos
  testes novos.
- **`PumpController::begin()` transmitia status antes de o rádio existir.** A app
  chama `begin()` antes de `radio.begin()`, porque o relé tem de ir ao estado
  seguro antes de qualquer espera de hardware. O `begin()` passava por `enter()`,
  que reportava `BOMBA_STATUS` por uma UART ainda não instalada, e o driver
  logava `uart driver error` em todo boot — ruído em quem depura enlace pelo log.
  O estado inicial agora é definido direto, e o primeiro status sai na primeira
  transição real.
- **`BOMBA_STATUS` reporta o tempo ligada também no bloqueio** (antes ia zerado
  em `kOverrun`, justamente onde o número importa).

### Adicionado (desenvolvimento)
- Skills locais C++/ESP-IDF em `.claude/skills/` (simplicidade, planejador e
  engenheiro sênior) e a regra de **nunca** usar trailer de co-autor em commits
  e PRs (2026-09-26).

## [0.0.3] - 2026-09-20

Instrumentação para investigar o "não comunica" na bancada.

### Adicionado
- **Log de diagnóstico do enlace:**
  - bytes crus em `E220Radio` (`TX/RX N B` + hexdump, nível `DEBUG`);
  - eventos de protocolo no `LinkLayer` via `onEvent(std::function<...>)` —
    `kTx`, `kAckOk` (com rtt), `kRetry`, `kTimeout`, `kCrcFail`, `kNotForMe` —
    logados na camada de app, mantendo `protocol/` sem ESP-IDF.

## [0.0.2] - 2026-09-19

Migração para ESP-IDF e montagem do protótipo de bancada. **Compila** com
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
  - potência de TX `kLow` via `cfg::kTxPower`: 21 dBm no T30D, que é o mínimo
    do módulo. Em campo volta a `kMax`.
- **Par E220-900T30D no protótipo** (`makeProfile()` dos dois nós). Na bancada:
  nós a ≥ 3 m, porque o receptor queima com mais de +10 dBm na entrada, e VCC
  do módulo no pino 5V do devkit com 1000 µF. O par T22D fica para a comparação
  de campo.
- `sdkconfig.defaults` do projeto.
- Documentação: `docs/context/` (status, decisões, hardware, protocolo e
  bancada), datasheets do E220 em `docs/datasheets/` e a análise regulatória da
  ANATEL (Ato 14.448/2017) em `docs/context/hardware.md`.

> **Nota de versionamento.** A tag `v0.0.2` existente aponta para a branch
> abandonada `archive/freertos-esp-idf` (tentativa com tarefas FreeRTOS que não
> seguiu adiante). A linha que vingou foi a `feature/prototipo-bancada`, aqui
> documentada. Mesmo marco (migração ESP-IDF), implementação diferente.

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

<!-- Issues -->
[#5]: https://github.com/gilsonfiho/lora-water-pump-control/issues/5
[#6]: https://github.com/gilsonfiho/lora-water-pump-control/issues/6
[#7]: https://github.com/gilsonfiho/lora-water-pump-control/issues/7
[#9]: https://github.com/gilsonfiho/lora-water-pump-control/issues/9

<!-- Links de comparação entre versões -->
[Não lançado]: https://github.com/gilsonfiho/lora-water-pump-control/compare/v0.0.5...HEAD
[0.0.5]: https://github.com/gilsonfiho/lora-water-pump-control/compare/v0.0.4...v0.0.5
[0.0.4]: https://github.com/gilsonfiho/lora-water-pump-control/compare/v0.0.3...v0.0.4
[0.0.3]: https://github.com/gilsonfiho/lora-water-pump-control/compare/v0.0.2...v0.0.3
[0.0.2]: https://github.com/gilsonfiho/lora-water-pump-control/compare/v0.0.1...v0.0.2
[0.0.1]: https://github.com/gilsonfiho/lora-water-pump-control/releases/tag/v0.0.1
