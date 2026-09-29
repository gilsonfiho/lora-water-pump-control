# Status

Última atualização: 2026-09-19

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
  `RadioConfig.h` vale `kLow` (10 dBm) na bancada e `kMax` em campo. Evita
  saturar o LLCC68 com os módulos próximos. Compila sem warnings.

## Em andamento

- Nada em aberto no build; próximo passo é a bancada.

## Foco atual: protótipo de bancada

Branch `feature/prototipo-bancada`. ESP32-C3, ESP-IDF, par T22D. Sem água, sem
bomba e sem carga AC: chave no lugar da boia, LED no lugar do contator, bateria
stubada e tempos curtos. Montagem e roteiro em [`bancada.md`](bancada.md).

## Próximos passos

1. Montar a bancada e rodar o roteiro de [`bancada.md`](bancada.md), em especial
   o teste de failsafe.
2. Só depois: boias reais, subsistema de energia e contator.
3. Teste de campo, comparando o par T22D com o par T30D.

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

- Pinagem e modelo exato dos módulos ainda não confirmados.
- Par de módulos do protótipo em aberto (quantidade disponível de cada modelo).
