# Arquivos de terceiros (não versionados)

Modelos 3D (STEP), footprints e símbolos baixados de fabricantes ficam nesta
pasta e **não vão ao Git** (ver `.gitignore`). Cada arquivo baixado é registrado
abaixo, para ser baixado de novo em um clone novo.

| Arquivo | Origem (URL) | Data | Usado em |
|---------|--------------|------|----------|
| ebyte_3d/E220-XXXT30D_3D.step (12,1 MB; zip de 1,9 MB) | https://www.cdebyte.com/pdf-down.aspx?id=3606 (aba "File Downloads" > "3D File" de https://www.cdebyte.com/products/E220-900T30D) | 2026-10-12 | E220-900T30D (modelo 3D oficial da EBYTE) |
| espressif/ESP32-C3-MINI-1.STEP (7,5 MB) | https://raw.githubusercontent.com/espressif/kicad-libraries/main/3dmodels/espressif.3dshapes/ESP32-C3-MINI-1.STEP (repo oficial da Espressif; licenca nao declarada no GitHub) | 2026-10-12 | ESP32-C3-MINI-1 (modelo 3D) |
| espressif/ESP32-C3-MINI-1.kicad_mod (13 KB) | https://raw.githubusercontent.com/espressif/kicad-libraries/main/footprints/Espressif.pretty/ESP32-C3-MINI-1.kicad_mod | 2026-10-12 | so referencia: confere os 61 pads do footprint proprio (origem 2,7 mm acima do centro do modulo) |
| easyeda/usbc_TYPE-C-31-M-12.obj (0,7 MB) | https://modules.easyeda.com/3dmodel/617b05f9bba7410b96c001093d8189e4 (LCSC C165948, via API easyeda.com/api/products/C165948) | 2026-10-12 | USB-C HRO TYPE-C-31-M-12 (convertido para .wrl por tools/obj2wrl.py) |
| easyeda/jack_PJ-102AH.obj (0,1 MB) | https://modules.easyeda.com/3dmodel/ada1fff93ffc4fd1a2325a88c7c7a76a (LCSC C9900001188, "Extended Part" da JLCPCB) | 2026-10-12 | Jack CUI PJ-102AH (convertido para .wrl) |

Para refazer em um clone novo: baixar os arquivos acima para estas pastas, rodar
`python hardware/tools/obj2wrl.py <obj> <wrl>` nos dois do EasyEDA e extrair o STEP do E220 do zip.
