"""Serigrafia padronizada das placas.

Padrao (ver hardware/docs/decisoes.md):
  * texto de 0,8 mm, traco de 0,12 mm, MAIUSCULAS, sempre na horizontal;
  * designador (REF) na serigrafia so para CI, conectores, chaves, jumper, LED, transistor
    e indutor; resistores, capacitores e diodos ficam com o REF na camada Fab (montagem);
  * nenhum texto sobre pad, corpo de componente, furo ou modulo: cada texto procura a
    vaga livre mais proxima do que descreve (rotulos funcionais) ou do componente (REF);
  * um bloco de identificacao (papel + versao) e os avisos de seguranca no mesmo lugar.
"""
import math

import pcbnew

MM = pcbnew.FromMM
SIZE = 0.8
THICK = 0.12
CHAR_W = 0.78       # largura media de um caractere (mm) no tamanho SIZE
SHOW_REF = ("U", "J", "SW", "JP", "Q", "L", "H")
LED_VALUES = ("STATUS", "SAIDA ON")   # LEDs (por valor, nao por referencia)


def v(x, y):
    return pcbnew.VECTOR2I(MM(x), MM(y))


def bbox_mm(b, pad=0.0):
    return (b.GetX() / 1e6 - pad, b.GetY() / 1e6 - pad,
            b.GetRight() / 1e6 + pad, b.GetBottom() / 1e6 + pad)


def hit(a, b):
    return not (a[2] <= b[0] or b[2] <= a[0] or a[3] <= b[1] or b[3] <= a[1])


class Silk:
    def __init__(self, board, fps, W, H):
        self.board, self.fps, self.W, self.H = board, fps, W, H
        self.blocked = []
        for ref, fp in fps.items():
            cy = fp.GetCourtyard(pcbnew.F_CrtYd)
            self.blocked.append(bbox_mm(cy.BBox(), 0.15))
            for p in fp.Pads():
                self.blocked.append(bbox_mm(p.GetBoundingBox(), 0.15))
        # corpos dos modulos (o texto sob o modulo nao aparece depois de montado)
        for ref in ("U1", "U2"):
            if ref in fps:
                self.blocked.append(bbox_mm(fps[ref].GetBoundingBox(False), 0.2))
        for fp in board.GetFootprints():
            for z in fp.Zones():
                self.blocked.append(bbox_mm(z.GetBoundingBox(), 0.2))
            if fp.GetReference().startswith("H"):
                self.blocked.append(bbox_mm(fp.GetBoundingBox(False), 0.3))
        self.placed = []

    def free(self, box):
        if box[0] < 0.6 or box[1] < 0.6 or box[2] > self.W - 0.6 or box[3] > self.H - 0.6:
            return False
        return not any(hit(box, b) for b in self.blocked + self.placed)

    def box_of(self, s, cx, cy, rot=0):
        w, h = len(s) * CHAR_W + 0.2, SIZE + 0.3
        if rot:
            w, h = h, w
        return (cx - w / 2, cy - h / 2, cx + w / 2, cy + h / 2)

    def find(self, s, ax, ay, rot=0, radius=14.0):
        """Vaga livre mais proxima de (ax, ay), em passos de 0,4 mm."""
        cands = []
        steps = int(radius / 0.4)
        for i in range(-steps, steps + 1):
            for j in range(-steps, steps + 1):
                cands.append((math.hypot(i, j), ax + i * 0.4, ay + j * 0.4))
        cands.sort()
        for _, x, y in cands:
            if self.free(self.box_of(s, x, y, rot)):
                return x, y
        return None

    def put(self, s, x, y, rot=0, layer=pcbnew.F_SilkS):
        t = pcbnew.PCB_TEXT(self.board)
        t.SetText(s)
        t.SetPosition(v(x, y))
        t.SetLayer(layer)
        t.SetTextSize(v(SIZE, SIZE))
        t.SetTextThickness(MM(THICK))
        t.SetTextAngleDegrees(rot)
        self.board.Add(t)
        self.placed.append(self.box_of(s, x, y, rot))

    def label(self, s, ax, ay, rot=0):
        """Rotulo funcional: na vaga livre mais proxima da ancora."""
        pos = self.find(s, ax, ay, rot)
        if pos is None:
            print(f"AVISO: sem vaga para o texto '{s}'")
            return
        self.put(s, pos[0], pos[1], rot)

    def refs(self):
        for ref, fp in self.fps.items():
            r = fp.Reference()
            r.SetTextSize(v(SIZE, SIZE))
            r.SetTextThickness(MM(THICK))
            r.SetTextAngleDegrees(0)
            show = fp.GetValue() in LED_VALUES or any(ref.startswith(p) and ref[len(p):].isdigit()
                                                      for p in SHOW_REF)
            if ref.startswith("H"):
                show = False
            if not show:
                r.SetLayer(pcbnew.F_Fab)
                continue
            r.SetLayer(pcbnew.F_SilkS)
            cb = bbox_mm(fp.GetCourtyard(pcbnew.F_CrtYd).BBox())
            if ref == "U2":                                  # modulo E220: ref na borda livre
                cb = bbox_mm(fp.GetBoundingBox(False))
            cx, cy = (cb[0] + cb[2]) / 2, (cb[1] + cb[3]) / 2
            w = len(ref) * CHAR_W + 0.2
            tries = [(cx, cb[1] - 0.8), (cx, cb[3] + 0.8), (cb[0] - w / 2 - 0.3, cy),
                     (cb[2] + w / 2 + 0.3, cy)]
            for x, y in tries:
                box = self.box_of(ref, x, y)
                others = [b for b in self.blocked if not self._own(b, cb)]
                if (0.6 <= box[0] and box[2] <= self.W - 0.6 and 0.6 <= box[1]
                        and box[3] <= self.H - 0.6
                        and not any(hit(box, b) for b in others + self.placed)):
                    r.SetPosition(v(x, y))
                    self.placed.append(box)
                    break
            else:
                pos = self.find(ref, cx, cy)
                if pos:
                    r.SetPosition(v(*pos))
                    self.placed.append(self.box_of(ref, *pos))
                else:
                    r.SetLayer(pcbnew.F_Fab)                   # sem vaga: vai para a camada Fab

    @staticmethod
    def _own(b, cb):
        """Caixa que pertence ao proprio componente (courtyard/pads dentro da sua caixa)."""
        return (b[0] >= cb[0] - 0.4 and b[1] >= cb[1] - 0.4 and b[2] <= cb[2] + 0.4
                and b[3] <= cb[3] + 0.4)
