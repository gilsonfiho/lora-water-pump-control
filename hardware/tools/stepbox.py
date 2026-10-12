"""Escritor minimo de STEP (AP214) com caixas coloridas: modelos 3D de envelope.

Cada caixa e um solido B-rep (6 faces planas). Unidades em mm; o eixo Z e a
altura acima da PCB, como nos modelos do KiCad.
"""


class Step:
    def __init__(self, name):
        self.name = name
        self.ents = []
        self.solids = []  # (id, rgb)

    def e(self, text):
        self.ents.append(text)
        return len(self.ents)

    def ref(self, text):
        return f"#{self.e(text)}"

    def pt(self, x, y, z):
        return self.ref(f"CARTESIAN_POINT('',({x:.5f},{y:.5f},{z:.5f}))")

    def dr(self, x, y, z):
        return self.ref(f"DIRECTION('',({x:.5f},{y:.5f},{z:.5f}))")

    def box(self, x0, y0, z0, x1, y1, z1, rgb):
        P = {}
        for i, (x, y, z) in enumerate([(x0, y0, z0), (x1, y0, z0), (x1, y1, z0), (x0, y1, z0),
                                       (x0, y0, z1), (x1, y0, z1), (x1, y1, z1), (x0, y1, z1)]):
            P[i] = (x, y, z)
        cp = {i: self.pt(*P[i]) for i in P}
        vp = {i: self.ref(f"VERTEX_POINT('',{cp[i]})") for i in P}
        edges = {}

        def edge(a, b):
            if (a, b) in edges:
                return edges[(a, b)], True
            if (b, a) in edges:
                return edges[(b, a)], False
            ax, ay, az = P[a]
            bx, by, bz = P[b]
            dx, dy, dz = bx - ax, by - ay, bz - az
            ln = (dx * dx + dy * dy + dz * dz) ** 0.5
            vec = self.ref(f"VECTOR('',{self.dr(dx / ln, dy / ln, dz / ln)},{ln:.5f})")
            line = self.ref(f"LINE('',{cp[a]},{vec})")
            ec = self.ref(f"EDGE_CURVE('',{vp[a]},{vp[b]},{line},.T.)")
            edges[(a, b)] = ec
            return ec, True

        # faces: (vertices em sentido anti-horario visto de fora, normal externa, eixo-x)
        faces = [
            ((0, 3, 2, 1), (0, 0, -1), (1, 0, 0)),
            ((4, 5, 6, 7), (0, 0, 1), (1, 0, 0)),
            ((0, 1, 5, 4), (0, -1, 0), (1, 0, 0)),
            ((2, 3, 7, 6), (0, 1, 0), (-1, 0, 0)),
            ((1, 2, 6, 5), (1, 0, 0), (0, 1, 0)),
            ((3, 0, 4, 7), (-1, 0, 0), (0, -1, 0)),
        ]
        fr = []
        for vs, n, xd in faces:
            oes = []
            for i in range(4):
                a, b = vs[i], vs[(i + 1) % 4]
                ec, fwd = edge(a, b)
                oes.append(self.ref(f"ORIENTED_EDGE('',*,*,{ec},{'.T.' if fwd else '.F.'})"))
            loop = self.ref(f"EDGE_LOOP('',({','.join(oes)}))")
            bound = self.ref(f"FACE_OUTER_BOUND('',{loop},.T.)")
            ax = self.ref(f"AXIS2_PLACEMENT_3D('',{cp[vs[0]]},{self.dr(*n)},{self.dr(*xd)})")
            plane = self.ref(f"PLANE('',{ax})")
            fr.append(self.ref(f"ADVANCED_FACE('',({bound}),{plane},.T.)"))
        shell = self.ref(f"CLOSED_SHELL('',({','.join(fr)}))")
        solid = self.ref(f"MANIFOLD_SOLID_BREP('{self.name}',{shell})")
        self.solids.append((solid, rgb))

    def render(self):
        app = self.ref("APPLICATION_CONTEXT('core data for automotive mechanical design processes')")
        self.ref(f"APPLICATION_PROTOCOL_DEFINITION('international standard','automotive_design',2000,{app})")
        pctx = self.ref(f"PRODUCT_CONTEXT('',{app},'mechanical')")
        prod = self.ref(f"PRODUCT('{self.name}','{self.name}','',({pctx}))")
        pdf = self.ref(f"PRODUCT_DEFINITION_FORMATION('','',{prod})")
        pdc = self.ref(f"PRODUCT_DEFINITION_CONTEXT('part definition',{app},'design')")
        pd = self.ref(f"PRODUCT_DEFINITION('design','',{pdf},{pdc})")
        pds = self.ref(f"PRODUCT_DEFINITION_SHAPE('','',{pd})")
        mm = self.ref("( LENGTH_UNIT() NAMED_UNIT(*) SI_UNIT(.MILLI.,.METRE.) )")
        ang = self.ref("( NAMED_UNIT(*) PLANE_ANGLE_UNIT() SI_UNIT($,.RADIAN.) )")
        sa = self.ref("( NAMED_UNIT(*) SI_UNIT($,.STERADIAN.) SOLID_ANGLE_UNIT() )")
        unc = self.ref(f"UNCERTAINTY_MEASURE_WITH_UNIT(LENGTH_MEASURE(1.E-05),{mm},'distance_accuracy_value','')")
        ctx = self.ref(f"( GEOMETRIC_REPRESENTATION_CONTEXT(3) GLOBAL_UNCERTAINTY_ASSIGNED_CONTEXT(({unc})) "
                       f"GLOBAL_UNIT_ASSIGNED_CONTEXT(({mm},{ang},{sa})) REPRESENTATION_CONTEXT('','') )")
        org = self.ref(f"AXIS2_PLACEMENT_3D('',{self.pt(0, 0, 0)},{self.dr(0, 0, 1)},{self.dr(1, 0, 0)})")
        items = ",".join([org] + [s for s, _ in self.solids])
        rep = self.ref(f"ADVANCED_BREP_SHAPE_REPRESENTATION('',({items}),{ctx})")
        self.ref(f"SHAPE_DEFINITION_REPRESENTATION({pds},{rep})")
        styled = []
        for solid, (r, g, b) in self.solids:
            col = self.ref(f"COLOUR_RGB('',{r:.4f},{g:.4f},{b:.4f})")
            fc = self.ref(f"FILL_AREA_STYLE_COLOUR('',{col})")
            fs = self.ref(f"FILL_AREA_STYLE('',({fc}))")
            sf = self.ref(f"SURFACE_STYLE_FILL_AREA({fs})")
            ss = self.ref(f"SURFACE_SIDE_STYLE('',({sf}))")
            su = self.ref(f"SURFACE_STYLE_USAGE(.BOTH.,{ss})")
            pa = self.ref(f"PRESENTATION_STYLE_ASSIGNMENT(({su}))")
            styled.append(self.ref(f"STYLED_ITEM('color',({pa}),{solid})"))
        self.ref(f"MECHANICAL_DESIGN_GEOMETRIC_PRESENTATION_REPRESENTATION('',({','.join(styled)}),{ctx})")
        body = "\n".join(f"#{i + 1}={t};" for i, t in enumerate(self.ents))
        return ("ISO-10303-21;\nHEADER;\nFILE_DESCRIPTION(('Modelo de envelope gerado por stepbox.py'),'2;1');\n"
                f"FILE_NAME('{self.name}.step','2026-10-11T00:00:00',('lora-water-pump-control'),(''),'stepbox','','');\n"
                "FILE_SCHEMA(('AUTOMOTIVE_DESIGN { 1 0 10303 214 1 1 1 1 }'));\nENDSEC;\nDATA;\n"
                f"{body}\nENDSEC;\nEND-ISO-10303-21;\n")
