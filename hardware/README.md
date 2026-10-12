# Hardware (PCB)

Duas placas, `emissor/` e `receptor/` (um projeto KiCad cada). Projeto KiCad **10** (10.0.x; formatos de arquivo da série 10, que as versões anteriores não abrem) da placa dos dois nós. Uma única PCB, com
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

## O que não vai ao Git

- `libs/vendor/` e `docs/datasheets/`: arquivos baixados de terceiros (STEP,
  footprints, datasheets). A origem de cada um fica no `SOURCES.md` da pasta.
  Os datasheets do E220, já no repositório, ficam em `../docs/datasheets/`.
- Versionados: o projeto KiCad e as bibliotecas **próprias** (`libs/symbols`,
  `libs/footprints`), que usam caminhos relativos (`${KIPRJMOD}`).

## Como abrir

- Emissor: `emissor/emissor.kicad_pro`. Receptor: `receptor/receptor.kicad_pro`.
- No editor de PCB, **View > 3D Viewer** (Alt+3) mostra a placa em 3D.
- Imagens e STEP prontos em `fab/emissor/` e `fab/receptor/`.

## Como regenerar (os arquivos KiCad saem dos scripts em `tools/`)

```powershell
python hardware/tools/gen_symbols.py      # simbolos proprios
python hardware/tools/gen_footprints.py   # footprints e STEP proprios
python hardware/tools/build_sch.py        # esquematicos das duas placas
$py = "C:\Program Files\KiCad\10.0\bin\python.exe"   # Python do proprio KiCad (modulo pcbnew)
& $py hardware/tools/build_pcb.py emissor
& $py hardware/tools/build_pcb.py receptor
```

Regenerar **sobrescreve** o que for editado no KiCad. Quando o layout passar a ser
editado a mao (roteamento), parar de rodar o `build_pcb.py`.
