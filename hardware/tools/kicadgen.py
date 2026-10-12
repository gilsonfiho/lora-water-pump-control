"""Gerador de esquematicos KiCad 10 (.kicad_sch) a partir de simbolos oficiais.

Cada pino usado recebe um fio curto e um rotulo de net (ou simbolo de
alimentacao). Nao ha fios entre componentes: a conectividade e por nome de net,
o que mantem o gerador simples e a netlist verificavel com o kicad-cli.
"""
import re
import uuid
from pathlib import Path

KICAD_SHARE = Path(r"C:\Program Files\KiCad\10.0\share\kicad")
PROJECT = "lora-pump"
STUB = 5.08  # comprimento do fio entre o pino e o rotulo, em mm
POWER_NETS = {"GND", "+3V3", "+5V", "+12V"}


# ---------------------------------------------------------------- S-expr ----
def tokenize(text):
    return re.findall(r'"(?:\\.|[^"\\])*"|\(|\)|[^\s()"]+', text)


def parse(text):
    stack, cur = [], []
    for tok in tokenize(text):
        if tok == "(":
            stack.append(cur)
            cur = []
        elif tok == ")":
            done, cur = cur, stack.pop()
            cur.append(done)
        else:
            cur.append(tok)
    return cur[0]


def dump(node, depth=0):
    """Serializa uma arvore de volta para texto, com indentacao por tabs."""
    if not isinstance(node, list):
        return node
    if all(not isinstance(x, list) for x in node):
        return "(" + " ".join(node) + ")"
    ind = "\t" * (depth + 1)
    out = ["(" + node[0]]
    inline = True
    for x in node[1:]:
        if isinstance(x, list):
            inline = False
            out.append("\n" + ind + dump(x, depth + 1))
        else:
            out.append((" " if inline else "\n" + ind) + x)
    out.append("\n" + "\t" * depth + ")")
    return "".join(out)


def q(s):
    s = s.replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return '"' + s + '"'


def unq(s):
    return s[1:-1] if s.startswith('"') else s


def snap(v, grid=2.54):
    """Alinha a coordenada a grade de conexao do KiCad."""
    return round(round(v / grid) * grid, 2)


def new_uuid():
    return str(uuid.uuid4())


# --------------------------------------------------------------- Simbolos ----
_lib_cache = {}


def load_symbol(libfile, name):
    """Le o simbolo `name` de um .kicad_sym; devolve a arvore (lista)."""
    path = Path(libfile)
    if not path.is_absolute():
        path = KICAD_SHARE / "symbols" / libfile
    key = str(path)
    if key not in _lib_cache:
        _lib_cache[key] = parse(path.read_text(encoding="utf-8"))
    for node in _lib_cache[key]:
        if isinstance(node, list) and node and node[0] == "symbol" and unq(node[1]) == name:
            if any(isinstance(x, list) and x[0] == "extends" for x in node):
                raise ValueError(f"{name}: simbolos com 'extends' nao suportados")
            return node
    raise KeyError(f"{name} nao encontrado em {path}")


def pins_of(symbol):
    """{numero: (x, y, angulo, nome, tipo)} de todas as unidades."""
    pins = {}

    def walk(n):
        for x in n:
            if isinstance(x, list):
                if x and x[0] == "pin":
                    at = next(y for y in x if isinstance(y, list) and y[0] == "at")
                    nm = next(y for y in x if isinstance(y, list) and y[0] == "name")
                    nu = next(y for y in x if isinstance(y, list) and y[0] == "number")
                    pins[unq(nu[1])] = (float(at[1]), float(at[2]), float(at[3]), unq(nm[1]), x[1])
                else:
                    walk(x)

    walk(symbol)
    return pins


# ------------------------------------------------------------------ Folha ----
class Sheet:
    def __init__(self, name, filename, title, rev, root_uuid, sheet_uuid, paper="A3"):
        self.name, self.filename, self.title, self.rev = name, filename, title, rev
        self.root_uuid, self.uuid, self.paper = root_uuid, sheet_uuid, paper
        self.lib_symbols = {}  # lib_id -> arvore
        self.items = []
        self.globals = set()

    # -- infra
    def _embed(self, lib_id, libfile):
        if lib_id in self.lib_symbols:
            return
        name = lib_id.split(":", 1)[1]
        sym = load_symbol(libfile, name)
        sym = [sym[0], q(lib_id)] + sym[2:]
        self.lib_symbols[lib_id] = sym

    def _pins(self, lib_id):
        return pins_of(self.lib_symbols[lib_id])

    def text(self, x, y, s, size=1.27):
        self.items.append(
            f'(text {q(s)} (exclude_from_sim no) (at {x} {y} 0) '
            f'(effects (font (size {size} {size})) (justify left top)) (uuid "{new_uuid()}"))'
        )

    def wire(self, x1, y1, x2, y2):
        self.items.append(
            f'(wire (pts (xy {x1:.2f} {y1:.2f}) (xy {x2:.2f} {y2:.2f})) '
            f'(stroke (width 0) (type default)) (uuid "{new_uuid()}"))'
        )

    def no_connect(self, x, y):
        self.items.append(f'(no_connect (at {x:.2f} {y:.2f}) (uuid "{new_uuid()}"))')

    def _label(self, net, x, y, dx, dy, is_global):
        ang = {(-1, 0): 180, (1, 0): 0, (0, 1): 270, (0, -1): 90}[(dx, dy)]
        just = "right" if ang in (180, 270) else "left"
        eff = f'(effects (font (size 1.27 1.27)) (justify {just} bottom))'
        if is_global:
            self.globals.add(net)
            self.items.append(
                f'(global_label {q(net)} (shape bidirectional) (at {x:.2f} {y:.2f} {ang}) '
                f'(fields_autoplaced yes) {eff} (uuid "{new_uuid()}") '
                f'(property "Intersheetrefs" "${{INTERSHEET_REFS}}" (at {x:.2f} {y:.2f} 0) '
                f'(effects (font (size 1.27 1.27)) hide)))'
            )
        else:
            self.items.append(
                f'(label {q(net)} (at {x:.2f} {y:.2f} {ang}) {eff} (uuid "{new_uuid()}"))'
            )

    def _power(self, net, x, y, ref_n):
        lib_id = f"power:{net}"
        self._embed(lib_id, "power.kicad_sym")
        ref = f"#PWR{ref_n}"
        self.items.append(
            f'(symbol (lib_id {q(lib_id)}) (at {x:.2f} {y:.2f} 0) (unit 1) '
            f'(exclude_from_sim no) (in_bom no) (on_board no) (dnp no) (uuid "{new_uuid()}") '
            f'(property "Reference" {q(ref)} (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) hide)) '
            f'(property "Value" {q(net)} (at {x:.2f} {y + (3 if net == "GND" else -3):.2f} 0) '
            f'(effects (font (size 1.27 1.27)))) '
            f'(pin "1" (uuid "{new_uuid()}")) '
            f'(instances (project {q(PROJECT)} (path "/{self.root_uuid}/{self.uuid}" '
            f'(reference {q(ref)}) (unit 1))))'
            f')'
        )

    _pwr_n = 0

    def pwr_flag(self, net, x, y, is_global=False):
        """PWR_FLAG ligado a uma net por um rotulo local no mesmo ponto."""
        self._embed("power:PWR_FLAG", "power.kicad_sym")
        x, y = snap(x), snap(y)
        Sheet._pwr_n += 1
        ref = f"#FLG{Sheet._pwr_n:02d}"
        self.items.append(
            f'(symbol (lib_id "power:PWR_FLAG") (at {x:.2f} {y:.2f} 0) (unit 1) '
            f'(exclude_from_sim no) (in_bom no) (on_board no) (dnp no) (uuid "{new_uuid()}") '
            f'(property "Reference" {q(ref)} (at {x:.2f} {y:.2f} 0) (effects (font (size 1.27 1.27)) hide)) '
            f'(property "Value" "PWR_FLAG" (at {x:.2f} {y - 3:.2f} 0) (effects (font (size 1.27 1.27)))) '
            f'(pin "1" (uuid "{new_uuid()}")) '
            f'(instances (project {q(PROJECT)} (path "/{self.root_uuid}/{self.uuid}" '
            f'(reference {q(ref)}) (unit 1))))'
            f')'
        )
        if net in POWER_NETS:
            self._power(net, x, y, 9000 + Sheet._pwr_n)
        else:
            self._label(net, x, y, 0, -1, is_global)

    # -- componente
    def add(self, lib_id, libfile, ref, value, x, y, nets, footprint="", mpn="",
            dnp=False, extra=None, globals_=(), nc=(), hide_value=False):
        """Posiciona um simbolo e liga cada pino de `nets` ({pino: net})."""
        self._embed(lib_id, libfile)
        pins = self._pins(lib_id)
        x, y = snap(x), snap(y)
        props = {"Reference": ref, "Value": value, "Footprint": footprint, "Datasheet": ""}
        if mpn:
            props["MPN"] = mpn
        props.update(extra or {})
        bits = []
        yoff = {"Reference": -6, "Value": 6}
        for k, v in props.items():
            hide = " hide" if k not in ("Reference", "Value") or (k == "Value" and hide_value) else ""
            bits.append(
                f'(property {q(k)} {q(v)} (at {x:.2f} {y + yoff.get(k, 0):.2f} 0) '
                f'(effects (font (size 1.27 1.27)){hide}))'
            )
        pin_ids = " ".join(f'(pin {q(n)} (uuid "{new_uuid()}"))' for n in pins)
        self.items.append(
            f'(symbol (lib_id {q(lib_id)}) (at {x:.2f} {y:.2f} 0) (unit 1) '
            f'(exclude_from_sim no) (in_bom yes) (on_board yes) (dnp {"yes" if dnp else "no"}) '
            f'(uuid "{new_uuid()}") {" ".join(bits)} {pin_ids} '
            f'(instances (project {q(PROJECT)} (path "/{self.root_uuid}/{self.uuid}" '
            f'(reference {q(ref)}) (unit 1))))'
            f')'
        )
        for num in nc:
            px, py, ang, _, _ = pins[num]
            self.no_connect(x + px, y - py)
        for num, net in nets.items():
            px, py, ang, _, _ = pins[num]
            wx, wy = x + px, y - py
            dx, dy = {0: (-1, 0), 180: (1, 0), 90: (0, 1), 270: (0, -1)}[int(ang)]
            ex, ey = wx + dx * STUB, wy + dy * STUB
            self.wire(wx, wy, ex, ey)
            if net in POWER_NETS:
                Sheet._pwr_n += 1
                self._power(net, ex, ey, Sheet._pwr_n)
            else:
                self._label(net, ex, ey, dx, dy, net in globals_)

    # -- saida
    def render(self):
        libs = "\n\t\t".join(dump(s, 2) for s in self.lib_symbols.values())
        body = "\n\t".join(self.items)
        return (
            f'(kicad_sch\n\t(version 20250114)\n\t(generator "eeschema")\n\t(generator_version "9.0")\n'
            f'\t(uuid "{self.uuid}")\n\t(paper "{self.paper}")\n'
            f'\t(title_block\n\t\t(title {q(self.title)})\n\t\t(date "2026-10-11")\n'
            f'\t\t(rev {q(self.rev)})\n\t\t(company "gilsonfiho")\n\t)\n'
            f'\t(lib_symbols\n\t\t{libs}\n\t)\n\t{body}\n'
            f'\t(sheet_instances\n\t\t(path "/" (page "1"))\n\t)\n\t(embedded_fonts no)\n)\n'
        )


def render_root(root_uuid, rev, sheets):
    """Folha raiz com um bloco por subfolha, em grade."""
    blocks = []
    for i, sh in enumerate(sheets):
        x, y = 30 + (i % 3) * 90, 40 + (i // 3) * 50
        blocks.append(
            f'(sheet (at {x} {y}) (size 70 30) (exclude_from_sim no) (in_bom yes) (on_board yes) (dnp no) '
            f'(fields_autoplaced yes) (stroke (width 0.1524) (type solid)) (fill (color 0 0 0 0.0000)) '
            f'(uuid "{sh.uuid}") '
            f'(property "Sheetname" {q(sh.name)} (at {x} {y - 1} 0) (effects (font (size 1.27 1.27)) (justify left bottom))) '
            f'(property "Sheetfile" {q(sh.filename)} (at {x} {y + 31} 0) (effects (font (size 1.27 1.27)) (justify left top))) '
            f'(instances (project {q(PROJECT)} (path "/{root_uuid}" (page "{i + 2}"))))'
            f')'
        )
    body = "\n\t".join(blocks)
    return (
        f'(kicad_sch\n\t(version 20250114)\n\t(generator "eeschema")\n\t(generator_version "9.0")\n'
        f'\t(uuid "{root_uuid}")\n\t(paper "A3")\n'
        f'\t(title_block\n\t\t(title "Controle de bomba d\'agua LoRa")\n\t\t(date "2026-10-11")\n'
        f'\t\t(rev {q(rev)})\n\t\t(company "gilsonfiho")\n'
        f'\t\t(comment 1 "Folha raiz: um bloco por subfolha")\n\t)\n'
        f'\t(lib_symbols)\n\t{body}\n'
        f'\t(sheet_instances\n\t\t(path "/" (page "1"))\n\t)\n\t(embedded_fonts no)\n)\n'
    )
