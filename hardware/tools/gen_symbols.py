"""Gera hardware/libs/symbols/lora-pump.kicad_sym.

Fontes:
  * ESP32-C3-MINI-1 Datasheet v2.2, Tabela 3-1 "Pin Definitions" (p. 11).
  * E220-900T30D User Manual v1.0 e E220-900T22D v1.2, Tabela 3-1 (p. 6).
"""
from pathlib import Path

OUT = Path(__file__).resolve().parent.parent / "libs" / "symbols" / "lora-pump.kicad_sym"
FX = '(effects (font (size 1.27 1.27)))'
FXH = '(effects (font (size 1.27 1.27)) hide)'


def pin(kind, x, y, ang, name, num, hide=False):
    h = " hide" if hide else ""
    return (
        f'\t\t\t(pin {kind} line (at {x} {y} {ang}) (length 5.08){h}\n'
        f'\t\t\t\t(name "{name}" {FX})\n\t\t\t\t(number "{num}" {FX})\n\t\t\t)'
    )


def symbol(name, ref, value, footprint, datasheet, desc, rect, pins):
    x0, y0, x1, y1 = rect
    return (
        f'\t(symbol "{name}"\n\t\t(exclude_from_sim no) (in_bom yes) (on_board yes)\n'
        f'\t\t(property "Reference" "{ref}" (at 0 {y0 + 2.54} 0) {FX})\n'
        f'\t\t(property "Value" "{value}" (at 0 {y1 - 2.54} 0) {FX})\n'
        f'\t\t(property "Footprint" "{footprint}" (at 0 0 0) {FXH})\n'
        f'\t\t(property "Datasheet" "{datasheet}" (at 0 0 0) {FXH})\n'
        f'\t\t(property "Description" "{desc}" (at 0 0 0) {FXH})\n'
        f'\t\t(symbol "{name}_0_1"\n'
        f'\t\t\t(rectangle (start {x0} {y0}) (end {x1} {y1}) (stroke (width 0.254) (type default)) (fill (type background)))\n\t\t)\n'
        f'\t\t(symbol "{name}_1_1"\n' + "\n".join(pins) + "\n\t\t)\n\t)"
    )


def esp32():
    left = [("EN", 8), ("IO0", 12), ("IO1", 13), ("IO2", 5), ("IO3", 6), ("IO4", 18),
            ("IO5", 19), ("IO6", 20), ("IO7", 21), ("IO8", 22), ("IO9", 23), ("IO10", 16)]
    right = [("IO18", 26), ("IO19", 27), ("IO20/RXD0", 30), ("IO21/TXD0", 31)]
    pins = []
    for i, (n, num) in enumerate(left):
        kind = "input" if n == "EN" else "bidirectional"
        pins.append(pin(kind, -12.7, round(15.24 - 2.54 * i, 2), 0, n, num))
    for i, (n, num) in enumerate(right):
        pins.append(pin("bidirectional", 12.7, round(15.24 - 2.54 * i, 2), 180, n, num))
    pins.append(pin("power_in", 0, 25.4, 270, "3V3", 3))
    pins.append(pin("power_in", 0, -25.4, 90, "GND", 1))
    # GND restantes e EPAD: ocultos; ligam-se por nome a net GND.
    for num in [2, 11, 14] + list(range(36, 54)):
        pins.append(pin("power_in", 0, -25.4, 90, "GND", num, hide=True))
    # NC (Tabela 3-1): ocultos.
    for num in [4, 7, 9, 10, 15, 17, 24, 25, 28, 29, 32, 33, 34, 35]:
        pins.append(pin("no_connect", 12.7, -20.32, 180, "NC", num, hide=True))
    return symbol(
        "ESP32-C3-MINI-1", "U", "ESP32-C3-MINI-1-H4X", "lora-pump:ESP32-C3-MINI-1",
        "https://www.espressif.com/sites/default/files/documentation/esp32-c3-mini-1_datasheet_en.pdf",
        "Modulo Wi-Fi/BLE ESP32-C3, antena de PCB, 53 pinos",
        (-7.62, 20.32, 7.62, -20.32), pins)


def e220():
    names = [("M0", 1, "input"), ("M1", 2, "input"), ("RXD", 3, "input"),
             ("TXD", 4, "tri_state"), ("AUX", 5, "tri_state")]
    pins = []
    for i, (n, num, kind) in enumerate(names):
        pins.append(pin(kind, -12.7, round(5.08 - 2.54 * i, 2), 0, n, num))
    pins.append(pin("power_in", 0, 17.78, 270, "VCC", 6))
    pins.append(pin("power_in", 0, -17.78, 90, "GND", 7))
    return symbol(
        "E220-900TxxD", "U", "E220-900T30D", "lora-pump:E220-900T30D",
        "~", "Modulo LoRa EBYTE E220-900T22D/T30D (LLCC68), UART TTL 3,3 V",
        (-7.62, 12.7, 7.62, -12.7), pins)


def main():
    text = (
        '(kicad_symbol_lib (version 20241209) (generator "gen_symbols.py") (generator_version "1.0")\n'
        + esp32() + "\n" + e220() + "\n)\n"
    )
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(text, encoding="utf-8")
    print("escrito", OUT)


if __name__ == "__main__":
    main()
