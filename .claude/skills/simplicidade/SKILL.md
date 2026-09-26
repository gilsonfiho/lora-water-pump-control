---
name: simplicidade
description: Regra permanente de estilo de código deste projeto — sempre gerar a solução mais simples possível antes de escrever qualquer código novo ou editar código existente. Use sempre, em toda tarefa de desenvolvimento neste repositório, não só quando pedido explicitamente.
metadata:
  source: "adaptada de /Users/pacelli/git/pacelli/opendisplay-android/.agents/skills/simplicidade/SKILL.md para o firmware C++/ESP-IDF deste projeto"
---

# Simplicidade acima de tudo

Regra do projeto, sempre ativa — não é uma revisão opcional, é como o código deve nascer.
Vale para os dois nós (`main/reservoir_app.cpp`, `main/pump_app.cpp`) e para todas as
camadas em `components/` (`config`, `hw`, `protocol`, `core`, `power`, `platform`).

## Antes de escrever qualquer código

Pare e pergunte: **existe uma solução mais simples que resolve a mesma necessidade?** Só siga
para a alternativa mais complexa se a resposta for claramente não. Em firmware embarcado isso
pesa mais ainda: cada abstração extra é RAM, flash e latência que o ESP32-C3 não sobra.

## Prioridades, nessa ordem

1. **Legibilidade** — código que se lê como prosa técnica direta.
2. **Manutenção** — fácil de mudar depois, sem precisar entender camadas extras primeiro.
3. **Clareza** — nomes que dizem o que a coisa é/faz, sem precisar de comentário pra decodificar.

## Evitar por padrão

- Abstrações genéricas pra um único caso de uso (interface `hw/IXxx` pra um único
  implementador real, template genérico onde um tipo concreto resolve).
- Padrões de projeto (Factory, Strategy, State como hierarquia de classes, Observer
  plugável) quando um `enum class` + `switch`, ou uma função simples, já resolve.
- Alocação dinâmica (`new`/`malloc`, containers que alocam) fora de inicialização única no
  boot — o projeto já roda sem exceções/RTTI (`sdkconfig.defaults`) por esse motivo.
- Otimizações prematuras — só otimizar quando há um problema de performance/timing medido e
  real (ex.: estourar `kAckTimeoutMs` ou o watchdog de tarefa).
- Complexidade "por precaução" — não construir para SKUs, sensores ou variantes de rádio
  hipotéticos que ainda não existem no roadmap (`docs/context/decisions.md`).

## Como aplicar

- Nomes claros e diretos (variável, função, classe) — se o nome já explica, não comenta.
- Métodos curtos, uma responsabilidade por função.
- Seguir o padrão que já existe no projeto (ver `CLAUDE.md`, `docs/ARCHITECTURE.md` e o
  código vizinho) em vez de introduzir um estilo novo pra a mesma coisa.
- Camadas superiores dependem só de interface (`hw/IPumpActuator`, `hw/ILevelSensor`,
  `hw/ILoRaRadio`, `hw/IBatteryMonitor`) — isso não é exceção à regra, é o único
  desacoplamento que o projeto já decidiu que vale (permite bancada vs. campo, ver
  `RelayPumpActuator` vs. um LED, `FloatSwitchLevelSensor` vs. `BenchSwitchLevelSensor`).
- Só usar solução mais complexa quando a mais simples realmente não resolve — e, nesse caso,
  deixar claro (num comentário curto) *por que* a simples não bastou.

## Exemplo de aplicação neste projeto

`core/PumpController` já segue isso: a máquina de estados do nó da bomba (`kBoot`, `kIdle`,
`kPumping`, `kLinkLost`) é um `enum class` + um `switch` em `enter()` — não uma hierarquia de
classes `State` com `Strategy`/`State pattern`. Não há necessidade disso aqui: são quatro
estados fixos, conhecidos em tempo de compilação, e o `switch` já deixa a transição óbvia de
ler.

Do mesmo jeito, `core::Signal<Event, kMaxObservers>` (`core/Observer.h`) implementa o
suficiente do padrão Observer pro projeto — array fixo de `kMaxObservers` handlers, sem
alocação dinâmica — em vez de um barramento de eventos genérico e plugável que este firmware
de dois nós nunca vai precisar.
