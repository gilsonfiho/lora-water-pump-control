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

W, H = 86.0, 52.0          # placa (mm)
E220_PIN1 = (W - 41.5, 21.1)  # pino 1 do E220: SMA rente a borda direita
ESP_POS = (11.05, 7.6)        # antena do MINI-1 rente a borda esquerda


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
    fps["J2"].SetPosition(vec(14.0, H - 4.2))          # USB-C: abertura para a borda de baixo
    fps["J3"].SetPosition(vec(48.0, H - 5.6))          # bornes na borda de baixo
    fps["J4"].SetPosition(vec(60.0, H - 5.6))
    px = E220_PIN1[0] - 3.2                            # desacoplamento ao lado dos pinos do E220
    for ref, dy in (("C5", -15.0), ("R8", -10.0), ("R7", -6.0)):
        fps[ref].SetOrientationDegrees(90)
        fps[ref].SetPosition(vec(px, E220_PIN1[1] + dy))

    fixed = {"U1", "U2", "J2", "J3", "J4", "C5", "R7", "R8"}
    groups = {
        (17.5, 1, 39.5, 29): ["C1", "C2", "R1", "C3", "R2", "R3", "R4", "R5", "R6", "SW1", "SW2",
                              "JP1", "D1", "J1"],
        (20.5, 30, 44.5, 51): ["U4", "R9", "R10", "D2", "U5", "C6", "C7", "C8", "R13", "R14"],
        (44, 27, 85.5, 38.5): ["C4", "D4", "D3", "F1", "R11", "R12", "D5", "D6", "R15", "R16",
                               "R17", "C9", "C10", "Q1"],
    }
    for region, refs in groups.items():
        pack([fps[r] for r in refs if r in fps], region, gap=0.8)
    left = [r for r in fps if r not in fixed and not any(r in v for v in groups.values())]
    if left:
        print("sem posicao:", left)
    for fp in fps.values():                            # referencias pequenas e finas
        fp.Reference().SetTextSize(vec(0.8, 0.8))
        fp.Reference().SetTextThickness(MM(0.12))

    # Contorno com cantos arredondados (raio 2 mm).
    r = 2.0
    c = [(r, 0, W - r, 0), (W, r, W, H - r), (W - r, H, r, H), (0, H - r, 0, r)]
    for x0, y0, x1, y1 in c:
        s = pcbnew.PCB_SHAPE(board)
        s.SetShape(pcbnew.SHAPE_T_SEGMENT)
        s.SetStart(vec(x0, y0))
        s.SetEnd(vec(x1, y1))
        s.SetLayer(pcbnew.Edge_Cuts)
        s.SetWidth(MM(0.1))
        board.Add(s)
    import math
    d = r * (1 - math.sqrt(0.5))
    for (cx, cy), (sx, sy), (mx, my), (ex, ey) in [
        ((r, r), (0, r), (d, d), (r, 0)),
        ((W - r, r), (W - r, 0), (W - d, d), (W, r)),
        ((W - r, H - r), (W, H - r), (W - d, H - d), (W - r, H)),
        ((r, H - r), (r, H), (d, H - d), (0, H - r)),
    ]:
        a_ = pcbnew.PCB_SHAPE(board)
        a_.SetShape(pcbnew.SHAPE_T_ARC)
        a_.SetArcGeometry(vec(sx, sy), vec(mx, my), vec(ex, ey))
        a_.SetLayer(pcbnew.Edge_Cuts)
        a_.SetWidth(MM(0.1))
        board.Add(a_)

    # Furos de fixacao M3 (nos cantos de baixo; os de cima ficam sob o modulo / a antena).
    for i, (x, y) in enumerate([(3.5, H - 3.5), (W - 3.5, H - 3.5)]):
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

    # Serigrafia (faixa livre abaixo da antena do MINI-1, x 0-17).
    emi = role == "emissor"
    for y, s_, sz in [(31.2, "NAO LIGAR O E220", 0.8), (32.6, "SEM ANTENA!", 0.8),
                      (34.6, "ENTRADA 5V APENAS", 0.8), (36.0, "NAO LIGAR 12V", 0.8),
                      (38.0, "JP1 PAPEL:", 0.8), (39.4, "ABERTO=RESERV." if emi else "FECHADO=BOMBA", 0.8),
                      (41.0, "EMISSOR" if emi else "RECEPTOR", 1.0)]:
        text(board, s_, 8.5, y, sz)
    text(board, "hw-v0.1 2026-10-11", 62, 3, 0.8)
    text(board, "J3: 5V+ | GND", 33.0, 44.0, 0.8)
    text(board, "J4: BOIA-B|GND|BOIA-A|GND" if emi else "J4: BOBINA 5V+ | -", 33.0, 46.5, 0.8)

    pcbnew.ZONE_FILLER(board).Fill(board.Zones())
    board.BuildConnectivity()
    out = HW / role / f"{role}.kicad_pcb"
    board.Save(str(out))
    print("escrito", out, f"({len(fps)} footprints, {len(nets)} nets)")


if __name__ == "__main__":
    main(sys.argv[1])
