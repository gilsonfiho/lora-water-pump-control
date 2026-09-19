# Controle de Bomba D'água à Distância — ESP32-C3 + LoRa

Sistema de controle remoto de bomba d'água com dois nós sem fio de longo
alcance (até ~5 km), baseado em **ESP32-C3** e módulos **LoRa EBYTE E220**
(900 MHz). O nó do reservatório mede o nível da caixa d'água e comanda, via
LoRa, o nó da bomba na captação — com confirmação (ACK), retransmissão e
**failsafe de segurança** (a bomba desliga se o enlace cair).

![Diagrama de blocos da solução](assets/block-diagram.svg)

> **Status do projeto:** 🟡 `v0.0.1` (estruturação, pré-alpha) — esqueleto
> funcional. Arquitetura, protocolo e drivers principais implementados. Pinagem
> e modelo exato dos módulos ainda precisam ser confirmados no hardware —
> procure por `TODO(hw)` no código. Histórico em [`CHANGELOG.md`](CHANGELOG.md).

---

## Sumário

- [Visão geral](#visão-geral)
- [Hardware necessário](#hardware-necessário)
- [Estrutura do repositório](#estrutura-do-repositório)
- [Como compilar e gravar](#como-compilar-e-gravar)
- [Configuração](#configuração)
- [Protocolo de comunicação](#protocolo-de-comunicação)
- [Documentação adicional](#documentação-adicional)
- [Pendências (TODOs de hardware)](#pendências-todos-de-hardware)

---

## Visão geral

| Nó | Papel | Componentes | Seleção |
|----|-------|-------------|---------|
| **Reservatório** | Transmissor | ESP32-C3, boia de nível, E220 | strap aberto |
| **Bomba** | Receptor / atuador | ESP32-C3, relé/contator, E220 | strap no GND |

**Lógica de controle (com histerese das boias):**

- Caixa **baixa** → comando **LIGAR** bomba (encher).
- Caixa **cheia** → comando **DESLIGAR** bomba.
- Nível intermediário → mantém o comando anterior (evita liga-desliga rápido).
- Falha de sensor **ou perda de enlace** → bomba **DESLIGADA** (seguro).

## Hardware necessário

- 2× **ESP32-C3** (RISC-V, Wi-Fi + BLE, baixo consumo).
- 2× módulo LoRa **EBYTE E220-900T30D** (30 dBm / 1 W) **ou** **E220-900T22D**
  (22 dBm / 160 mW) — o firmware suporta ambos (só muda a tabela de potência).
  > ⚠️ Os E220 são **UART/TTL**, não SPI. A camada HAL abstrai isso; um driver
  > SPI SX1276/RFM95 existe como **segunda estratégia** (esqueleto).
- Sensor de nível: **boia** (padrão) — 1 ou 2 boias para histerese.
  Ultrassônico é **opcional** (flag `USE_ULTRASONIC_SENSOR`).
- 1× **relé/contator** dimensionado para a bomba (no nó da bomba).
- Alimentação **dual com failover** por nó:
  - Fonte externa (adaptador 5 V / 12 V) **+**
  - Bateria de lítio 18650 com carregador/proteção (**TP4056** ou similar);
  - Chaveamento automático fonte↔bateria (ideal-diode/diodos);
  - Monitor de tensão/corrente (ADC via divisor, ou **INA219/INA226** por I2C).
- Antenas SMA adequadas à faixa 900 MHz (ganho ~2–3 dBi).

### Ligações dos nós

| Reservatório (transmissor) | Bomba (receptor/atuador) |
|:--------------------------:|:------------------------:|
| ![Ligações do reservatório](assets/schematic-reservoir.svg) | ![Ligações da bomba](assets/schematic-pump.svg) |

> Esquemático completo, pinagem detalhada e notas de projeto:
> [`assets/schematic.md`](assets/schematic.md).

## Estrutura do repositório

```
lora-water-pump-control/
├── CMakeLists.txt              # projeto ESP-IDF
├── sdkconfig.defaults          # alvo esp32c3 + opções do projeto
├── README.md
├── CHANGELOG.md                # histórico de versões (SemVer)
├── docs/
│   ├── ARCHITECTURE.md         # decisões, camadas, protocolo, máquinas de estado
│   ├── context/                # notas de contexto (decisões, hardware, status)
│   └── datasheets/             # manuais dos módulos E220
├── assets/
│   ├── block-diagram.md/.svg   # diagrama de blocos da solução
│   ├── schematic.md            # esquemático elétrico (imagens + Mermaid + texto)
│   ├── schematic-power.svg     # subsistema de energia (failover + TP4056)
│   ├── schematic-reservoir.svg # ligações do nó do reservatório
│   └── schematic-pump.svg      # ligações do nó da bomba
├── components/                 # código compartilhado, em camadas
│   ├── config/                 # pinos, endereços, timeouts, perfil de RF
│   ├── platform/               # relógio monotônico (isola o ESP-IDF)
│   ├── hal/                    # abstração de hardware (interfaces + drivers)
│   ├── protocol/               # pacote, CRC, camada de enlace (ACK/retry)
│   ├── core/                   # máquinas de estado + Observer
│   └── power/                  # deep sleep + failover de energia
└── main/
    ├── main.cpp                # lê o strap e assume o papel do nó
    ├── reservoir_app.cpp       # laço do nó do reservatório
    └── pump_app.cpp            # laço do nó da bomba
```

As dependências entre camadas são declaradas em `REQUIRES`, no `CMakeLists.txt`
de cada componente — o build **impede** que `core/` ou `protocol/` alcancem um
driver concreto.

## Como compilar e gravar

Requisitos: [ESP-IDF](https://docs.espressif.com/projects/esp-idf/) v5.x (CLI ou
a extensão do VS Code).

**Um único binário serve aos dois nós.** O papel é lido no boot pelo pino
`cfg::RolePins::kRoleStrap`:

| Strap | Papel |
|-------|-------|
| aberto | reservatório (transmissor) |
| ligado ao GND | bomba (receptor/atuador) |

O aberto virar reservatório é proposital: é o papel que não aciona o relé, então
um strap solto nunca faz um nó assumir o comando da bomba.

```bash
idf.py set-target esp32c3
idf.py build

# Gravar e abrir o monitor serial
idf.py -p COM3 flash monitor
```

## Configuração

Toda a configuração fica em [`components/config/include/config/`](components/config/include/config):

| Arquivo | O que define |
|---------|--------------|
| `PinConfig.h` | Pinos físicos (strap de papel, UART do E220, M0/M1/AUX, boias, relé, ADC, I2C). |
| `NodeConfig.h` | Endereços lógicos dos nós, timeouts, retries, deep sleep. |
| `RadioConfig.h` | Canal, taxa aérea, potência, variante do E220 (T30D/T22D). |

**Sensor de nível:** o alvo de campo é a **boia** (`FloatSwitchLevelSensor`), e o
ultrassônico é opcional. O protótipo de bancada usa uma **chave manual**
(`BenchSwitchLevelSensor`) — ver [`docs/context/bancada.md`](docs/context/bancada.md).

> ⚠️ **A branch `feature/prototipo-bancada` está em modo bancada:** chave no
> lugar da boia, LED no lugar do contator, bateria stubada e tempos curtos
> (`BENCH_PROFILE` em `NodeConfig.h`). Não é a configuração de campo.

## Protocolo de comunicação

Protocolo próprio, ponto-a-ponto, com quadro de 8 bytes de cabeçalho + payload
+ CRC-16. Tipos: `STATUS_NIVEL`, `COMANDO_BOMBA`, `ACK`, `HEARTBEAT`,
`BATERIA_STATUS`. Detalhes (layout de bits, ACK/retry, failsafe) em
[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md#protocolo-de-comunicação).

## Documentação adicional

- **Histórico de versões:** [`CHANGELOG.md`](CHANGELOG.md)
- **Arquitetura e decisões:** [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)
- **Diagrama de blocos:** [`assets/block-diagram.md`](assets/block-diagram.md)
- **Esquemático elétrico:** [`assets/schematic.md`](assets/schematic.md) — energia
  ([SVG](assets/schematic-power.svg)), reservatório
  ([SVG](assets/schematic-reservoir.svg)) e bomba
  ([SVG](assets/schematic-pump.svg))

## Pendências (TODOs de hardware)

Decisões que dependem do hardware final estão marcadas com `TODO(hw)` no código:

- [ ] Pinagem exata do ESP32-C3 (UART, M0/M1/AUX, relé, boias, ADC, I2C).
- [ ] Modelo do módulo por nó: **E220-900T30D** ou **E220-900T22D**.
- [ ] Nível lógico do módulo relé (ativo alto/baixo) e da(s) boia(s).
- [ ] Razão do divisor resistivo do ADC de bateria (ou adoção do INA219/226).
- [ ] Geometria da caixa, caso o sensor ultrassônico seja usado.
- [ ] Habilitar/validar o deep sleep e a chave de acordar por evento.
