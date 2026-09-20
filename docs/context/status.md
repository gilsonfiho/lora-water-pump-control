# Status

Última atualização: 2026-09-20

## Versão atual

`v0.0.1` (pré-alpha). Esqueleto funcional: arquitetura, protocolo e drivers
principais implementados. Ainda não validado em hardware real.

## Feito

- Estrutura em camadas: `config/`, `platform/`, `hw/`, `protocol/`, `core/`, `power/`.
- Protocolo com quadro, CRC-16, ACK e retransmissão (`LinkLayer`).
- Máquinas de estado dos dois nós e failsafe de perda de enlace.
- **Migração para ESP-IDF (2026-09-19):** PlatformIO/Arduino removidos, camadas
  viraram componentes do IDF, drivers reescritos sobre `driver/uart`,
  `driver/gpio` e `esp_adc`. Binário único com papel por strap de GPIO.

- **Primeira compilação OK (2026-09-19)** com ESP-IDF **v6.1**, alvo esp32c3.
  App 0x2eb00 bytes (82% livre na partição de 1 MB), sem warnings do projeto.
  Dois ajustes foram necessários:
  - componente `hal` renomeado para **`hw`** (dir, include `hw/` e namespace
    `hw::`): ele sombreava o componente `hal` interno do ESP-IDF;
  - `REQUIRES driver` trocado por `esp_driver_uart esp_driver_gpio` (no IDF 6,
    o `driver` não reexporta mais `driver/uart.h` e `driver/gpio.h`).

- **Potência de TX no `BENCH_PROFILE` (2026-09-19):** `cfg::kTxPower` em
  `RadioConfig.h` vale `kLow` na bancada e `kMax` em campo.
- **Par T30D no protótipo (2026-09-19):** `makeProfile()` dos dois nós usa
  `kT30D`. Na bancada: `kLow` = 21 dBm (mínimo do T30D), nós a ≥ 3 m (o RX
  queima acima de +10 dBm), VCC no pino 5V do devkit com 1000 µF. O par T22D
  fica para a comparação de campo. Compila sem warnings.

- **Log de diagnóstico do enlace (2026-09-20):** a comunicação não logava nada,
  o que deixava a bancada cega. Adicionados: bytes crus (hex, `DEBUG`) no
  `E220Radio` e eventos tipados do `LinkLayer` (`onEvent` → TX/ACK+rtt/retry/
  timeout/crc/notforme) logados na `main/`. `protocol/`/`core/` seguem sem
  ESP-IDF. Compila no v6.1 (0x2f980 B, 81% livre). Para ver o hex, subir o nível
  do tag: `esp_log_level_set("e220", ESP_LOG_DEBUG)`.

## Em andamento

- Diagnóstico do "não comunica" na bancada: forte suspeita de saturação do RX
  (par T30D a curta distância, mínimo 21 dBm) e/ou brownout na TX. Confirmar
  pelos novos logs (`ACK` com rtt vs. `TIMEOUT`; `CRC inválido` = satura).
- Próximo passo continua sendo a bancada.

## Foco atual: protótipo de bancada

Branch `feature/prototipo-bancada`. ESP32-C3, ESP-IDF, par T30D. Sem água, sem
bomba e sem carga AC: chave no lugar da boia, LED no lugar do contator, bateria
stubada e tempos curtos. Montagem e roteiro em [`bancada.md`](bancada.md).

## Próximos passos

1. Montar a bancada e rodar o roteiro de [`bancada.md`](bancada.md), em especial
   o teste de failsafe.
2. Só depois: boias reais, subsistema de energia e contator.
3. Teste de campo com o par T30D, usando o par T22D como comparação. A 30 dBm
   com o E220 em canal fixo, o teste fica fora da conformidade ANATEL: vale
   como medição de engenharia.

## Para o produto (depois do protótipo)

- Esquemático elétrico de referência: fonte, ESP32-C6, E220-T22D, contator,
  SCT-013 e circuitos de surto/proteção.
- Plano de teste de campo (Fase 2): valida a atenuação por vegetação antes de
  fechar a antena, principalmente para enlaces acima de ~4 km.

## Já existe fora do repositório

- *Link budget* em planilha Excel (formatação em semáforo). Não está no repositório;
  se for versionar, colocar em `docs/`.

## Bloqueios

- **Regulatório (ANATEL):** o canal já foi para 55 (905,125 MHz), dentro da
  faixa permitida. Mas o E220 em canal fixo só se enquadra no limite genérico
  (≈ −1 dBm EIRP). Detalhes
  e opções em [`hardware.md`](hardware.md#regulatório-anatel-faixa-e-potência).
  Afeta canal, escolha do rádio e o SKU T30D.

- Pinagem ainda não confirmada no hardware.
- Alimentação do T30D pelo VBUS do devkit ainda não testada: pode exigir fonte
  de 5 V externa.
