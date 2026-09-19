# CLAUDE.md

Firmware PlatformIO (ESP32-C3 + LoRa E220) para controle remoto de bomba d'água,
com dois nós: `reservoir_node` (transmissor) e `pump_node` (receptor/atuador).

## Contexto

Antes de trabalhar, leia:

- @docs/context/status.md
- @docs/context/decisions.md
- @docs/context/bancada.md
- @docs/ARCHITECTURE.md

Consulte conforme o assunto: `docs/context/hardware.md` e `docs/context/protocol.md`.
Ao terminar uma tarefa relevante, atualize `docs/context/status.md`.

## Modo de trabalho

- Comunicação em nível técnico de par: sem simplificar eletrônica embarcada ou RF.
- Antes de decisões de arquitetura, fazer perguntas estruturadas para fechar o escopo.
- Este é um produto com vários SKUs: considerar escalabilidade e conformidade ANATEL.

## Comandos

ESP-IDF v5.x, alvo ESP32-C3. Um único binário serve aos dois nós; o papel vem do
strap em `cfg::RolePins::kRoleStrap` (aberto = reservatório, GND = bomba).

```bash
idf.py set-target esp32c3
idf.py build
idf.py -p COM3 flash monitor
```

Datasheets dos módulos: `docs/datasheets/`. Consulte-os antes de mexer no driver do E220.

## Estrutura

`components/<camada>/include/<camada>/*.h` + fontes na raiz do componente. As
dependências entre camadas ficam em `REQUIRES`, no `CMakeLists.txt` de cada uma.
`protocol/` e `core/` não podem depender de driver concreto nem do ESP-IDF: para
tempo, usam `platform::millis()` / `platform::delayMs()`.

## Regras

- Segurança primeiro: a bomba fica desligada em qualquer incerteza.
- Nada de `delay()` longo; usar máquinas de estado e `poll()`.
- Camadas superiores dependem só das interfaces da `hal/`, nunca dos drivers.
- Pinos, endereços e timeouts ficam em `components/config/`, não espalhados no código.
- O laço principal sempre cede o processador (`platform::delayMs`), senão o
  watchdog de tarefa dispara.
- Decisões que dependem do hardware final levam `TODO(hw)`.
- Estamos em **modo bancada**: chave no lugar da boia, LED no lugar do contator,
  bateria stubada, tempos curtos. Substituições vivem na `hal/`, nunca na `core/`.
- No ESP32-C3, evite os GPIOs 2, 8, 9 (straps), 11–17 (flash), 18–19 (USB) e
  20–21 (console). Livres: 0, 1, 3, 4, 5, 6, 7, 10.
