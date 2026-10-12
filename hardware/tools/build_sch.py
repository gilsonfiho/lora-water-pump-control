"""Gera os esquematicos das duas placas: emissor (reservatorio) e receptor (bomba).

Uso:  python hardware/tools/build_sch.py
Saida: hardware/emissor/ e hardware/receptor/ (projeto KiCad completo cada um).
Folhas: MCU, Radio E220, Alimentacao, Entradas/Saidas. A conectividade e por nome
de net (ver kicadgen.py). Fontes de cada decisao: hardware/docs/decisoes.md.
"""
import json
from pathlib import Path

import kicadgen as k

HW = Path(__file__).resolve().parent.parent
LIB = str(HW / "libs" / "symbols" / "lora-pump.kicad_sym")
REV = "0.1"

FP_R = "Resistor_SMD:R_0603_1608Metric"
FP_C = "Capacitor_SMD:C_0603_1608Metric"
MPN_R = {"10k": "RC0603FR-0710KL", "4k7": "RC0603FR-074K7L", "1k": "RC0603FR-071KL",
         "5k1": "RC0603FR-075K1L", "100k": "RC0603FR-07100KL", "100R": "RC0603FR-07100RL",
         "0R": "RC0603ZR-070RL"}
MPN_C = {"100nF": "CL10B104KB8NNNC", "10uF": "CL10A106KP8NNNC", "1uF": "CL10A105KB8NNNC"}

ROLES = {
    "emissor": {"titulo": "Emissor (reservatorio)", "gpio1": "FLOAT_LOW",
                "root": "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f802001", "n": 2},
    "receptor": {"titulo": "Receptor (bomba)", "gpio1": "PUMP_DRV",
                 "root": "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f803001", "n": 3},
}
COMMON_GLOBALS = {
    "E220_M0", "E220_M1", "E220_RXD", "E220_TXD", "E220_AUX", "VCC_RF", "BAT_ADC",
    "FLOAT_HIGH", "ROLE_STRAP", "I2C_SDA", "I2C_SCL", "USB_DM", "USB_DP",
    "EXT_PRESENT", "LED_STATUS_DRV", "FLOAT_LOW", "PUMP_DRV",
}


class Ctx:
    def __init__(self, role):
        self.role = role
        self.cfg = ROLES[role]
        self.root = self.cfg["root"]
        self.gpio1 = self.cfg["gpio1"]
        self.G = COMMON_GLOBALS


def R(sh, c, ref, val, x, y, n1, n2, dnp=False):
    sh.add("Device:R", "Device.kicad_sym", ref, val, x, y, {"1": n1, "2": n2},
           footprint=FP_R, mpn=MPN_R[val], dnp=dnp, globals_=c.G)


def C(sh, c, ref, val, x, y, n1, n2):
    sh.add("Device:C", "Device.kicad_sym", ref, val, x, y, {"1": n1, "2": n2},
           footprint=FP_C, mpn=MPN_C[val], globals_=c.G)


# ---------------------------------------------------------------- MCU ------
def sheet_mcu(c, uid):
    sh = k.Sheet("MCU", "mcu.kicad_sch", f"MCU - {c.cfg['titulo']}", REV, c.root, uid)
    sh.text(15, 12, "MCU - ESP32-C3-MINI-1-H4X (Datasheet v2.2)", 2.5)
    sh.text(15, 20, "Alimentacao 3V3 (3,0-3,6 V). EN com RC de 10 k + 1 uF (p.34).\n"
                    "Strapping (Tabela 4-3, p.13): GPIO9=1 no boot (pull-up 4k7 + botao BOOT);\n"
                    "GPIO8=1 para download (pull-up 4k7, tambem SDA); GPIO2 com pull-up (recomendado).\n"
                    "GPIO18/19 = USB Serial/JTAG. GPIO20/21 = UART0 (console): ver decisoes.md.")
    sh.add("lora-pump:ESP32-C3-MINI-1", LIB, "U1", "ESP32-C3-MINI-1-H4X", 210, 150, {
        "3": "+3V3", "1": "GND",
        "8": "MCU_EN", "12": "BAT_ADC", "13": c.gpio1, "5": "FLOAT_HIGH",
        "6": "ROLE_STRAP", "18": "E220_TXD", "19": "E220_RXD", "20": "E220_M0",
        "21": "E220_M1", "22": "I2C_SDA", "23": "I2C_SCL", "16": "E220_AUX",
        "26": "USB_DM", "27": "USB_DP", "30": "EXT_PRESENT", "31": "LED_STATUS_DRV",
    }, footprint="lora-pump:ESP32-C3-MINI-1", mpn="ESP32-C3-MINI-1-H4X", globals_=c.G)
    C(sh, c, "C1", "10uF", 40, 80, "+3V3", "GND")
    C(sh, c, "C2", "100nF", 65, 80, "+3V3", "GND")
    R(sh, c, "R1", "10k", 100, 80, "+3V3", "MCU_EN")
    C(sh, c, "C3", "1uF", 125, 80, "MCU_EN", "GND")
    sh.add("Switch:SW_Push", "Switch.kicad_sym", "SW1", "RESET", 160, 80,
           {"1": "MCU_EN", "2": "GND"}, footprint="Button_Switch_SMD:SW_SPST_TL3342",
           mpn="TL3342F160QG")
    R(sh, c, "R2", "10k", 200, 80, "+3V3", "FLOAT_HIGH")
    R(sh, c, "R3", "4k7", 225, 80, "+3V3", "I2C_SDA")
    R(sh, c, "R4", "4k7", 250, 80, "+3V3", "I2C_SCL")
    sh.add("Switch:SW_Push", "Switch.kicad_sym", "SW2", "BOOT", 285, 80,
           {"1": "I2C_SCL", "2": "GND"}, footprint="Button_Switch_SMD:SW_SPST_TL3342",
           mpn="TL3342F160QG", globals_=c.G)
    R(sh, c, "R5", "10k", 320, 80, "+3V3", "ROLE_STRAP")
    # Papel fixo por placa: emissor = jumper ABERTO (reservatorio); receptor = FECHADO (bomba).
    if c.role == "receptor":
        sh.add("Jumper:SolderJumper_2_Bridged", "Jumper.kicad_sym", "JP1", "PAPEL=BOMBA", 355, 80,
               {"1": "ROLE_STRAP", "2": "GND"},
               footprint="Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm", globals_=c.G)
    else:
        sh.add("Jumper:SolderJumper_2_Open", "Jumper.kicad_sym", "JP1", "PAPEL=RESERV.", 355, 80,
               {"1": "ROLE_STRAP", "2": "GND"},
               footprint="Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm", globals_=c.G)
    R(sh, c, "R6", "1k", 320, 200, "LED_STATUS_DRV", "LED_A")
    sh.add("Device:LED", "Device.kicad_sym", "D1", "STATUS", 355, 200,
           {"1": "GND", "2": "LED_A"}, footprint="LED_SMD:LED_0603_1608Metric", mpn="APT1608SGC")
    sh.add("Connector_Generic:Conn_01x04", "Connector_Generic.kicad_sym", "J1", "I2C", 355, 150,
           {"1": "+3V3", "2": "I2C_SDA", "3": "I2C_SCL", "4": "GND"},
           footprint="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
           mpn="PinHeader_1x04_P2.54mm", globals_=c.G)
    return sh


# ------------------------------------------------------------- Radio -------
def sheet_radio(c, uid):
    sh = k.Sheet("Radio E220", "radio.kicad_sch", "Radio - E220-900T30D", REV, c.root, uid)
    sh.text(15, 12, "RADIO - EBYTE E220-900T30D (par do prototipo)", 2.5)
    sh.text(15, 22,
            "!! O E220 TRANSMITE LOGO NO BOOT: NUNCA ENERGIZAR SEM ANTENA NO SMA DO MODULO !!\n"
            "A antena e o SMA-K do proprio modulo (Manual T30D, Tabela 2-2, p.5): nao ha SMA na PCB.\n"
            "Pinos (Tabela 3-1, p.6): 1 M0, 2 M1, 3 RXD, 4 TXD, 5 AUX, 6 VCC, 7 GND; 8-11 furos de fixacao.\n"
            "UART/M0/M1/AUX em 3,3 V. VCC em 5 V (>= 5,0 V para potencia plena): 620 mA de pico em TX.\n"
            "Entrada maxima do RX: +10 dBm (nos a >= 3 m na bancada).\n"
            "Pull-ups de 4-10 k em TXD/AUX so para MCU de 5 V (Tabela 4-1, p.7): DNP aqui (MCU de 3,3 V).")
    sh.add("lora-pump:E220-900TxxD", LIB, "U2", "E220-900T30D", 110, 130,
           {"1": "E220_M0", "2": "E220_M1", "3": "E220_RXD", "4": "E220_TXD",
            "5": "E220_AUX", "6": "VCC_RF", "7": "GND"},
           footprint="lora-pump:E220-900T30D", mpn="E220-900T30D", globals_=c.G)
    sh.add("Device:C_Polarized", "Device.kicad_sym", "C4", "470uF/10V", 190, 130,
           {"1": "VCC_RF", "2": "GND"}, footprint="Capacitor_SMD:CP_Elec_8x10.5",
           mpn="EEE-FK1A471P", globals_=c.G)
    sh.text(175, 150, "TODO(hw): 470 uF e palpite;\nbancada usa 1000 uF", 1.0)
    C(sh, c, "C5", "100nF", 230, 130, "VCC_RF", "GND")
    R(sh, c, "R7", "10k", 260, 130, "+3V3", "E220_TXD", dnp=True)
    R(sh, c, "R8", "10k", 290, 130, "+3V3", "E220_AUX", dnp=True)
    return sh


# ---------------------------------------------------------- Alimentacao ----
def sheet_power(c, uid):
    sh = k.Sheet("Alimentacao", "power.kicad_sch", "Alimentacao e USB", REV, c.root, uid)
    sh.text(15, 12, "ALIMENTACAO - 5 V (USB-C ou borne) -> +5V -> LDO 3V3", 2.5)
    sh.text(15, 22,
            "Entrada externa SO 5 V (nao ligar 12 V!). Dois Schottky fazem o OR USB/borne: queda de ~0,3-0,5 V,\n"
            "o que deixa o E220 abaixo de 5,0 V (potencia plena do T30D). TODO(hw): ideal diode + buck 12 V + bateria no hw-v0.2.\n"
            "LDO AP2112K-3.3: 600 mA min, Cin = Cout = 1 uF X5R/X7R (Datasheet Diodes, p.2). CC1/CC2 com 5k1 (UFP).\n"
            "EXT_PRESENT: divisor 100k/100k do borne (2,5 V no GPIO20). BAT_ADC sem bateria: pull-down de 100k.")
    sh.add("Connector:USB_C_Receptacle_USB2.0_16P", "Connector.kicad_sym", "J2", "USB-C", 60, 120,
           {"A4": "VBUS", "A9": "VBUS", "B4": "VBUS", "B9": "VBUS",
            "A1": "GND", "A12": "GND", "B1": "GND", "B12": "GND", "SH": "GND",
            "A5": "CC1", "B5": "CC2", "A6": "USB_DP", "B6": "USB_DP", "A7": "USB_DM", "B7": "USB_DM"},
           nc=("A8", "B8"),
           footprint="lora-pump:USB_C_Receptacle_HRO_TYPE-C-31-M-12", mpn="TYPE-C-31-M-12",
           globals_=c.G)
    R(sh, c, "R9", "5k1", 20, 60, "CC1", "GND")
    R(sh, c, "R10", "5k1", 45, 60, "CC2", "GND")
    sh.add("Power_Protection:USBLC6-2SC6", "Power_Protection.kicad_sym", "U4", "USBLC6-2SC6", 130, 60,
           {"1": "USB_DP", "6": "USB_DP", "3": "USB_DM", "4": "USB_DM", "2": "GND", "5": "VBUS"},
           footprint="Package_TO_SOT_SMD:SOT-23-6", mpn="USBLC6-2SC6", globals_=c.G)
    sh.add("Device:D_Schottky", "Device.kicad_sym", "D2", "SS34", 140, 120,
           {"2": "VBUS", "1": "+5V"}, footprint="Diode_SMD:D_SMA", mpn="SS34")
    sh.add("Connector:Screw_Terminal_01x02", "Connector.kicad_sym", "J3", "5V EXT", 60, 200,
           {"1": "5V_IN", "2": "GND"},
           footprint="TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
           mpn="MKDS 1,5/2-5,08")
    sh.add("Device:Polyfuse", "Device.kicad_sym", "F1", "1.5A", 100, 200,
           {"1": "5V_IN", "2": "+5V_EXT"}, footprint="Fuse:Fuse_1206_3216Metric",
           mpn="MF-MSMF150/16X-2")
    sh.add("Device:D_TVS", "Device.kicad_sym", "D4", "SMBJ5.0A", 130, 230,
           {"1": "+5V_EXT", "2": "GND"}, footprint="Diode_SMD:D_SMB", mpn="SMBJ5.0A")
    sh.add("Device:D_Schottky", "Device.kicad_sym", "D3", "SS34", 175, 200,
           {"2": "+5V_EXT", "1": "+5V"}, footprint="Diode_SMD:D_SMA", mpn="SS34")
    R(sh, c, "R11", "100k", 215, 200, "+5V_EXT", "EXT_PRESENT")
    R(sh, c, "R12", "100k", 240, 200, "EXT_PRESENT", "GND")
    C(sh, c, "C6", "10uF", 215, 120, "+5V", "GND")
    sh.add("Regulator_Linear:AP2112K-3.3", "Regulator_Linear.kicad_sym", "U5", "AP2112K-3.3", 290, 120,
           {"1": "+5V", "3": "+5V", "2": "GND", "5": "+3V3"}, nc=("4",),
           footprint="Package_TO_SOT_SMD:SOT-23-5", mpn="AP2112K-3.3TRG1")
    C(sh, c, "C7", "1uF", 265, 80, "+5V", "GND")
    C(sh, c, "C8", "1uF", 330, 80, "+3V3", "GND")
    R(sh, c, "R13", "0R", 215, 60, "+5V", "VCC_RF")
    R(sh, c, "R14", "100k", 360, 120, "BAT_ADC", "GND")
    sh.pwr_flag("+5V", 215, 20)
    sh.pwr_flag("GND", 245, 20)
    sh.pwr_flag("VCC_RF", 275, 20, is_global=True)
    return sh


# ------------------------------------------------------------- IO ----------
def sheet_io(c, uid):
    sh = k.Sheet("Entradas e Saidas", "io.kicad_sch", f"E/S - {c.cfg['titulo']}", REV, c.root, uid)
    if c.role == "emissor":
        sh.text(15, 12, "ENTRADAS - 2 boias (chave seca para GND)", 2.5)
        sh.text(15, 22,
                "Cada entrada: TVS no borne, 1 k em serie e 100 nF ao GND (filtro RC) e pull-up de 10 k para 3V3.\n"
                "Boia baixa -> GPIO1 (FLOAT_LOW). Boia alta -> GPIO2 (FLOAT_HIGH, pull-up R2 na folha MCU).\n"
                "Fechada = GND (tabela de histerese em assets/schematic.md). Sem sensor ultrassonico nesta revisao.")
        sh.add("Connector:Screw_Terminal_01x04", "Connector.kicad_sym", "J4", "BOIAS", 60, 130,
               {"1": "FLOAT_LOW_IN", "2": "GND", "3": "FLOAT_HIGH_IN", "4": "GND"},
               footprint="TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-4-5.08_1x04_P5.08mm_Horizontal",
               mpn="MKDS 1,5/4-5,08")
        for ref_t, ref_r, ref_c, nin, nout, x in (("D5", "R15", "C9", "FLOAT_LOW_IN", "FLOAT_LOW", 130),
                                                  ("D6", "R16", "C10", "FLOAT_HIGH_IN", "FLOAT_HIGH", 210)):
            sh.add("Device:D_TVS", "Device.kicad_sym", ref_t, "PESD5V0S1BL", x, 100,
                   {"1": nin, "2": "GND"}, footprint="Diode_SMD:D_SOD-323", mpn="PESD5V0S1BL")
            R(sh, c, ref_r, "1k", x + 25, 140, nin, nout)
            C(sh, c, ref_c, "100nF", x + 50, 100, nout, "GND")
        R(sh, c, "R17", "10k", 180, 180, "+3V3", "FLOAT_LOW")
    else:
        sh.text(15, 12, "SAIDA - comando de contator externo por contato seco (bornes)", 2.5)
        sh.text(15, 22,
                "A PCB NAO tem tensao de rede: o MOSFET liga a bobina de um rele/contator EXTERNO de 5 V (modulo de rele\n"
                "ou rele interposto). Estado seguro: pull-down de 10 k em PUMP_DRV -> MOSFET cortado em reset/boot/pino\n"
                "flutuante (o GPIO1 do C3 e entrada em alta impedancia no boot). Diodo de roda livre na bobina.\n"
                "Corrente da bobina limitada pelo fusivel de entrada de 1,5 A (dividido com o E220); TODO(hw).")
        R(sh, c, "R15", "10k", 60, 120, "PUMP_DRV", "GND")
        R(sh, c, "R16", "100R", 100, 140, "PUMP_DRV", "GATE")
        sh.add("Transistor_FET:AO3400A", "Transistor_FET.kicad_sym", "Q1", "AO3400A", 160, 130,
               {"1": "GATE", "2": "GND", "3": "COIL_NEG"},
               footprint="Package_TO_SOT_SMD:SOT-23", mpn="AO3400A")
        sh.add("Connector:Screw_Terminal_01x02", "Connector.kicad_sym", "J4", "BOBINA 5V", 240, 130,
               {"1": "+5V", "2": "COIL_NEG"},
               footprint="TerminalBlock_Phoenix:TerminalBlock_Phoenix_MKDS-1,5-2-5.08_1x02_P5.08mm_Horizontal",
               mpn="MKDS 1,5/2-5,08")
        sh.add("Diode:1N4148W", "Diode.kicad_sym", "D5", "1N4148W", 280, 130,
               {"1": "+5V", "2": "COIL_NEG"}, footprint="Diode_SMD:D_SOD-123", mpn="1N4148W")
        R(sh, c, "R17", "1k", 320, 100, "+5V", "LED_COIL")
        sh.add("Device:LED", "Device.kicad_sym", "D6", "SAIDA ON", 355, 130,
               {"1": "COIL_NEG", "2": "LED_COIL"}, footprint="LED_SMD:LED_0603_1608Metric",
               mpn="APT1608SGC")
    return sh


BUILDERS = [("mcu", sheet_mcu, "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f80%d101"),
            ("radio", sheet_radio, "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f80%d102"),
            ("power", sheet_power, "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f80%d103"),
            ("io", sheet_io, "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f80%d104")]

SYM_TABLE = ('(sym_lib_table\n  (version 7)\n  (lib (name "lora-pump")(type "KiCad")'
             '(uri "${KIPRJMOD}/../libs/symbols/lora-pump.kicad_sym")(options "")'
             '(descr "Simbolos proprios do projeto"))\n)\n')
FP_TABLE = ('(fp_lib_table\n  (version 7)\n  (lib (name "lora-pump")(type "KiCad")'
            '(uri "${KIPRJMOD}/../libs/footprints/lora-pump.pretty")(options "")'
            '(descr "Footprints proprios do projeto"))\n)\n')


def build(role):
    c = Ctx(role)
    n = c.cfg["n"]
    out = HW / role
    out.mkdir(exist_ok=True)
    sheets = [fn(c, bid % n) for _, fn, bid in BUILDERS]
    for sh in sheets:
        (out / sh.filename).write_text(sh.render(), encoding="utf-8")
    (out / f"{role}.kicad_sch").write_text(k.render_root(c.root, REV, sheets), encoding="utf-8")
    pro = out / f"{role}.kicad_pro"
    if not pro.exists():
        pro.write_text(json.dumps({"meta": {"filename": f"{role}.kicad_pro", "version": 3}}, indent=2) + "\n")
    (out / "sym-lib-table").write_text(SYM_TABLE, encoding="utf-8")
    (out / "fp-lib-table").write_text(FP_TABLE, encoding="utf-8")
    print("escrito", out)


def main():
    build("emissor")
    build("receptor")


if __name__ == "__main__":
    main()
