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

- Nada por enquanto.

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
