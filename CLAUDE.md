# CLAUDE.md

Firmware ESP-IDF (ESP32-C3 + LoRa E220) para controle remoto de bomba d'água,
com dois nós: reservatório (transmissor, `main/reservoir_app.cpp`) e bomba
(receptor/atuador, `main/pump_app.cpp`).

## Contexto

Antes de trabalhar, leia:

- @docs/context/status.md
- @docs/context/decisions.md
- @docs/context/bancada.md
- @docs/ARCHITECTURE.md

Consulte conforme o assunto: `docs/context/hardware.md` (inclui a seção
regulatória da ANATEL) e `docs/context/protocol.md`.
Ao terminar uma tarefa relevante, atualize `docs/context/status.md`. Mudanças
visíveis ao usuário também entram em `CHANGELOG.md`, na seção `[Não lançado]`.

## Modo de trabalho

- Comunicação em nível técnico de par: sem simplificar eletrônica embarcada ou RF.
- Antes de decisões de arquitetura, fazer perguntas estruturadas para fechar o escopo.
- Este é um produto com vários SKUs: considerar escalabilidade e conformidade ANATEL.
- **Commits/PRs: nunca trailer de co-autor.** Nenhum commit ou PR deste repositório leva
  `Co-Authored-By` (nem Claude, nem qualquer ferramenta de IA) — remover essa linha antes de
  todo `git commit`/`git push`, independente de o sistema sugerir esse padrão de atribuição por
  padrão.

## Skills locais deste projeto

Adaptadas para C++17/ESP-IDF (linguagem/stack deste firmware); têm prioridade sobre skills
globais de mesmo nome ao trabalhar neste repositório.

- `.claude/skills/simplicidade/SKILL.md` — regra permanente: sempre a solução mais simples que
  resolve a necessidade real, sem abstração/padrão/otimização antecipada. Adaptada de
  `opendisplay-android/.agents/skills/simplicidade/SKILL.md`.
- `.claude/skills/engenheiro-planejador-pre-plano/SKILL.md` — persona de planejador/analista:
  investiga intenção, mapeia impacto/risco, cataloga TODOs e restrições antes de qualquer
  código. Não escreve código.
- `.claude/skills/engenheiro-software-senior/SKILL.md` — persona de engenheiro sênior
  C++17/ESP-IDF/firmware embarcado (RAII, sem exceções/RTTI, sem alocação dinâmica em runtime,
  máquinas de estado não bloqueantes), SOLID/Clean Code balanceado com simplicidade.

Ambas as personas partiram de agentes genéricos em Python/FastAPI; aqui trocam o domínio para
C++/ESP-IDF e removem referências a `.specify/memory/constitution.md`, que não existe neste
repositório.

## Comandos

ESP-IDF **v6.1** (instalado pelo EIM em `C:\esp\v6.1`), alvo ESP32-C3. O
`idf.py` não fica no PATH: ative o ambiente uma vez em cada terminal
PowerShell.

```powershell
. C:\Espressif\tools\Microsoft.v6.1.PowerShell_profile.ps1
idf.py build
idf.py -p COM3 flash monitor     # sair do monitor: Ctrl+]
```

- `idf.py reconfigure`: depois de criar, renomear ou apagar componente, ou de
  mexer em `REQUIRES`.
- `idf.py set-target esp32c3`: só se o `sdkconfig` for apagado (ele o recria).

Um único binário serve aos dois nós; o papel vem do strap em
`cfg::RolePins::kRoleStrap` (aberto = reservatório, GND = bomba).

Datasheets dos módulos: `docs/datasheets/`. Consulte-os antes de mexer no driver do E220.

## Estrutura

`components/<camada>/include/<camada>/*.h` + fontes na raiz do componente. As
dependências entre camadas ficam em `REQUIRES`, no `CMakeLists.txt` de cada uma.
`protocol/` e `core/` não podem depender de driver concreto nem do ESP-IDF: para
tempo, usam `platform::millis()` / `platform::delayMs()`.

- **Nenhum componente pode ter o nome de um componente do ESP-IDF** (conferir em
  `C:\esp\v6.1\esp-idf\components\`). O do projeto substitui o do framework e
  quebra o build. Foi por isso que a camada `hal` virou `hw`.
- No IDF 6, o guarda-chuva `driver` não exporta mais UART e GPIO: use
  `REQUIRES esp_driver_uart esp_driver_gpio`.

## Regras

- Segurança primeiro: a bomba fica desligada em qualquer incerteza.
- Nada de `delay()` longo; usar máquinas de estado e `poll()`.
- Camadas superiores dependem só das interfaces da `hw/`, nunca dos drivers.
- Pinos, endereços, timeouts e parâmetros de RF ficam em `components/config/`,
  não espalhados no código.
- O laço principal sempre cede o processador (`platform::delayMs`), senão o
  watchdog de tarefa dispara.
- Decisões que dependem do hardware final levam `TODO(hw)`.
- Estamos em **modo bancada**: chave no lugar da boia, LED no lugar do contator,
  bateria stubada, tempos curtos e TX a 10 dBm (`cfg::kTxPower`). Tudo isso sai
  do `BENCH_PROFILE`. Substituições vivem na `hw/`, nunca na `core/`.
- No ESP32-C3, evite os GPIOs 2, 8, 9 (straps), 11–17 (flash), 18–19 (USB) e
  20–21 (console). Livres: 0, 1, 3, 4, 5, 6, 7, 10.

### RF e ANATEL

- Canal do E220 só nas faixas permitidas: **CH 52–57** (902–907,5 MHz) ou
  **CH 65–77** (915–928 MHz). Hoje é o CH 55 (905,125 MHz). Um `static_assert`
  em `RadioConfig.h` barra os demais canais. O padrão de fábrica (CH 23,
  873,125 MHz) é ilegal no Brasil.
- Em canal fixo de 125 kHz, o E220 só se enquadra no limite genérico
  (≈ −1 dBm EIRP). Não prometa alcance nem potência sem resolver isso: ver
  `docs/context/hardware.md`.
- Os dois nós gravam a configuração do E220 a cada boot. Ao mudar canal, taxa
  aérea ou endereço, **regrave os dois nós juntos**: em configurações
  diferentes, eles não se comunicam, e o sintoma é o mesmo de um enlace caído.
