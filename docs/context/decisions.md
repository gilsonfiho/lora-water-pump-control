# Decisões

Formato: data, decisão, motivo. Mais recentes no topo. Decisões anteriores à
criação deste arquivo foram extraídas de [`../ARCHITECTURE.md`](../ARCHITECTURE.md).

## Requisitos do produto

- Controle remoto ponto a ponto de bomba monofásica de 1–3 CV, a **2–10 km**,
  em terreno com vegetação.
- Obrigatórios: liga/desliga remoto e **confirmação** de que a bomba ligou.
- Produto com **múltiplos SKUs**, não projeto único: escalabilidade e
  conformidade **ANATEL** (limite de EIRP) valem para todos.

## Decisões de produto (kit chave bóia sem fio)

- **MCU: ESP32-C6-WROOM-1 nos dois lados.** Escolhido sobre C3 e S3 por folga de
  GPIO e RAM, homologação ANATEL e expansibilidade futura.
- **Rádio padrão: E220-900T22D (LLCC68).** Módulo homologável, UART simples.
  Trade-off de sensibilidade em relação ao SX1262 aceito de forma consciente
  (ver [`hardware.md`](hardware.md)).
- **Confirmação por corrente (SCT-013).** Comprova que a bomba está de fato
  consumindo, o que o estado do contator não garante.
- **Antena é decisão estrutural.** Ganho e altura definem o enlace acima de ~4 km.
  A escolha só fecha após o teste de campo.
- **Firmware único com strap de GPIO** para escolher o papel (comando ou atuador).
- **SKUs:** (1) padrão E220-T22D; (2) longo alcance E22-T30D, *pin-compatible*;
  (3) repetidor, planejado.

## Par T30D no protótipo (decidido em 2026-09-19)

Substitui a decisão anterior "Par do protótipo: T22D".

- **Os dois nós usam E220-900T30D.** O par T22D passa a ser a **comparação no
  teste de campo**, que mede na prática os 8 dB de diferença e alimenta a
  decisão do SKU padrão.
- **Alimentação na bancada: pino 5V (VBUS) do devkit.** É um improviso aceito
  conscientemente. A porta USB 2.0 fornece 500 mA, e o T30D puxa 620 mA de pico
  a 30 dBm (menos em `kLow`). Exige eletrolítico de 1000 µF junto ao VCC do
  módulo. Se houver reset ou queda da porta USB durante TX, o próximo passo é
  uma fonte de 5 V externa com GND comum.
- **Potência na bancada: `kLow` = 21 dBm, com os nós a ≥ 3 m.** O T30D não
  desce abaixo de 21 dBm. O manual limita a entrada do receptor a **+10 dBm, sob
  risco de queimar o módulo**. A 3 m chegam cerca de −15 dBm.
- **Regulatório:** a 30 dBm, a ANATEL exige salto de frequência em ≥ 35 canais,
  e o E220 só tem 19 canais legais. O teste de campo a 30 dBm fica **fora da
  conformidade**, e vale como medição de engenharia, não como configuração de
  produto (ver [`hardware.md`](hardware.md#regulatório-anatel-faixa-e-potência)).

## Canal 55 (905,125 MHz) (decidido em 2026-09-19)

- **Motivo:** o canal de fábrica do E220 (23 → 873,125 MHz) cai no downlink
  celular de 850 MHz e não é permitido pela ANATEL. O canal 55 fica no meio da
  faixa de 902–907,5 MHz, longe das bordas e fora da 915–928 MHz, onde operam as
  redes LoRaWAN AU915.
- Um `static_assert` em `RadioConfig.h` impede canal fora de CH 52–57 / 65–77.
- **Não resolve a potência:** em canal fixo, o E220 continua no limite genérico
  (ver [`hardware.md`](hardware.md#regulatório-anatel-faixa-e-potência)).

## Camada `hal` renomeada para `hw` (decidido em 2026-09-19)

- **Motivo:** o ESP-IDF tem um componente interno chamado `hal`. Um componente
  do projeto com o mesmo nome o **substitui**, e o build quebra dentro do IDF
  (`hal/etm_types.h`, `hal/efuse_hal.h` não encontrados).
- Renomeados diretório, prefixo de include (`hw/...`) e namespace (`hw::`), para
  não deixar o prefixo `hal/` ambíguo com os headers do IDF.
- **Regra:** nenhum componente do projeto pode ter o nome de um componente do
  ESP-IDF (conferir em `$IDF_PATH/components/`).

## Primeiro protótipo (decidido em 2026-09-19)

Escopo reduzido em relação ao produto. O protótipo valida o enlace e a lógica de
controle; as decisões de produto acima ficam para a versão seguinte.

- **MCU: ESP32-C3** (não o C6 do produto).
- **Alcance alvo: 5 km** (não 2 a 10 km).
- **Sem medição de corrente da bomba.** A confirmação é só o ACK de rádio;
  o SCT-013 fica fora do protótipo.
- **Toolchain: extensão ESP-IDF do VS Code**, em vez do PlatformIO.
- **Módulos disponíveis:** 2× E220-900T22D e 2× E220-900T30D. Datasheets em
  [`../datasheets/`](../datasheets/).
- ~~**Par do protótipo: T22D.**~~ *Substituído pelo par T30D (ver acima).*
  Valida o SKU padrão e é viável com bateria. O par
  T30D fica para comparação no teste de campo, o que mede na prática o ganho dos
  8 dB a mais e alimenta a decisão do SKU longo alcance.
- **Papéis por strap de GPIO, já no protótipo.** Um binário só para os dois nós:
  evita gravar o firmware errado numa placa e antecipa a validação da abordagem
  do produto. Aberto = reservatório (não aciona relé), GND = bomba.

### Conflitos entre o repositório e o protótipo

| Tema | Repositório hoje | Protótipo |
|------|------------------|-----------|
| Build | PlatformIO + Arduino | **ESP-IDF** (migrado em 2026-09-19) |
| MCU | ESP32-C3 | ESP32-C3 (coincide) |
| Alcance | "até ~5 km" | 5 km (coincide) |
| Módulo | T30D ou T22D em aberto | **Par T30D** (T22D na comparação de campo) |
| Papéis | Dois ambientes de build | **Binário único com strap de GPIO** |

### Escopo do protótipo de bancada (decidido em 2026-09-19)

Antes de qualquer hardware hidráulico ou elétrico, validar enlace, protocolo e
failsafe na mesa. Detalhes em [`bancada.md`](bancada.md).

- **Chave que trava no lugar da boia.** Chave, não botão momentâneo: `read()` é
  chamado uma vez por ciclo, e um botão quase nunca estaria pressionado no
  instante da leitura.
- **LED no lugar do contator.** Valida o caminho inteiro sem carga AC.
- **Bateria stubada.** O subsistema de energia não é o que o protótipo quer
  validar, e dispensa o divisor na protoboard.
- **Tempos curtos** (`BENCH_PROFILE`), mantendo a proporção failsafe ÷ heartbeat
  de 3:1. O `kAckTimeoutMs` não muda: depende do tempo de ar.
- Nada disso tocou em `core/`, `protocol/` nem no nó da bomba — só trocaram
  implementações de `ILevelSensor` e `IBatteryMonitor`.

### Migração para ESP-IDF: HAL nativa (decidido em 2026-09-19)

A HAL será **reescrita com os drivers nativos do ESP-IDF** (`driver/uart`,
`driver/gpio`, `esp_adc`), em vez de manter o Arduino como componente.

- **Motivo:** o projeto já isola o hardware atrás de interfaces (`ILoRaRadio`,
  `ILevelSensor`, `IPumpActuator`, `IBatteryMonitor`), então só os drivers
  concretos mudam. As camadas `protocol/` e `core/` são C++ puro e seguem iguais.
- O E220 é só uma UART com três GPIOs, então o driver é simples de portar.
- Arduino como componente do ESP-IDF deixaria a dependência de pé e pesaria o
  build sem ganho real.
- **Custo:** `E220Radio` (`Serial`, `millis`), `RelayPumpActuator`,
  `FloatSwitchLevelSensor`, `AdcBatteryMonitor`, `PowerManager` e os dois
  `main.cpp` (`setup`/`loop`) precisam ser reescritos.

## Decisões iniciais (`v0.0.1`)

- **Bomba desligada por padrão.** Qualquer incerteza (boot, sensor falho, enlace
  perdido) desliga a bomba. Motivo: evitar bombeamento sem controle.
- **Failsafe de 45 s.** Sem tráfego válido por `kLinkLostTimeoutMs`, a bomba
  desliga e só volta ao normal ao receber tráfego válido.
- **E220 em modo transparente, com driver escrito à mão.** Motivo: seguir o
  datasheet sem ficar preso à API de uma biblioteca.
- **Framing na camada `protocol/`.** O E220 é apenas um "cano de bytes".
- **Taxa aérea de 2,4 kbps.** Prioriza alcance (até ~5 km) sobre velocidade.
- **Boia como sensor padrão; ultrassônico opcional** via `USE_ULTRASONIC_SENSOR`.
- **Sem `delay()` longo.** Tudo dirigido por máquinas de estado e `poll()`.
- **`SX127xRadio` (SPI) como esqueleto.** Existe para provar que a abstração
  `ILoRaRadio` permite trocar de rádio.
- **Failover fonte↔bateria em hardware.** O firmware apenas observa a fonte ativa.
