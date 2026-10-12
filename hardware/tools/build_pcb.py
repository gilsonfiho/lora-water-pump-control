"""Gera a PCB de uma placa (emissor ou receptor) a partir do esquematico.

Uso (com o Python do KiCad):
  "C:\\Program Files\\KiCad\\10.0\\bin\\python.exe" hardware/tools/build_pcb.py emissor

Le a netlist exportada pelo kicad-cli, carrega os footprints, posiciona, desenha o
contorno, os furos de fixacao, a serigrafia e o plano de GND. NAO roteia: as trilhas
ficam a cargo do KiCad/usuario (ver hardware/docs/decisoes.md).
"""
import subprocess
import sys
from pathlib import Path

import pcbnew

sys.path.insert(0, str(Path(__file__).resolve().parent))
import kicadgen as k  # noqa: E402

HW = Path(__file__).resolve().parent.parent
KICAD = Path(r"C:\Program Files\KiCad\10.0")
FP_DIRS = {"lora-pump": HW / "libs" / "footprints" / "lora-pump.pretty"}
MM = pcbnew.FromMM

W, H = 100.0, 76.0          # placa (mm)
E220_PIN1 = (W - 41.5, 36.0)  # pino 1 do E220: SMA rente a borda direita
ESP_POS = (11.05, 38.0)       # antena do MINI-1 rente a borda esquerda


def vec(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))


def netlist(role):
    out = HW / role / "_netlist.net"
    subprocess.run([str(KICAD / "bin" / "kicad-cli.exe"), "sch", "export", "netlist",
                    str(HW / role / f"{role}.kicad_sch"), "-o", str(out), "--format", "kicadsexpr"],
                   check=True, capture_output=True)
    tree = k.parse(out.read_text(encoding="utf-8"))
    out.unlink()
    comps, nets = {}, {}
    for sec in tree:
        if not isinstance(sec, list):
            continue
        if sec[0] == "components":
            for c in sec[1:]:
                d = {x[0]: x[1:] for x in c if isinstance(x, list)}
                ref = k.unq(d["ref"][0])
                tst = k.unq(d["tstamps"][0]) if "tstamps" in d else ""
                sp = next((x for x in c if isinstance(x, list) and x[0] == "sheetpath"), None)
                spath = k.unq(next(y for y in sp if isinstance(y, list) and y[0] == "tstamps")[1]) if sp else "/"
                comps[ref] = {"value": k.unq(d["value"][0]), "fp": k.unq(d["footprint"][0]),
                              "path": spath.rstrip("/") + "/" + tst}
        elif sec[0] == "nets":
            for n in sec[1:]:
                d = [x for x in n if isinstance(x, list)]
                name = k.unq(next(x for x in d if x[0] == "name")[1])
                nets[name] = [(k.unq(next(y for y in x if isinstance(y, list) and y[0] == "ref")[1]),
                               k.unq(next(y for y in x if isinstance(y, list) and y[0] == "pin")[1]))
                              for x in d if x[0] == "node"]
    return comps, nets


def load_fp(fpid):
    nick, name = fpid.split(":")
    lib = FP_DIRS.get(nick, KICAD / "share" / "kicad" / "footprints" / f"{nick}.pretty")
    fp = pcbnew.FootprintLoad(str(lib), name)
    fp.SetFPID(pcbnew.LIB_ID(nick, name))
    return fp


def crtyd(fp):
    b = fp.GetCourtyard(pcbnew.F_CrtYd).BBox()
    return b.GetX() / 1e6, b.GetY() / 1e6, b.GetWidth() / 1e6, b.GetHeight() / 1e6


def pack(fps, region, gap=1.0):
    """Empacota os footprints em linhas dentro de `region` (x0, y0, x1, y1)."""
    x0, y0, x1, y1 = region
    x, y, rowh = x0, y0, 0.0
    for fp in fps:
        bx, by, w, h = crtyd(fp)
        if x + w > x1 and x > x0:
            x, y, rowh = x0, y + rowh + gap, 0.0
        fp.SetPosition(vec(x - bx, y - by))
        x += w + gap
        rowh = max(rowh, h)
    if y + rowh > y1:
        print(f"AVISO: regiao {region} estourou em {y + rowh - y1:.1f} mm")


def text(board, s, x, y, size=1.0, layer=pcbnew.F_SilkS, rot=0):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(s)
    t.SetPosition(vec(x, y))
    t.SetLayer(layer)
    t.SetTextSize(vec(size, size))
    t.SetTextThickness(MM(size * 0.15))
    t.SetTextAngleDegrees(rot)
    board.Add(t)


def main(role):
    comps, nets = netlist(role)
    board = pcbnew.BOARD()
    board.SetCopperLayerCount(2)
    tb = board.GetTitleBlock()
    tb.SetTitle("Controle de bomba d'agua LoRa - " + ("Emissor (reservatorio)" if role == "emissor" else "Receptor (bomba)"))
    tb.SetRevision("0.1")
    tb.SetDate("2026-10-11")
    tb.SetCompany("gilsonfiho")

    fps = {}
    for ref, c in sorted(comps.items()):
        fp = load_fp(c["fp"])
        fp.SetReference(ref)
        fp.SetValue(c["value"])
        fp.SetPath(pcbnew.KIID_PATH(c["path"]))
        fps[ref] = fp
        board.Add(fp)

    # Nets e pads.
    for name, nodes in nets.items():
        ni = pcbnew.NETINFO_ITEM(board, name)
        board.Add(ni)
        for ref, pin in nodes:
            for p in fps[ref].Pads():
                if p.GetNumber() == pin:
                    p.SetNet(ni)

    # Modulos principais.
    fps["U2"].SetOrientationDegrees(90)
    fps["U2"].SetPosition(vec(*E220_PIN1))
    fps["U1"].SetOrientationDegrees(90)
    fps["U1"].SetPosition(vec(*ESP_POS))
    fps["J2"].SetOrientationDegrees(180)
    fps["J2"].SetPosition(vec(36.0, 4.4))
    fps["C4"].SetPosition(vec(64.0, 9.0))
    px = E220_PIN1[0] - 3.2
    for ref, dy in (("C5", -15.0), ("R8", -10.0), ("R7", -6.0)):
        fps[ref].SetOrientationDegrees(90)
        fps[ref].SetPosition(vec(px, E220_PIN1[1] + dy))

    fixed = {"U1", "U2", "J2", "C4", "C5", "R7", "R8"}
    groups = {
        (19, 12, 56, 26): ["U4", "R9", "R10", "D2", "U5", "C6", "C7", "C8", "R13", "R14"],
        (19, 29, 56, 54): ["C1", "C2", "R1", "C3", "R2", "R3", "R4", "R5", "R6", "SW1", "SW2",
                           "JP1", "D1", "J1"],
        (19, 57, 56, 74): ["J3", "F1", "D3", "D4", "R11", "R12"],
        (58, 54, 99, 74): ["J4", "D5", "D6", "R15", "R16", "R17", "C9", "C10", "Q1"],
    }
    for region, refs in groups.items():
        pack([fps[r] for r in refs if r in fps], region)
    left = [r for r in fps if r not in fixed and not any(r in v for v in groups.values())]
    if left:
        print("sem posicao:", left)

    # Contorno da placa.
    corners = [(0, 0), (W, 0), (W, H), (0, H)]
    for i in range(4):
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(vec(*corners[i]))
        s.SetEnd(vec(*corners[(i + 1) % 4]))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(MM(0.1))
        board.Add(s)

    # Furos de fixacao M3.
    for i, (x, y) in enumerate([(4, 4), (W - 4, 4), (4, H - 4), (W - 4, H - 4)]):
        mh = load_fp("MountingHole:MountingHole_3.2mm_M3")
        mh.SetReference(f"H{i + 1}")
        mh.SetPosition(vec(x, y))
        mh.Reference().SetVisible(False)
        board.Add(mh)

    # Plano de GND nas duas faces.
    gnd = board.FindNet("GND")
    for layer in (pcbnew.F_Cu, pcbnew.B_Cu):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(gnd)
        z.SetMinThickness(MM(0.2))
        z.SetLocalClearance(MM(0.3))
        z.SetPadConnection(pcbnew.ZONE_CONNECTION_FULL)
        o = z.Outline()
        o.NewOutline()
        for x, y in [(0.3, 0.3), (W - 0.3, 0.3), (W - 0.3, H - 0.3), (0.3, H - 0.3)]:
            o.Append(MM(x), MM(y))
        board.Add(z)

    # Serigrafia.
    papel = "EMISSOR (RESERVATORIO)" if role == "emissor" else "RECEPTOR (BOMBA)"
    text(board, papel, 80, 6, 1.8)
    text(board, "hw-v0.1  2026-10-11", 80, 10, 1.0)
    text(board, "NAO LIGAR O E220 SEM ANTENA!", 58, 44, 1.2)
    text(board, "O E220 TRANSMITE NO BOOT", 58, 47, 1.0)
    text(board, "ENTRADA 5V APENAS - NAO LIGAR 12V", 38, 55.5, 0.9)
    if role == "emissor":
        text(board, "BOIAS: BAIXA | GND | ALTA | GND", 58, 51, 0.9)
    else:
        text(board, "BOBINA 5V: + | -", 58, 51, 0.9)
    text(board, "JP1 PAPEL: " + ("ABERTO=RESERV." if role == "emissor" else "FECHADO=BOMBA"), 38, 51.5, 0.9)

    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.BuildConnectivity()
    out = HW / role / f"{role}.kicad_pcb"
    board.Save(str(out))
    print("escrito", out, f"({len(fps)} footprints, {len(nets)} nets)")


if __name__ == "__main__":
    main(sys.argv[1])
