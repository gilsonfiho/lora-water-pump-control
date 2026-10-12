"""Roteador simples (A* em grade, 2 camadas, vias) para as placas do projeto.

Roda sobre uma pcbnew.BOARD ja posicionada (ver build_pcb.py). GND nao e roteado como
trilha: cada pad SMD de GND recebe uma via para o plano da face de baixo, e o plano da
face de cima cobre o resto. Nao e um roteador de uso geral: serve a estas placas, e o
resultado deve ser conferido no KiCad (RF, potencia, USB).
"""
import heapq
import random
import math

import numpy as np
import pcbnew

MM = pcbnew.FromMM
PITCH = 0.1          # grade (mm)
CLR = 0.15           # folga minima (mm); JLCPCB atende 0,127 mm (conferir no site)
W_SIG, W_PWR = 0.2, 0.5
VIA_D, VIA_DRILL = 0.6, 0.3
EDGE = 0.5           # folga da borda da placa
POWER = {"+5V", "+5V_EXT", "5V_IN", "VBUS", "VCC_RF", "+3V3", "COIL_NEG"}
VIA_COST = 14.0
TURN = 0.6
TOL = 0.07
MAX_EXPAND = 300_000
SIG, PWR, VIA = 0, 1, 2   # classes de margem
MARGIN = [CLR + W_SIG / 2, CLR + W_PWR / 2, CLR + VIA_D / 2]
DIRS = [(1, 0, 1.0), (-1, 0, 1.0), (0, 1, 1.0), (0, -1, 1.0),
        (1, 1, 1.414), (1, -1, 1.414), (-1, 1, 1.414), (-1, -1, 1.414)]


def mm(v):
    return v / 1e6


class Router:
    def __init__(self, board, W, H):
        self.b, self.W, self.H = board, W, H
        self.nx, self.ny = int(W / PITCH) + 1, int(H / PITCH) + 1
        self.own = [np.zeros((2, self.ny, self.nx), np.int32) for _ in MARGIN]
        self.flat = [a.reshape(-1) for a in self.own]
        self.failed = []
        self.vias_xy = []
        self.n_tracks = self.n_vias = 0

    # ---- grade
    def idx(self, l, y, x):
        return (l * self.ny + y) * self.nx + x

    def cell(self, v):
        return int(round(v / PITCH))

    def paint(self, k, layer, x0, y0, x1, y1, net, margin=None, override=False):
        m = MARGIN[k] if margin is None else margin
        # so celulas cujo CENTRO fica dentro da margem: as trilhas passam pelos centros
        ix0, ix1 = max(0, math.ceil((x0 - m) / PITCH - 1e-9)), min(self.nx - 1, math.floor((x1 + m) / PITCH + 1e-9))
        iy0, iy1 = max(0, math.ceil((y0 - m) / PITCH - 1e-9)), min(self.ny - 1, math.floor((y1 + m) / PITCH + 1e-9))
        if ix0 > ix1 or iy0 > iy1:
            return
        sub = self.own[k][layer, iy0:iy1 + 1, ix0:ix1 + 1]
        if net == -1 or override:
            sub[:] = net
            return
        free = sub == 0
        other = (sub != 0) & (sub != net)
        sub[free] = net
        sub[other] = -1

    def paint_all(self, layer, x0, y0, x1, y1, net, extra=0.0):
        for k in range(len(MARGIN)):
            self.paint(k, layer, x0, y0, x1, y1, net, MARGIN[k] + extra)

    # ---- obstaculos iniciais
    def load(self):
        b = self.b
        ring = [(0, 0, EDGE, self.H), (self.W - EDGE, 0, self.W, self.H),
                (0, 0, self.W, EDGE), (0, self.H - EDGE, self.W, self.H)]
        for l in (0, 1):
            for r in ring:
                for k in range(len(MARGIN)):
                    self.paint(k, l, *r, -1, margin=0.0)
        pads = []
        for fp in b.GetFootprints():
            for z in fp.Zones():                      # keepout da antena (rule area)
                bb = z.GetBoundingBox()
                for l in (0, 1):
                    for k in range(len(MARGIN)):
                        self.paint(k, l, mm(bb.GetX()), mm(bb.GetY()), mm(bb.GetRight()),
                                   mm(bb.GetBottom()), -1, margin=0.0)
            for p in fp.Pads():
                bb = p.GetBoundingBox()
                r = (mm(bb.GetX()), mm(bb.GetY()), mm(bb.GetRight()), mm(bb.GetBottom()))
                layers = [l for l, ly in ((0, pcbnew.F_Cu), (1, pcbnew.B_Cu)) if p.IsOnLayer(ly)]
                net = p.GetNetCode()
                if p.GetAttribute() == pcbnew.PAD_ATTRIB_NPTH:
                    for l in (0, 1):
                        self.paint_all(l, *r, -1, extra=0.1)
                    continue
                if net == 0:
                    net = -1
                pads.append((p, fp, r, layers, net))
        for p, fp, r, layers, net in pads:            # margens primeiro, depois o cobre
            for l in layers:
                self.paint_all(l, *r, net)
        for p, fp, r, layers, net in pads:            # via nunca dentro de um pad
            for l in layers:
                self.paint(VIA, l, *r, -1, margin=0.0)
        return pads

    # ---- A*
    def passable(self, k, l, y, x, net):
        v = self.flat[k][self.idx(l, y, x)]
        return v == 0 or v == net

    def search(self, net, k, starts, goal, box):
        """A* com penalidade de curva; `box` = (x0, y0, x1, y1) em celulas, caixa do alvo."""
        nx, ny = self.nx, self.ny
        flat = self.flat[k]
        flatv = self.flat[VIA]
        g, par, dirm = {}, {}, {}
        heap = []
        for s in starts:
            g[s] = 0.0
            heapq.heappush(heap, (self._h(s, box), 0.0, s))
        n = 0
        while heap:
            f, gc, cur = heapq.heappop(heap)
            if gc > g.get(cur, 1e18) + 1e-9:
                continue
            if goal(cur):
                return self._path(par, cur)
            n += 1
            if n > MAX_EXPAND:
                return None
            l, rem = divmod(cur, ny * nx)
            y, x = divmod(rem, nx)
            din = dirm.get(cur, -1)
            for di, (dx, dy, c) in enumerate(DIRS):
                xx, yy = x + dx, y + dy
                if xx < 0 or yy < 0 or xx >= nx or yy >= ny:
                    continue
                ni = (l * ny + yy) * nx + xx
                v = flat[ni]
                if v != 0 and v != net:
                    continue
                if dx and dy:
                    a, b_ = flat[(l * ny + y) * nx + xx], flat[(l * ny + yy) * nx + x]
                    if (a != 0 and a != net) or (b_ != 0 and b_ != net):
                        continue
                ng = gc + c * (1.15 if l else 1.0) + (TURN if din >= 0 and di != din else 0.0)
                if ng < g.get(ni, 1e18):
                    g[ni] = ng
                    par[ni] = cur
                    dirm[ni] = di
                    heapq.heappush(heap, (ng + self._h(ni, box), ng, ni))
            oi = ((1 - l) * ny + y) * nx + x
            v1, v2, v3 = flatv[cur], flatv[oi], flat[oi]
            if (v1 == 0 or v1 == net) and (v2 == 0 or v2 == net) and (v3 == 0 or v3 == net):
                ng = gc + VIA_COST
                if ng < g.get(oi, 1e18):
                    g[oi] = ng
                    par[oi] = cur
                    dirm[oi] = -1
                    heapq.heappush(heap, (ng + self._h(oi, box), ng, oi))
        return None

    def _h(self, node, box):
        if not box:
            return 0.0
        rem = node % (self.ny * self.nx)
        y, x = divmod(rem, self.nx)
        dx = max(box[0] - x, 0, x - box[2])
        dy = max(box[1] - y, 0, y - box[3])
        return math.hypot(dx, dy)

    @staticmethod
    def _path(par, cur):
        out = [cur]
        while cur in par:
            cur = par[cur]
            out.append(cur)
        return out[::-1]

    # ---- escrita na placa
    def node_xy(self, node):
        l, rem = divmod(node, self.ny * self.nx)
        y, x = divmod(rem, self.nx)
        return l, x * PITCH, y * PITCH

    def add_track(self, layer, x0, y0, x1, y1, w, net):
        if abs(x0 - x1) < 1e-6 and abs(y0 - y1) < 1e-6:
            return
        t = pcbnew.PCB_TRACK(self.b)
        t.SetStart(pcbnew.VECTOR2I(MM(x0), MM(y0)))
        t.SetEnd(pcbnew.VECTOR2I(MM(x1), MM(y1)))
        t.SetWidth(MM(w))
        t.SetLayer(pcbnew.F_Cu if layer == 0 else pcbnew.B_Cu)
        t.SetNetCode(net)
        self.b.Add(t)
        self.n_tracks += 1
        steps = max(1, int(math.hypot(x1 - x0, y1 - y0) / (PITCH / 2)))
        for i in range(steps + 1):
            px, py = x0 + (x1 - x0) * i / steps, y0 + (y1 - y0) * i / steps
            for k in range(len(MARGIN)):
                self.paint(k, layer, px, py, px, py, net, margin=w / 2 + MARGIN[k])

    def add_via(self, x, y, net, d=VIA_D):
        self.vias_xy.append((x, y))
        v = pcbnew.PCB_VIA(self.b)
        v.SetPosition(pcbnew.VECTOR2I(MM(x), MM(y)))
        v.SetViaType(pcbnew.VIATYPE_THROUGH)
        v.SetWidth(MM(d))
        v.SetDrill(MM(VIA_DRILL))
        v.SetLayerPair(pcbnew.F_Cu, pcbnew.B_Cu)
        v.SetNetCode(net)
        self.b.Add(v)
        self.n_vias += 1
        for l in (0, 1):
            for k in range(len(MARGIN)):
                self.paint(k, l, x, y, x, y, net, margin=d / 2 + MARGIN[k])

    def commit(self, path, net, w, stub=None):
        """Converte o caminho de nos em trilhas e vias."""
        pts = [self.node_xy(n) for n in path]
        seg_start = 0
        for i in range(1, len(pts) + 1):
            end = i == len(pts)
            if not end:
                l0, x0, y0 = pts[seg_start]
                l1, x1, y1 = pts[i]
                if l1 != pts[i - 1][0]:
                    end = True            # troca de camada
                elif i - 1 > seg_start:
                    px, py = pts[i - 1][1:]
                    sx, sy = pts[i - 2][1:]
                    if abs((x1 - px) * (py - sy) - (y1 - py) * (px - sx)) > 1e-9:
                        end = True        # mudou de direcao
            if end:
                last = i - 1
                l, xa, ya = pts[seg_start]
                _, xb, yb = pts[last]
                self.add_track(l, xa, ya, xb, yb, w, net)
                if i < len(pts):
                    if pts[i][0] != l:
                        self.add_via(xb, yb, net)
                        seg_start = i
                    else:
                        seg_start = last
        if stub:
            l, sx, sy, px, py = stub
            n0 = self.node_xy(path[0])
            self.add_track(l, px, py, n0[1], n0[2], w, net)

    # ---- roteamento de uma net
    def route_net(self, net, name, pads, seed=None):
        w = W_PWR if name in POWER else W_SIG
        k = PWR if name in POWER else SIG
        tree = set()

        def add_tree(rect, layers):
            x0, y0, x1, y1 = rect
            for l in layers:
                for y in range(max(0, self.cell(y0)), min(self.ny, self.cell(y1) + 1)):
                    for x in range(max(0, self.cell(x0)), min(self.nx, self.cell(x1) + 1)):
                        tree.add(self.idx(l, y, x))

        def center_node(rect, l):
            cx, cy = (rect[0] + rect[2]) / 2, (rect[1] + rect[3]) / 2
            x, y = self.cell(cx), self.cell(cy)
            for r in range(0, 8):
                for dy in range(-r, r + 1):
                    for dx in range(-r, r + 1):
                        if max(abs(dx), abs(dy)) != r:
                            continue
                        xx, yy = x + dx, y + dy
                        if 0 <= xx < self.nx and 0 <= yy < self.ny and self.passable(k, l, yy, xx, net):
                            return self.idx(l, yy, xx), cx, cy
            return None, cx, cy

        if seed:
            tree.update(seed)
            sx, sy = self.node_xy(next(iter(seed)))[1:]
            first = (None, None, (sx, sy, sx, sy), [])
            tree_pads = []
            rest = list(pads)
        else:
            first = pads[0]
            add_tree(first[2], first[3])
            tree_pads = [first]
            rest = pads[1:]
        ok = True
        while rest:
            cx0 = (first[2][0] + first[2][2]) / 2
            cy0 = (first[2][1] + first[2][3]) / 2
            rest.sort(key=lambda p: math.hypot((p[2][0] + p[2][2]) / 2 - cx0, (p[2][1] + p[2][3]) / 2 - cy0))
            p = rest.pop(0)
            starts, info = [], None
            for l in p[3]:
                s, cx, cy = center_node(p[2], l)
                if s is not None:
                    starts.append(s)
                    info = (l, cx, cy)
            if not starts:
                ok = False
                continue
            arr = np.fromiter(tree, dtype=np.int64)
            rem = arr % (self.ny * self.nx)
            ys, xs = rem // self.nx, rem % self.nx
            box = (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))
            path = self.search(net, k, starts, lambda n: n in tree, box)
            if not path:
                ok = False
                continue
            l0, x0, y0 = self.node_xy(path[0])
            stub = (l0, 0, 0, info[1], info[2]) if info else None
            self.commit(path, net, w, stub)
            self._end_stub(path[-1], net, w, tree_pads)
            tree.update(path)
            tree_pads.append(p)
            add_tree(p[2], p[3])
            first = p
        if not ok:
            self.failed.append(name)

    def _end_stub(self, node, net, w, tree_pads):
        """Se o caminho terminou dentro do retangulo de um pad, liga ao centro dele."""
        l, x, y = self.node_xy(node)
        for p in tree_pads:
            r, layers = p[2], p[3]
            if l in layers and r[0] - TOL <= x <= r[2] + TOL and r[1] - TOL <= y <= r[3] + TOL:
                self.add_track(l, x, y, (r[0] + r[2]) / 2, (r[1] + r[3]) / 2, w, net)
                return

    def preroute_usb(self):
        """Escape do USB-C (passo 0,5 mm, D+ e D- intercalados): D+ junta por via + B.Cu, D- por F.Cu."""
        j2 = next((f for f in self.b.GetFootprints() if f.GetReference() == "J2"), None)
        seeds = {}
        if j2 is None:
            return seeds
        pads = {p.GetNumber(): p for p in j2.Pads() if p.GetNumber() in ("A6", "B6", "A7", "B7")}
        if len(pads) < 4:
            return seeds
        sign = 1 if mm(pads["A6"].GetY()) > mm(j2.GetY()) else -1
        pos = {k: (mm(p.GetX()), mm(p.GetY())) for k, p in pads.items()}
        y0 = pos["A6"][1]
        dp, dm = pads["A6"].GetNetCode(), pads["A7"].GetNetCode()
        w = W_SIG
        cells = {dp: set(), dm: set()}

        def mark(l, xa, ya, xb, yb, net):
            n = max(1, int(math.hypot(xb - xa, yb - ya) / PITCH))
            for i in range(n + 1):
                px, py = xa + (xb - xa) * i / n, ya + (yb - ya) * i / n
                cells[net].add(self.idx(l, self.cell(py), self.cell(px)))

        vias = []
        for k in ("B6", "A6"):                       # D+: stub, via 0,5 mm, B.Cu entre as vias
            x, y = pos[k]
            self.add_track(0, x, y, x, y + sign * 0.6, w, dp)
            self.add_via(x, y + sign * 0.9, dp, d=0.5)
            self.add_track(0, x, y + sign * 0.6, x, y + sign * 0.9, w, dp)
            vias.append((x, y + sign * 0.9))
            mark(0, x, y, x, y + sign * 0.9, dp)
        (xa, ya), (xb, yb) = vias
        self.add_track(1, xa, ya, xb, yb, w, dp)
        mark(1, xa, ya, xb, yb, dp)
        for k in ("A7", "B7"):                       # D-: stubs ate y0+1,4 e uniao em F.Cu
            x, y = pos[k]
            self.add_track(0, x, y, x, y + sign * 1.4, w, dm)
            mark(0, x, y, x, y + sign * 1.4, dm)
        xa, xb = pos["A7"][0], pos["B7"][0]
        self.add_track(0, xa, y0 + sign * 1.4, xb, y0 + sign * 1.4, w, dm)
        mark(0, xa, y0 + sign * 1.4, xb, y0 + sign * 1.4, dm)
        return {dp: cells[dp], dm: cells[dm]}

    # ---- vias de GND
    def gnd_vias(self, pads, skip_refs):
        for p, fp, r, layers, net in pads:
            if layers != [0] or fp.GetReference() in skip_refs:
                continue
            cx, cy = (r[0] + r[2]) / 2, (r[1] + r[3]) / 2
            x, y = self.cell(cx), self.cell(cy)
            if not (0 <= x < self.nx and 0 <= y < self.ny):
                print(f"AVISO: pad GND fora da grade: {fp.GetReference()}.{p.GetNumber()} em ({cx:.1f}, {cy:.1f})")
                continue
            start = self.idx(0, y, x)
            if self.flat[SIG][start] not in (0, net):
                continue

            def goal(n, cx=cx, cy=cy, net=net):
                l, nx_, ny_ = self.node_xy(n)
                if l != 0 or math.hypot(nx_ - cx, ny_ - cy) < 0.9:
                    return False
                if any(math.hypot(nx_ - vx, ny_ - vy) < 0.9 for vx, vy in self.vias_xy):
                    return False
                o = n + self.ny * self.nx
                return self.flat[VIA][n] in (0, net) and self.flat[VIA][o] in (0, net)

            path = self.search(net, SIG, [start], goal, None)
            if not path:
                # ultimo recurso: via de 0,5 mm no proprio pad, se a folga permitir
                if self.flat[PWR][start] in (0, net) and self.flat[PWR][start + self.ny * self.nx] in (0, net) and not any(
                        math.hypot(cx - vx, cy - vy) < 0.8 for vx, vy in self.vias_xy):
                    self.add_via(cx, cy, net, d=0.5)
                else:
                    self.failed.append(f"GND:{fp.GetReference()}.{p.GetNumber()}")
                continue
            self.commit(path, net, W_SIG)
            l, vx, vy = self.node_xy(path[-1])
            if not any(math.hypot(vx - ax, vy - ay) < 0.05 for ax, ay in self.vias_xy):
                self.add_via(vx, vy, net)

    def stitch_epad(self, fp, net):
        """Vias no EPAD (pads '49' da linha central do footprint) ligando os dois planos de GND."""
        done = set()
        for p in fp.Pads():
            if p.GetNumber() == "49" and p.GetNetCode() == net:
                rel = p.GetFPRelativePosition()
                pos = p.GetPosition()
                key = (round(mm(pos.x), 2), round(mm(pos.y), 2))
                if abs(mm(rel.y)) < 0.1 and key not in done:
                    done.add(key)
                    self.add_via(*key, net)


def _attempt(board, W, H, priority, seed=0):
    r = Router(board, W, H)
    pads = r.load()
    gnd = board.FindNet("GND").GetNetCode()
    nets = {}
    for p, fp, rect, layers, net in pads:
        if net > 0 and net != gnd:
            nets.setdefault(net, []).append((p, fp, rect, layers))
    items = []
    for net, ps in nets.items():
        if len(ps) < 2:
            continue
        name = board.FindNet(net).GetNetname()
        xs = [(q[2][0] + q[2][2]) / 2 for q in ps]
        ys = [(q[2][1] + q[2][3]) / 2 for q in ps]
        hp = (max(xs) - min(xs)) + (max(ys) - min(ys))
        items.append((0 if name in priority else 1, 0 if name in POWER else 1, hp, net, name, ps))
    rnd = random.Random(seed)
    items.sort(key=lambda t: (t[0], t[1], t[2] * (1.0 + (rnd.uniform(-0.4, 0.4) if seed else 0.0))))
    for fp in board.GetFootprints():
        if fp.GetReference() == "U1":
            r.stitch_epad(fp, gnd)
    gpads = [t for t in pads if t[4] == gnd]
    r.gnd_vias(gpads, skip_refs={"U1", "U2", "J2", "J1", "J3", "J4", "H1", "H2"})
    seeds = r.preroute_usb()
    for _, _, hp, net, name, ps in items:
        if net in seeds:
            ps = [q for q in ps if q[1].GetReference() != "J2"]
            r.route_net(net, name, ps, seed=seeds[net])
        else:
            r.route_net(net, name, ps)
    return r


def _clear(board):
    for t in list(board.GetTracks()):
        board.Remove(t)


def run(board, W, H, attempts=8):
    """Roteia; refaz dando prioridade as nets que falharam, e fica com a melhor tentativa."""
    priority, best = set(), None
    for i in range(attempts):
        r = _attempt(board, W, H, priority, seed=i)
        bad = [f for f in r.failed if not f.startswith("GND:")]
        score = len(r.failed) + 3 * len(bad)
        print(f"tentativa {i + 1}: {r.n_tracks} trilhas, {r.n_vias} vias; falhas: {r.failed or 'nenhuma'}")
        if best is None or score < best[0]:
            best = (score, set(priority), i)
        if not r.failed:
            return r
        if score > (best[0] if best else 99):
            pass
        priority |= set(bad)
        if i < attempts - 1:
            _clear(board)
    if best[2] != attempts - 1:  # a ultima nao foi a melhor: refaz a melhor
        _clear(board)
        r = _attempt(board, W, H, best[1], seed=best[2])
        print(f"melhor tentativa refeita: falhas: {r.failed or 'nenhuma'}")
    return r
