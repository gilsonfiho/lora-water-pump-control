# Decisões de hardware

Formato: data, decisão, motivo. Mais recentes no topo.

## 2026-10-11: estrutura do projeto

- Branch `feature/pcb`, a partir da `develop` (já atualizada com a
  `origin/develop`), e não da `main`: a `develop` tem o trabalho mais recente.
- Projeto em `hardware/`, com `libs/`, `fab/` e `docs/`. Bibliotecas ficam
  versionadas; backups, autosave e `*.kicad_prl` ficam no `.gitignore`.
- Tags de hardware `hw-vX.Y`.

## 2026-10-11: esquemático, etapa 1 (folhas MCU e Rádio)

Gerado por `hardware/tools/build_sch.py` (símbolos oficiais do KiCad + os
próprios em `libs/symbols/lora-pump.kicad_sym`, gerados por `gen_symbols.py`).
Conectividade por nome de net, sem fios entre componentes. Para editar o
esquemático, altere o gerador (ou edite no KiCad e abandone o gerador: decisão
a tomar antes do layout).

- **ESP32-C3-MINI-1-H4X** (-40 a 105 °C, 4 MB), não o N4X (-40 a 85 °C): caixa
  externa ao sol. O N4 e o H4 estão em NRND; N4X e H4X são os recomendados
  (Datasheet v2.2, Tabela 1-1, p. 3).
- **Símbolo próprio do MINI-1**: o KiCad 10 não tem símbolo nem footprint do
  ESP32-C3-MINI-1. Pinos da Tabela 3-1 (p. 11). Os 18 GND (inclusive o EPAD) são
  pinos ocultos ligados à net GND por nome.
- **EN com RC de 10 k + 1 uF** (p. 34), botão RESET em EN→GND. Botão BOOT em
  GPIO9→GND, que também é `I2C_SCL`; GPIO8 (`I2C_SDA`) e GPIO9 têm pull-up de
  4k7 para garantir nível 1 no boot (Tabela 4-3, p. 13).
- **GPIO2 com pull-up de 10 k** (nota da Tabela 4-3: recomendado, por causa de
  glitches). Serve também de polarização da boia alta.
- **Strap de papel (GPIO3):** pull-up 10 k + solder jumper `JP1` para GND
  (fechado = bomba), serigrafia "PAPEL". Mantém "aberto = reservatório" do firmware.
- **E220 T30D e T22D sobrepostos** (U2/U3, campo `Variante`), mesmas nets. Os pinos
  TXD e AUX do símbolo são `tri_state` para o ERC aceitar as duas saídas na
  mesma net; só um módulo é montado.
- **A antena é o SMA-K do próprio módulo E220** (Manual, Tabela 2-2, p. 5). Não há
  conector SMA na PCB: o que a placa define é a posição do módulo na caixa.
- **Pull-ups de 10 k em TXD e AUX do E220: DNP.** Só para MCU de 5 V (Tabela 4-1,
  p. 7); aqui o MCU é de 3,3 V.
- **Capacitor de VCC_RF: 470 uF, `TODO(hw)`.** O README fala em 100 uF + 100 nF e
  a bancada usa 1000 uF; o valor definitivo sai da medição com o buck escolhido.
- PWR_FLAG de `+3V3`, `GND` e `VCC_RF` são provisórios (marcados TEMP no
  esquemático) e saem com a folha de alimentação.
- **MPNs** são sugestões; estoque e preço no catálogo da JLCPCB **não foram
  verificados** (sem acesso nesta sessão).

ERC (kicad-cli 10.0.7): 0 erros, 8 avisos: 3 de footprint não encontrado (os
footprints próprios são a etapa seguinte) e 5 de rótulo global com um só pino
(`BAT_ADC`, `FLOAT_LOW_PUMP_DRV`, `USB_DM`, `USB_DP`, `EXT_PRESENT`: serão
ligados nas folhas de alimentação, entradas/saídas e conectores).

## 2026-10-11: premissas adotadas ao "construir o projeto" (a revisar)

O autor pediu "construa o projeto" sem responder às perguntas de arquitetura.
Adotadas as recomendações abaixo, **todas revisáveis**; a da bomba é de
segurança e precisa de confirmação explícita antes do hw-v0.1.

| Tema | Premissa |
|------|----------|
| Fabricante | JLCPCB, SMT econômico; E220, SMA e bornes à mão |
| Rádio | footprints T22D e T30D sobrepostos, variantes DNP |
| Fonte | entrada 5–12 V, buck para 5 V (rádio), LDO 3V3 (MCU) |
| Bateria | divisor no ADC; header I2C livre; INA219/226 opcional (DNP) |
| **Bomba** | **só contato seco em bornes**: a PCB comanda um contator externo, sem rede na placa |
| Boias | 2, chave seca para GND |
| Camadas | 2 |

## 2026-10-11: mapa de pinos proposto (ESP32-C3-MINI-1)

Fontes: ESP32-C3-MINI-1 Datasheet v2.2, Tabela 3-1 (p. 11), Tabelas 4-1 e 4-3
(p. 12-13). O MINI-1 não expõe GPIO11–17 (flash interna).

Strapping (Tabela 4-3, p. 13): modo SPI boot exige GPIO9 = 1; download exige
GPIO8 = 1 e GPIO9 = 0; GPIO2 deve ter pull-up ("recomendado ... devido a
glitches").

| GPIO | Função | Papel | Observação |
|------|--------|-------|------------|
| 0 | `BAT_ADC` | ambos | ADC1_CH0; divisor da bateria |
| 1 | `FLOAT_LOW` **ou** `PUMP_DRV` | reservatório / bomba | net única, variante DNP; na bomba o driver tem pull-down (estado seguro) |
| 2 | `FLOAT_HIGH` | reservatório | strap: pull-up externo de 10 k (exigido pelo datasheet) |
| 3 | `ROLE_STRAP` | ambos | jumper: aberto = reservatório, GND = bomba |
| 4 | UART RX (← E220 TXD) | ambos | |
| 5 | UART TX (→ E220 RXD) | ambos | |
| 6 | `E220_M0` | ambos | |
| 7 | `E220_M1` | ambos | |
| 8 | `I2C_SDA` | ambos | strap: pull-up 4k7 mantém 1 (ok) |
| 9 | `I2C_SCL` + botão BOOT | ambos | strap: pull-up 4k7 mantém 1; o botão compartilha a linha |
| 10 | `E220_AUX` | ambos | |
| 18, 19 | USB D−/D+ | ambos | reservados |
| 20 | `EXT_PRESENT` | ambos | **exige mover o console para o USB Serial/JTAG** (mudança no `sdkconfig.defaults`, firmware) |
| 21 | `LED_STATUS` | ambos | TXD0: o ROM imprime no boot e o LED pisca; inofensivo |

Escolhas e consequências:

- **GPIO1 compartilhado** entre a boia baixa e o driver da bomba: os papéis são
  exclusivos e as variantes DNP separam os circuitos. Custo: uma mesma placa não
  faz os dois papéis na bancada.
- **Relé nunca em GPIO20/21**: a UART0 do console toca neles durante o boot, e
  isso poderia acionar a saída. Por isso o driver da bomba fica em GPIO1.
- `PinConfig.h` só muda depois de aprovado, em commit separado: hoje o relé está
  no GPIO0 (colide com `BAT_ADC`) e o `EXT_PRESENT` no GPIO18.

## Em aberto (bloqueiam o esquemático)

- Fabricante (JLCPCB, PCBWay ou outro) e tipo de montagem (SMT ou manual).
- Caixa, tamanho alvo da placa e posição do SMA.
- Rádio: T22D, T30D ou os dois footprints sobrepostos.
- Fonte externa: 5 V, 12 V ou ambas.
- Medição de bateria: divisor + ADC ou INA219/226.
- Saída da bomba: contato seco em bornes ou relé de potência na placa.
- Boias: 1 ou 2.
- Mapa de pinos final (faltam 14 sinais para 8 GPIOs livres; ver conversa de
  2026-10-11) e se o console migra para o USB Serial/JTAG.
- Ferramentas: KiCad 10 e leitor de PDF ainda não instalados (aguardando OK).

## 2026-10-11: KiCad 10

- Projeto em **KiCad 10** (instalação via `winget install KiCad.KiCad`, hoje
  10.0.6). Motivo: decisão do autor. Consequência: os arquivos gerados usam o
  formato da série 10 e não abrem em 8 ou 9; o `kicad-cli` usado em ERC, DRC e
  fabricação é o da mesma versão.
