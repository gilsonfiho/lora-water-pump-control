# Decisões de hardware

Formato: data, decisão, motivo. Mais recentes no topo.

## 2026-10-11: estrutura do projeto

- Branch `feature/pcb`, a partir da `develop` (já atualizada com a
  `origin/develop`), e não da `main`: a `develop` tem o trabalho mais recente.
- Projeto em `hardware/`, com `libs/`, `fab/` e `docs/`. Bibliotecas ficam
  versionadas; backups, autosave e `*.kicad_prl` ficam no `.gitignore`.
- Tags de hardware `hw-vX.Y`.

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
