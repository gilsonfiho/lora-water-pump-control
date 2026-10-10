# Hardware

Registrar aqui apenas valores **confirmados no hardware real**. Enquanto um item
estiver em aberto, o código mantém o `TODO(hw)` correspondente.

## Carga e ambiente

- Bomba d'água **monofásica de 1–3 CV**, acionada por **contator**.
- Distância alvo do produto: **2 a 10 km**, terreno com vegetação. **Protótipo: 5 km.**
- Rede elétrica (127/220 V AC) disponível no local da bomba.
- Conformidade **ANATEL**: respeitar o limite de **EIRP** em todos os SKUs
  (potência do módulo + ganho da antena − perdas).

## Componentes

| Item | Situação | Observação |
|------|----------|------------|
| MCU (protótipo) | **ESP32-C3** | Placa exata a confirmar (`platformio.ini` usa `esp32-c3-devkitm-1` como palpite). |
| MCU (produto) | ESP32-C6-WROOM-1 | Só depois do protótipo. |
| Módulo LoRa (protótipo) | **Par E220-900T30D** | Decidido em 2026-09-19. O par T22D fica para a comparação de campo. Datasheets em `docs/datasheets/`. |
| Módulo LoRa (produto, SKU padrão) | E220-900T22D (LLCC68, 22 dBm, UART) | Decidido. |
| Módulo LoRa (produto, longo alcance) | **E22-T30D** (30 dBm) | Família diferente do E220. Ver observação abaixo. |
| Confirmação da bomba (produto) | **SCT-013** (sensor de corrente) | **Fora do protótipo.** Detecta que a bomba realmente ligou, não só o comando ao contator. |
| Contator | A dimensionar | Nível lógico do acionamento (ativo alto/baixo) a confirmar. |
| Antena | Decisão estrutural | Ganho e altura de instalação são críticos acima de ~4 km. |
| Sensor de nível | Boia (padrão) | Quantidade de boias (1 ou 2) a confirmar. Ultrassônico é opcional. |
| Carregador de bateria | TP4056 ou similar | Failover fonte↔bateria feito em hardware. |
| Monitor de bateria | ADC ou INA219/226 | Razão do divisor do ADC a definir. |

Observação: o E22-T30D não é da mesma família do E220 (o E220 usa LLCC68).
Antes de fixar o SKU longo alcance, conferir no datasheet o chip, o comando de
configuração e as taxas aéreas do E22, pois o driver atual segue o E220.
A alternativa natural é o **E220-900T30D**, que compartilha o mesmo mapa de
registradores (ver comparação abaixo).

## E220-900T22D × E220-900T30D

Lido dos manuais em [`../datasheets/`](../datasheets/) (T22D v1.2, T30D v1.0).

**Idênticos nos dois módulos**, logo o mesmo driver serve para ambos:

- Pinos de sinal: `M0`, `M1`, `RXD`, `TXD`, `AUX`, `VCC`, `GND`.
- Modos por `M1 M0`: 0 = transparente (0 0), 1 = WOR TX (0 1), 2 = WOR RX (1 0),
  3 = sleep/configuração (1 1). Nos manuais, os textos das seções 6.3 e 6.4
  contradizem essa tabela; **a tabela da seção 6 é a correta**.
- Comandos: `C0` grava, `C1` lê, `C2` grava temporário (perde no reset),
  `FF FF FF` indica formato inválido. Configuração só em **9600 8N1**.
- Mapa de registradores `00H`–`07H`, taxas aéreas de 2,4 k a 62,5 kbps,
  canal = `850,125 + CH` MHz, buffer de 400 B, sub-pacote de 32/64/128/200 B.
- Padrão de fábrica: `C0 00 00 62 00 17` → canal 0x17 (873,125 MHz), 2,4 kbps,
  9600 8N1, endereço 0x0000, potência máxima.
- Sleep de ~5 µA e RX de ~17 mA.

**Diferenças que importam:**

| | E220-900T22D | E220-900T30D |
|---|---|---|
| Potência máxima | 22 dBm | 30 dBm |
| Tabela `REG1[1:0]` | 22 / 17 / 13 / 10 dBm | 30 / 27 / 24 / 21 dBm |
| Corrente de TX (pico) | **110 mA** | **620 mA** |
| Tensão para potência plena | ≥ 3,3 V (opera de 2,3 V) | **≥ 5,0 V** (opera de 3,0 a 5,5 V) |
| Nível lógico da UART | 3,3 V | **3,3 V** (manual: "5 V TTL may burn") |
| Entrada máxima no RX (*blocking power*) | — | **+10 dBm**, acima disso pode queimar |
| Distância de referência | 5 km | 10 km |
| Tamanho | 21 × 36 mm | 24 × 43 mm |

Consequências de projeto:

- Os dois **não são intercambiáveis na PCB**: o footprint difere. Só as funções
  dos pinos coincidem.
- O T30D puxa **620 mA no pico de TX e exige 5 V** para dar 30 dBm. Isso mexe na
  fonte, no dimensionamento da bateria e nos capacitores de desacoplamento. Num
  nó alimentado por bateria, é um custo alto.
- A distância de referência de 5 km do T22D vale em **área aberta e limpa**, com
  antena de 5 dBi a 2,5 m e 2,4 kbps. O alvo do protótipo é 5 km **com
  vegetação**, ou seja, está no limite do módulo. Reforça que antena e altura
  decidem o enlace.
- O EIRP da ANATEL limita potência do módulo **mais** ganho de antena. Com o
  T30D, uma antena de ganho alto pode estourar o limite.

### Módulo por nó (protótipo): par T30D

Decidido em 2026-09-19: **par T30D** nos dois nós (ver
[`decisions.md`](decisions.md)). O mesmo modelo nos dois lados, porque o enlace
é limitado pela direção mais fraca e um par misto não representa nenhum SKU. O
par T22D entra no teste de campo como comparação.

Na bancada:

- **VCC pelo pino 5V (VBUS) do devkit**, com eletrolítico de 1000 µF + 100 nF
  junto ao VCC do módulo e fios curtos. A USB 2.0 fornece 500 mA; o pico do
  T30D é 620 mA a 30 dBm (menos em `kLow`). Reset ou queda da porta durante TX
  indicam que é hora de uma fonte de 5 V externa com GND comum.
- **UART, M0, M1 e AUX ficam direto no ESP32-C3** (3,3 V), mesmo com o módulo
  alimentado a 5 V.
- **`kLow` = 21 dBm e nós a ≥ 3 m.** Com antenas de ~2 dBi, a 10 cm chegam
  ~+13 dBm ao receptor, acima do limite de dano de +10 dBm.

## Pendente no esquemático de referência

Fonte de alimentação, ESP32-C6, E220-T22D, contator, SCT-013 e circuitos de
surto/proteção (o contator e a carga AC exigem proteção contra transientes).

## Pinagem

Definida em `lib/config/PinConfig.h`. Preencher quando confirmada:

| Função | Pino | Confirmado |
|--------|------|------------|
| UART TX/RX do E220 | — | não |
| M0 / M1 / AUX do E220 | — | não |
| Boia(s) | — | não |
| Contator (acionamento) | — | não |
| SCT-013 (ADC) | — | não |
| Strap de papel (comando vs. atuador) | — | não |
| ADC da bateria | — | não |
| I2C (INA219/226) | — | não |

## Regulatório (ANATEL): faixa e potência

Fonte: Ato nº 14.448/2017 (radiação restrita), cópia de 2018. O Ato foi alterado
em 2020, 2022 e 2025 (Ato 14.158/2025); **confirmar no texto consolidado** e com
um OCD antes de fechar o produto.

- **Faixas permitidas perto de 900 MHz: 902–907,5 MHz e 915–928 MHz.** A faixa
  907,5–915 MHz não é permitida.
- **O canal padrão do E220 (23 → 873,125 MHz) é ilegal no Brasil.** Ele cai no
  downlink da banda celular de 850 MHz (869–894 MHz).
- Canais E220 legais (`freq = 850,125 + CH` MHz): **CH 52–57** (902,125–907,125)
  e **CH 65–77** (915,125–927,125), ou seja, 19 canais no total.
- Potência depende da técnica de modulação:

| Enquadramento | Condição | Limite |
|---|---|---|
| Genérico (item 4.1.4 / item 20) | canal fixo | 50 mV/m a 3 m (≈ −1,2 dBm EIRP) |
| Salto de frequência (10.2.5) | largura a 20 dB < 250 kHz: ≥ 35 canais, ≤ 0,4 s a cada 14 s | 1 W (≥ 35 canais) |
| Salto de frequência (10.2.5) | largura a 20 dB ≥ 250 kHz (máx. 500 kHz): ≥ 17 canais, ≤ 0,4 s a cada 7 s | 0,25 W (< 35 canais) |
| Modulação digital (10.3) | largura a 6 dB ≥ 500 kHz; ≤ 8 dBm/3 kHz | 1 W |
| Antena > 6 dBi (10.5) | — | reduzir 1 dB de potência por dB acima de 6 dBi |

- **Consequência: E220 em canal fixo a 125 kHz não se enquadra em alta
  potência.** Não é salto de frequência nem tem 500 kHz a 6 dB. Sobra o limite
  genérico (≈ −1 dBm EIRP), inviável para 5 km.
- Salto de frequência com ≥ 35 canais não cabe na grade de 1 MHz do E220 (só 19
  canais legais). A saída com o E220 seria salto em ≥ 17 canais com largura
  ≥ 250 kHz, a 24 dBm no máximo. **A confirmar:** o E220 não expõe a largura de
  banda LoRa, e não se sabe se alguma taxa aérea usa 250/500 kHz. Isso toca a
  escolha do rádio e do SKU T30D (30 dBm exige ≥ 35 canais).
- **Pendente:** verificar o certificado ANATEL do E220 (se existe e sob qual
  enquadramento).

## Restrições conhecidas

- O E220 é **UART**, não SPI.
- O **LLCC68**, em BW de 125 kHz, é limitado a **SF9** (não SF11). Sensibilidade
  de cerca de −129 dBm, contra −137 dBm do SX1262: ~8 dB a menos no *link budget*.
  A escolha do E220 foi feita ciente desse custo, que pesa nos limites de alcance.
- **Divergência de sensibilidade.** Os manuais da Ebyte anunciam −147 dBm (T22D) e
  −148 dBm (T30D) a 2,4 kbps. O LLCC68 não chega perto disso: o número realista
  é ~−129 dBm. O *link budget* usa −129 dBm; **não usar o valor do manual**.
- A atenuação por vegetação é a maior incerteza do enlace.
- Firmware unificado: **uma PCB** atende vários SKUs, e o papel do nó é escolhido
  por *strap* de GPIO.
- Ambos os nós devem usar os mesmos parâmetros de RF (canal, taxa aérea, endereço).
- Esquemáticos em [`../../assets/schematic.md`](../../assets/schematic.md).
