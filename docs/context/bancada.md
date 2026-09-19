# Protótipo de bancada

Montagem mínima para validar enlace, protocolo e failsafe **sem água, sem
bomba e sem carga AC**. Vale para a branch `feature/prototipo-bancada`.

## O que o protótipo troca

| Campo | Bancada | Onde |
|-------|---------|------|
| Boia (2 unidades) | **Chave que trava** (1 unidade) | `BenchSwitchLevelSensor` |
| Contator + bomba | **LED + resistor 330 Ω** | `RelayPumpActuator`, mesmo driver |
| ADC de bateria + divisor | **Stub fixo** (externa, 100%) | `StubBatteryMonitor` |
| Tempos de campo | **Tempos curtos** | `BENCH_PROFILE` em `NodeConfig.h` |

`ReservoirController`, `PumpController`, `LinkLayer` e o protocolo **não mudam**.
Só trocam implementações de `ILevelSensor` e `IBatteryMonitor`, que é para isso
que as interfaces existem.

## Pinagem (ESP32-C3, provisória)

Os dois nós gravam **o mesmo binário**; o papel vem do strap.

| Função | GPIO | Ligação |
|--------|------|---------|
| Strap de papel | 3 | aberto = reservatório · ao GND = bomba |
| E220 `RXD` | 5 (TX do ESP) | |
| E220 `TXD` | 4 (RX do ESP) | |
| E220 `M0` | 6 | |
| E220 `M1` | 7 | |
| E220 `AUX` | 10 | |
| Chave de nível (só reservatório) | 1 | outro lado no GND |
| LED (só bomba) | 0 | LED + 330 Ω para o GND |

### Pinos que não dá para usar no C3

Isto restringiu as escolhas acima e vale relembrar antes de remapear:

- **11–17:** flash SPI interna.
- **18, 19:** USB D−/D+. Usá-los como GPIO **derruba o USB Serial/JTAG**, que é
  como o devkit grava e abre o monitor.
- **20, 21:** UART0 do console.
- **2, 8, 9:** straps de boot. Um sinal externo segurando-os no nível errado no
  reset impede o ESP de subir. O 9 é o botão BOOT.

Sobram 0, 1, 3, 4, 5, 6, 7 e 10 — exatamente os oito que o protótipo usa.

> Duas pendências herdadas, sem efeito na bancada mas registradas em
> `PinConfig.h` com `TODO(hw)`: `kFloatHigh` está no GPIO 2 (strap) e
> `kExternalPresent` no GPIO 18 (USB D−). Ambos precisam mudar antes de voltar
> às boias e ao monitor de bateria real.

## Tempos de bancada

`BENCH_PROFILE` (ligado por padrão em `NodeConfig.h`) encurta o que o operador
espera, e nada mais. Para os tempos de campo, troque
`#define BENCH_PROFILE 1` por `0` em `NodeConfig.h`. `idf.py build
-DBENCH_PROFILE=0` **não** funciona: cria uma variável do CMake, não um
`#define` do compilador.

| | Bancada | Campo |
|---|---|---|
| Ciclo do reservatório | 2 s | 10 s |
| Heartbeat | 5 s | 15 s |
| Failsafe (perda de enlace) | 15 s | 45 s |
| Telemetria de bateria | 15 s | 60 s |
| Potência de TX (`cfg::kTxPower`) | `kLow` = 10 dBm | `kMax` = 22 dBm |

A proporção failsafe ÷ heartbeat continua 3:1 nos dois perfis — são três
heartbeats perdidos antes de desligar. Um `static_assert` trava isso.

A potência cai na bancada porque os módulos ficam a centímetros um do outro.
A 22 dBm e ~30 cm de distância, chegam de +1 a +5 dBm no receptor, perto do
máximo de entrada do LLCC68 (~+10 dBm). O front-end satura e aparecem erros de
CRC que não vêm do protocolo. Mesmo a 10 dBm, mantenha ≥ 1 m entre as antenas.

O `kAckTimeoutMs` (800 ms) **não** muda: depende do tempo de ar, não da
conveniência de quem está testando.

## Roteiro de validação

Grave o mesmo binário nas duas placas e mude só o strap.

1. **Boot.** Cada nó deve logar o papel que assumiu. Se os dois disserem
   "RESERVATORIO", o strap da bomba não está no GND.
2. **Rádio.** Nenhum nó pode logar `FALHA ao iniciar o radio E220`. Se logar,
   confira alimentação do módulo, o par TX/RX cruzado e o `AUX`.
3. **Liga.** Feche a chave. Em até ~2 s o LED da bomba acende e o receptor loga
   `estado -> BOMBEANDO`.
4. **Desliga.** Abra a chave. O LED apaga e o log vai para `OCIOSO`.
5. **Failsafe.** Com a chave fechada e o LED aceso, **tire a alimentação do
   reservatório**. Em ~15 s o LED deve apagar sozinho e o log mostrar
   `ENLACE PERDIDO (failsafe)`. Este é o teste que mais importa.
6. **Recuperação.** Religue o reservatório: o enlace volta e o LED reacende.

## O que a bancada ainda não prova

- Alcance e margem de enlace. O teste de campo é outro.
- Comportamento com a carga AC real: surto do contator, transientes, ruído.
- Consumo e autonomia (não há bateria nem medição).
- Comportamento da boia de verdade (chave travada, fiação invertida, `kMid`).
