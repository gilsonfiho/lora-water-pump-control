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
| E220 `VCC` | pino **5V** (VBUS) | 1000 µF + 100 nF junto ao módulo |
| E220 `GND` | GND | |
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
| Potência de TX (`cfg::kTxPower`, T30D) | `kLow` = 21 dBm | `kMax` = 30 dBm |

A proporção failsafe ÷ heartbeat continua 3:1 nos dois perfis — são três
heartbeats perdidos antes de desligar. Um `static_assert` trava isso.

### T30D: potência, distância e alimentação

O protótipo usa o **par E220-900T30D**. Três cuidados que o T22D não exigia:

- **Nós a ≥ 3 m um do outro, obrigatoriamente.** O T30D não desce abaixo de
  21 dBm (`kLow`), e o manual limita a entrada do receptor a **+10 dBm, sob
  risco de queimar o módulo**. Com antenas de ~2 dBi:

  | Distância | Chega no receptor (21 dBm) | |
  |---|---|---|
  | 10 cm | ~+13 dBm | **dano** |
  | 30 cm | ~+4 dBm | satura, CRC falha |
  | 3 m | ~−15 dBm | seguro |

  Pontas opostas da sala ou salas diferentes. Nunca as duas placas lado a lado
  na mesma protoboard ou mesa.
- **VCC do E220 no pino 5V (VBUS) do devkit.** O T30D precisa de ≥ 5 V para a
  potência nominal e puxa até 620 mA de pico (a 30 dBm; menos em `kLow`). A
  USB 2.0 fornece 500 mA, então é um improviso: 1000 µF + 100 nF junto ao VCC
  do módulo e fios curtos. **Se o ESP resetar ou a porta USB cair a cada
  transmissão**, troque por uma fonte de 5 V externa (≥ 1 A) com GND comum.
- **Os sinais continuam em 3,3 V.** UART, M0, M1 e AUX vão direto ao ESP32-C3.
  O manual avisa que TTL de 5 V pode queimar o módulo.

O `kAckTimeoutMs` (800 ms) **não** muda: depende do tempo de ar, não da
conveniência de quem está testando.

## Roteiro de validação

Grave o mesmo binário nas duas placas e mude só o strap. Antes de energizar,
confira que **cada E220 tem a sua antena** e que os nós estão **a ≥ 3 m**.

1. **Boot.** Cada nó deve logar o papel que assumiu. Se os dois disserem
   "RESERVATORIO", o strap da bomba não está no GND.
2. **Rádio.** Nenhum nó pode logar `FALHA ao iniciar o radio E220`. Se logar,
   confira alimentação do módulo (5 V no VCC), o par TX/RX cruzado e o `AUX`.
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
