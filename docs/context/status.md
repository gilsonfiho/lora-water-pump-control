# Status

Última atualização: 2026-09-19

## Versão atual

`v0.0.1` (pré-alpha). Esqueleto funcional: arquitetura, protocolo e drivers
principais implementados. Ainda não validado em hardware real.

## Feito

- Estrutura em camadas: `config/`, `platform/`, `hal/`, `protocol/`, `core/`, `power/`.
- Protocolo com quadro, CRC-16, ACK e retransmissão (`LinkLayer`).
- Máquinas de estado dos dois nós e failsafe de perda de enlace.
- **Migração para ESP-IDF (2026-09-19):** PlatformIO/Arduino removidos, camadas
  viraram componentes do IDF, drivers reescritos sobre `driver/uart`,
  `driver/gpio` e `esp_adc`. Binário único com papel por strap de GPIO.

## Em andamento

- Migração ESP-IDF escrita, mas **nunca compilada** — falta instalar o framework.

## Foco atual: protótipo de bancada

Branch `feature/prototipo-bancada`. ESP32-C3, ESP-IDF, par T22D. Sem água, sem
bomba e sem carga AC: chave no lugar da boia, LED no lugar do contator, bateria
stubada e tempos curtos. Montagem e roteiro em [`bancada.md`](bancada.md).

## Próximos passos

1. **Instalar o framework ESP-IDF**: a extensão 2.2.0 do VS Code está instalada,
   mas o framework não. Rodar *ESP-IDF: Configure ESP-IDF Extension*.
2. **Compilar pela primeira vez** (`idf.py set-target esp32c3 && idf.py build`) e
   registrar aqui o resultado. O código migrado **ainda não foi compilado**.
3. Montar a bancada e rodar o roteiro de [`bancada.md`](bancada.md), em especial
   o teste de failsafe.
4. Só depois: boias reais, subsistema de energia e contator.
5. Teste de campo, comparando o par T22D com o par T30D.

## Para o produto (depois do protótipo)

- Esquemático elétrico de referência: fonte, ESP32-C6, E220-T22D, contator,
  SCT-013 e circuitos de surto/proteção.
- Plano de teste de campo (Fase 2): valida a atenuação por vegetação antes de
  fechar a antena, principalmente para enlaces acima de ~4 km.

## Já existe fora do repositório

- *Link budget* em planilha Excel (formatação em semáforo). Não está no repositório;
  se for versionar, colocar em `docs/`.

## Bloqueios

- Pinagem e modelo exato dos módulos ainda não confirmados.
- Par de módulos do protótipo em aberto (quantidade disponível de cada modelo).
