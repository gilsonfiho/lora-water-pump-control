"""Gera os esquematicos do projeto (folha raiz + subfolhas).

Uso:  python hardware/tools/build_sch.py
Cada folha e uma funcao; a conectividade e por nome de net (ver kicadgen.py).
Valores, footprints e MPN ficam aqui; fontes de cada decisao em hardware/docs/decisoes.md.
"""
from pathlib import Path

import kicadgen as k

HW = Path(__file__).resolve().parent.parent
LIB = str(HW / "libs" / "symbols" / "lora-pump.kicad_sym")
REV = "0.1"
ROOT = "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f801001"

FP_R = "Resistor_SMD:R_0603_1608Metric"
FP_C = "Capacitor_SMD:C_0603_1608Metric"
MPN_R = {"10k": "RC0603FR-0710KL", "4k7": "RC0603FR-074K7L", "1k": "RC0603FR-071KL",
         "5k1": "RC0603FR-075K1L"}
MPN_C = {"100nF": "CL10B104KB8NNNC", "10uF": "CL10A106KP8NNNC", "1uF": "CL10A105KB8NNNC"}

# Nets compartilhadas entre folhas (rotulos globais).
GLOBALS = {
    "E220_M0", "E220_M1", "E220_RXD", "E220_TXD", "E220_AUX", "VCC_RF",
    "BAT_ADC", "FLOAT_LOW_PUMP_DRV", "FLOAT_HIGH", "ROLE_STRAP",
    "I2C_SDA", "I2C_SCL", "USB_DM", "USB_DP", "EXT_PRESENT", "LED_STATUS_DRV",
}


def R(sh, ref, val, x, y, n1, n2, dnp=False, variant=""):
    sh.add("Device:R", "Device.kicad_sym", ref, val, x, y, {"1": n1, "2": n2},
           footprint=FP_R, mpn=MPN_R[val], dnp=dnp, globals_=GLOBALS)


def C(sh, ref, val, x, y, n1, n2):
    sh.add("Device:C", "Device.kicad_sym", ref, val, x, y, {"1": n1, "2": n2},
           footprint=FP_C, mpn=MPN_C[val], globals_=GLOBALS)


def sheet_mcu(uid):
    sh = k.Sheet("MCU", "mcu.kicad_sch", "MCU: ESP32-C3-MINI-1", REV, ROOT, uid)
    sh.text(15, 12, "MCU - ESP32-C3-MINI-1-H4X (Datasheet v2.2)", 2.5)
    sh.text(15, 20, "Alimentacao 3V3 (3,0-3,6 V, Tabela 1 / p.1). EN com RC de 10 k + 1 uF (p.34).\n"
                    "Strapping (Tabela 4-3, p.13): GPIO9=1 no boot (pull-up 4k7 + botao BOOT);\n"
                    "GPIO8=1 para download (pull-up 4k7, tambem SDA); GPIO2 com pull-up (recomendado).\n"
                    "GPIO18/19 = USB Serial/JTAG. GPIO20/21 = UART0 (console): ver decisoes.md.")

    sh.add("lora-pump:ESP32-C3-MINI-1", LIB, "U1", "ESP32-C3-MINI-1-H4X", 210, 150, {
        "3": "+3V3", "1": "GND",
        "8": "MCU_EN", "12": "BAT_ADC", "13": "FLOAT_LOW_PUMP_DRV", "5": "FLOAT_HIGH",
        "6": "ROLE_STRAP", "18": "E220_TXD", "19": "E220_RXD", "20": "E220_M0",
        "21": "E220_M1", "22": "I2C_SDA", "23": "I2C_SCL", "16": "E220_AUX",
        "26": "USB_DM", "27": "USB_DP", "30": "EXT_PRESENT", "31": "LED_STATUS_DRV",
    }, footprint="lora-pump:ESP32-C3-MINI-1", mpn="ESP32-C3-MINI-1-H4X",
        globals_=GLOBALS, nc=())

    # Desacoplamento do 3V3 junto ao modulo.
    C(sh, "C1", "10uF", 40, 80, "+3V3", "GND")
    C(sh, "C2", "100nF", 65, 80, "+3V3", "GND")
    # EN: RC de reset + botao RESET.
    R(sh, "R1", "10k", 100, 80, "+3V3", "MCU_EN")
    C(sh, "C3", "1uF", 125, 80, "MCU_EN", "GND")
    sh.add("Switch:SW_Push", "Switch.kicad_sym", "SW1", "RESET", 160, 80,
           {"1": "MCU_EN", "2": "GND"}, footprint="Button_Switch_SMD:SW_SPST_TL3342",
           mpn="TL3342F160QG")
    # GPIO2 (strap): pull-up; tambem polariza a boia alta no reservatorio.
    R(sh, "R2", "10k", 200, 80, "+3V3", "FLOAT_HIGH")
    # I2C: pull-ups; GPIO8/GPIO9 sao straps e precisam ficar em 1 no boot.
    R(sh, "R3", "4k7", 225, 80, "+3V3", "I2C_SDA")
    R(sh, "R4", "4k7", 250, 80, "+3V3", "I2C_SCL")
    sh.add("Switch:SW_Push", "Switch.kicad_sym", "SW2", "BOOT", 285, 80,
           {"1": "I2C_SCL", "2": "GND"}, footprint="Button_Switch_SMD:SW_SPST_TL3342",
           mpn="TL3342F160QG", globals_=GLOBALS)
    # Strap de papel (GPIO3): aberto = reservatorio, GND = bomba.
    R(sh, "R5", "10k", 320, 80, "+3V3", "ROLE_STRAP")
    sh.add("Jumper:SolderJumper_2_Open", "Jumper.kicad_sym", "JP1", "PAPEL: fechar=BOMBA", 355, 80,
           {"1": "ROLE_STRAP", "2": "GND"},
           footprint="Jumper:SolderJumper-2_P1.3mm_Open_RoundedPad1.0x1.5mm", globals_=GLOBALS)
    # LED de estado (GPIO21 = TXD0: pisca no boot, inofensivo).
    R(sh, "R6", "1k", 320, 200, "LED_STATUS_DRV", "LED_A")
    sh.add("Device:LED", "Device.kicad_sym", "D1", "STATUS", 355, 200,
           {"1": "GND", "2": "LED_A"}, footprint="LED_SMD:LED_0603_1608Metric",
           mpn="APT1608SGC")
    # Header de I2C livre (expansao / INA219-226).
    sh.add("Connector_Generic:Conn_01x04", "Connector_Generic.kicad_sym", "J1", "I2C",
           355, 150, {"1": "+3V3", "2": "I2C_SDA", "3": "I2C_SCL", "4": "GND"},
           footprint="Connector_PinHeader_2.54mm:PinHeader_1x04_P2.54mm_Vertical",
           mpn="PinHeader_1x04_P2.54mm", globals_=GLOBALS)

    # TEMP: fontes das nets de alimentacao, ate a folha de alimentacao existir.
    sh.text(15, 250, "TEMP: PWR_FLAG de +3V3 e GND saem quando a folha de alimentacao for criada.")
    sh.pwr_flag("+3V3", 40, 260)
    sh.pwr_flag("GND", 80, 260)
    return sh


def sheet_radio(uid):
    sh = k.Sheet("Radio E220", "radio.kicad_sch", "Radio: E220-900T30D / T22D", REV, ROOT, uid)
    sh.text(15, 12, "RADIO - EBYTE E220-900T30D (U2) ou E220-900T22D (U3): popular UM so", 2.5)
    sh.text(15, 22,
            "!! O E220 TRANSMITE LOGO NO BOOT: NUNCA ENERGIZAR SEM ANTENA NO SMA DO MODULO !!\n"
            "A antena e o SMA-K do proprio modulo (T30D/T22D Manual, Tabela 2-2, p.5): nao ha SMA na PCB.\n"
            "Pinos (Tabela 3-1, p.6): 1 M0, 2 M1, 3 RXD, 4 TXD, 5 AUX, 6 VCC, 7 GND; 8-11 furos de fixacao.\n"
            "UART/M0/M1/AUX em 3,3 V. VCC em 5 V (>= 5,0 V para potencia plena): T30D 620 mA de pico;\n"
            "T22D 110 mA. Entrada maxima do RX: +10 dBm (nos a >= 3 m na bancada).\n"
            "Pull-ups de 4-10 k em TXD/AUX so para MCU de 5 V (Tabela 4-1, p.7): DNP aqui (MCU de 3,3 V).")

    pins = {"1": "E220_M0", "2": "E220_M1", "3": "E220_RXD", "4": "E220_TXD",
            "5": "E220_AUX", "6": "VCC_RF", "7": "GND"}
    sh.add("lora-pump:E220-900TxxD", LIB, "U2", "E220-900T30D", 110, 130, pins,
           footprint="lora-pump:E220-900T30D", mpn="E220-900T30D", globals_=GLOBALS,
           extra={"Variante": "T30D"})
    sh.add("lora-pump:E220-900TxxD", LIB, "U3", "E220-900T22D", 200, 130, pins,
           footprint="lora-pump:E220-900T22D", mpn="E220-900T22D", globals_=GLOBALS,
           extra={"Variante": "T22D"})

    # Desacoplamento junto ao VCC do modulo (README: 100 uF + 100 nF; bancada usa 1000 uF).
    sh.add("Device:C_Polarized", "Device.kicad_sym", "C4", "470uF/10V", 290, 130,
           {"1": "VCC_RF", "2": "GND"}, footprint="Capacitor_SMD:CP_Elec_8x10.5",
           mpn="EEE-FK1A471P", globals_=GLOBALS)
    sh.text(275, 150, "TODO(hw): 470 uF e palpite;\nbancada usa 1000 uF", 1.0)
    C(sh, "C5", "100nF", 320, 130, "VCC_RF", "GND")
    # Pull-ups opcionais (DNP): so para MCU de 5 V.
    R(sh, "R7", "10k", 350, 130, "+3V3", "E220_TXD", dnp=True)
    R(sh, "R8", "10k", 375, 130, "+3V3", "E220_AUX", dnp=True)

    # TEMP: fonte de VCC_RF e +3V3 ate a folha de alimentacao existir.
    sh.text(15, 250, "TEMP: PWR_FLAG de VCC_RF sai quando a folha de alimentacao for criada.")
    sh.pwr_flag("VCC_RF", 40, 260, is_global=True)
    return sh


def main():
    ids = {"mcu": "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f801101",
           "radio": "7f0c2b1e-5a3d-4c8e-9b1a-2d4e6f801102"}
    sheets = [sheet_mcu(ids["mcu"]), sheet_radio(ids["radio"])]
    for sh in sheets:
        (HW / sh.filename).write_text(sh.render(), encoding="utf-8")
        print("escrito", sh.filename)
    (HW / "lora-pump.kicad_sch").write_text(k.render_root(ROOT, REV, sheets), encoding="utf-8")
    print("escrito lora-pump.kicad_sch")


if __name__ == "__main__":
    main()
