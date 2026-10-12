# Hardware (PCB)

Projeto KiCad (8 ou superior) da placa dos dois nós. Uma única PCB, com
componentes marcados DNP conforme o papel (reservatório ou bomba).

> **Status:** estrutura criada. Nenhum esquemático ainda: o escopo depende das
> decisões em aberto de [`docs/decisoes.md`](docs/decisoes.md).

## Estrutura

| Pasta | Conteúdo |
|-------|----------|
| `./` | projeto KiCad (`.kicad_pro`, `.kicad_sch`, `.kicad_pcb`) |
| `libs/` | símbolos, footprints e modelos 3D do projeto (versionados) |
| `fab/` | saídas de fabricação: Gerber, furação, BOM por variante, CPL, PDF, STEP, renders |
| `docs/` | decisões de projeto e notas |

## Convenções

- Versões de hardware com tag `hw-vX.Y`, para não colidir com o SemVer do
  firmware (`vX.Y.Z`). Registradas no [`CHANGELOG.md`](../CHANGELOG.md).
- Decisões e motivos em [`docs/decisoes.md`](docs/decisoes.md).
- Pinagem do ESP32-C3 final só vai ao `PinConfig.h` depois de aprovada, em
  commit separado.
- **O E220 transmite logo no boot e nunca pode ficar sem antena.**
