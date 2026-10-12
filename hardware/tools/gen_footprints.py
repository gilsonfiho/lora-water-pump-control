"""Gera footprints e modelos 3D proprios em hardware/libs/.

* ESP32-C3-MINI-1: geometria copiada do footprint oficial do ESP32-C6-MINI-1
  (KiCad), que tem o mesmo involucro de 13,2 x 16,6 mm e 53 pinos. Verificado:
  EPAD de 1,45 mm e corpo de ~13,3 x 16,7 mm (Datasheet ESP32-C3-MINI-1 v2.2,
  Figura 10-1). TODO(hw): conferir visualmente contra a Figura 10-1.
* E220-900T30D: cotas do Manual v1.0, Figura 3-1 (p. 6), lidas de uma imagem de
  baixa resolucao. TODO(hw): CONFERIR COM PAQUIMETRO NO MODULO FISICO.
* Modelos 3D: envelopes (caixas), nao os STEP do fabricante.
"""
import re
from pathlib import Path

from stepbox import Step

HW = Path(__file__).resolve().parent.parent
PRETTY = HW / "libs" / "footprints" / "lora-pump.pretty"
D3 = HW / "libs" / "3d"
KICAD_FP = Path(r"C:\Program Files\KiCad\10.0\share\kicad\footprints\RF_Module.pretty")

MODEL = '\t(model "${KIPRJMOD}/../libs/3d/%s.step"\n\t\t(offset (xyz 0 0 0))\n\t\t(scale (xyz 1 1 1))\n\t\t(rotate (xyz 0 0 0))\n\t)\n'


def mini1():
    src = (KICAD_FP / "ESP32-C6-MINI-1.kicad_mod").read_text(encoding="utf-8")
    txt = src.replace("ESP32-C6-MINI-1", "ESP32-C3-MINI-1")
    txt = txt.replace("esp32-c6-mini-1_mini-1u", "esp32-c3-mini-1")
    txt = txt.replace("2.4 GHz Wi-Fi, Bluetooth, Zigbee and Thread combo chip",
                      "2.4 GHz Wi-Fi and Bluetooth LE SoC module")
    txt = txt.replace("Zigbee Thread ", "")
    # remove o modelo 3D herdado do C6 (nao existe para o C3 na biblioteca oficial)
    txt = re.sub(r'\t\(model "\$\{KICAD10_3DMODEL_DIR\}[^"]*"[\s\S]*?\n\t\)\n', "", txt)
    txt = txt.rstrip()
    assert txt.endswith(")")
    txt = txt[:-1].rstrip() + "\n" + MODEL % "ESP32-C3-MINI-1" + ")\n"
    (PRETTY / "ESP32-C3-MINI-1.kicad_mod").write_text(txt, encoding="utf-8")

    # 3D: o eixo Y do modelo e o oposto do eixo Y do footprint.
    s = Step("ESP32-C3-MINI-1")
    s.box(-6.6, -5.65, 0.0, 6.6, 11.05, 0.8, (0.05, 0.25, 0.10))      # PCB do modulo
    s.box(-6.6, -5.65, 0.8, 6.6, 6.8, 2.4, (0.75, 0.75, 0.78))        # can metalico
    (D3 / "ESP32-C3-MINI-1.step").write_text(s.render(), encoding="utf-8")


def e220_t30d():
    x0, x1, y0, y1 = -4.38, 19.62, -1.5, 41.5   # contorno do modulo (24 x 43 mm)
    pads = []
    for i in range(7):
        shape = "rect" if i == 0 else "circle"
        pads.append(f'\t(pad "{i + 1}" thru_hole {shape} (at {i * 2.54:.2f} 0) (size 1.7 1.7) (drill 0.9) '
                    f'(layers "*.Cu" "*.Mask"))')
    for i in range(4):  # furos de fixacao (pinos 8-11), sem rede
        pads.append(f'\t(pad "{8 + i}" thru_hole circle (at {-0.18 + i * 2.54:.2f} 38.5) (size 1.7 1.7) '
                    f'(drill 0.9) (layers "*.Cu" "*.Mask"))')

    def rect(layer, a, b, c, d, w):
        return (f'\t(fp_rect (start {a} {b}) (end {c} {d}) (stroke (width {w}) (type default)) '
                f'(fill no) (layer "{layer}"))')

    gfx = [
        rect("F.SilkS", x0, y0, x1, y1, 0.15),
        rect("F.Fab", x0, y0, x1, y1, 0.1),
        # Courtyard so onde NAO pode haver peca: faixa dos pinos, furos de fixacao e SMA.
        # O resto do modulo fica 4,2 mm acima da placa: pecas baixas (<= 3 mm) cabem embaixo.
        # TODO(hw): confirmar no modulo fisico que o lado de baixo e liso.
        rect("F.CrtYd", -4.7, -1.8, 19.9, 1.8, 0.05),
        rect("F.CrtYd", -1.2, 37.3, 8.5, 39.7, 0.05),
        rect("F.CrtYd", 10.0, 30.3, 16.8, 41.8, 0.05),
        '\t(fp_text user "ANTENA" (at 13.4 36 0) (layer "F.SilkS") '
        '(effects (font (size 1 1) (thickness 0.15))))',
        '\t(fp_text reference "REF**" (at 7.62 -3.5 0) (layer "F.SilkS") '
        '(effects (font (size 1 1) (thickness 0.15))))',
        '\t(fp_text value "E220-900T30D" (at 7.62 20 0) (layer "F.Fab") '
        '(effects (font (size 1 1) (thickness 0.15))))',
    ]
    body = "\n".join(pads + gfx)
    txt = (
        '(footprint "E220-900T30D" (version 20241229) (generator "gen_footprints.py") (generator_version "1.0")\n'
        '\t(layer "F.Cu")\n'
        '\t(descr "Modulo LoRa EBYTE E220-900T30D, 24 x 43 mm, 7 pinos a 2,54 mm + 4 furos de fixacao. '
        'Cotas do Manual v1.0 Fig. 3-1 (p.6): CONFERIR COM PAQUIMETRO")\n'
        '\t(tags "LoRa E220 EBYTE T30D")\n\t(attr through_hole)\n'
        f'{body}\n' + MODEL % "E220-900T30D" + ')\n'
    )
    (PRETTY / "E220-900T30D.kicad_mod").write_text(txt, encoding="utf-8")

    s = Step("E220-900T30D")
    ym = lambda a, b: (-b, -a)  # (y0,y1) do footprint -> (Y0,Y1) do modelo
    a, b = ym(-1.5, 30.5)
    s.box(x0, a, 4.2, x1, b, 5.8, (0.10, 0.20, 0.55))                  # PCB azul do modulo
    a, b = ym(4.0, 22.0)
    s.box(2.0, a, 5.8, 16.0, b, 8.3, (0.55, 0.56, 0.60))               # blindagem
    a, b = ym(30.5, 41.5)
    s.box(10.3, a, 5.8, 16.5, b, 12.8, (0.85, 0.70, 0.20))             # conector SMA-K
    a, b = ym(-1.3, 1.3)
    s.box(-1.3, a, 0.0, 16.5, b, 4.2, (0.05, 0.05, 0.05))              # espacador dos pinos
    (D3 / "E220-900T30D.step").write_text(s.render(), encoding="utf-8")


def usbc():
    """USB-C HRO TYPE-C-31-M-12: o STEP oficial nao vem nesta instalacao do KiCad."""
    src = (KICAD_FP.parent / "Connector_USB.pretty" /
           "USB_C_Receptacle_HRO_TYPE-C-31-M-12.kicad_mod").read_text(encoding="utf-8")
    txt = re.sub(r'\t\(model "\$\{KICAD10_3DMODEL_DIR\}[^"]*"[\s\S]*?\n\t\)\n', "", src).rstrip()
    txt = txt[:-1].rstrip() + "\n" + MODEL % "USB_C_HRO_TYPE-C-31-M-12" + ")\n"
    (PRETTY / "USB_C_Receptacle_HRO_TYPE-C-31-M-12.kicad_mod").write_text(txt, encoding="utf-8")
    s = Step("USB_C_HRO_TYPE-C-31-M-12")
    s.box(-4.47, -3.7, 0.0, 4.47, 3.7, 3.26, (0.55, 0.56, 0.60))   # envelope da carcaca
    (D3 / "USB_C_HRO_TYPE-C-31-M-12.step").write_text(s.render(), encoding="utf-8")


def main():
    PRETTY.mkdir(parents=True, exist_ok=True)
    D3.mkdir(parents=True, exist_ok=True)
    mini1()
    e220_t30d()
    usbc()
    print("footprints e modelos escritos")


if __name__ == "__main__":
    main()
