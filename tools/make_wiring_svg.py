"""Generate docs/wiring.svg: ESP32-S3 N16R8 DevKitC-1 clone headers in physical
order, wired to the MPU-92.65. Run: python tools/make_wiring_svg.py"""
from pathlib import Path

LEFT = ["3V3", "3V3", "RST", "4", "5", "6", "7", "15", "16", "17", "18", "8", "3", "46",
        "9", "10", "11", "12", "13", "14", "5V", "GND"]
RIGHT = ["GND", "TX", "RX", "1", "2", "42", "41", "40", "39", "38", "37 ✕", "36 ✕", "35 ✕",
         "0 BOOT", "45", "48 RGB", "47", "21", "20", "19", "GND", "GND"]
USED_LEFT = {0, 6, 11, 14, 21}
MPU = ["FSYNC", "NCS", "INT", "ADD", "ECL", "EDA", "SDA", "SCL", "GND", "VCC"]
RED, GND, I2C, INT = "#D85A30", "#5F5E5A", "#1D9E75", "#7F77DD"

def py(i): return 60 + 22 * i          # board pin y
def my(i): return 156 + 28 * i         # MPU pin y

o = ['<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 680 680" font-family="Segoe UI,Roboto,sans-serif">',
     '<style>.t{font-size:14px;fill:#2C2C2A}.b{font-size:14px;font-weight:600;fill:#2C2C2A}.s{font-size:12px;fill:#5F5E5A}</style>',
     '<rect width="680" height="680" fill="#FFFFFF"/>',
     f'<line x1="250" y1="60" x2="250" y2="408" stroke="{RED}" stroke-width="3"/>',
     f'<line x1="220" y1="156" x2="220" y2="522" stroke="{GND}" stroke-width="3"/>',
     '<text class="s" x="250" y="44" text-anchor="middle">3V3 rail</text>',
     '<text class="s" x="220" y="542" text-anchor="middle">GND rail</text>']
wires = [(f"M420 60 H250", RED, ""), (f"M420 522 H220", GND, ""),
         (f"M170 {my(1)} H250", RED, ""), (f"M170 {my(9)} H250", RED, ""),
         (f"M170 {my(0)} H220", GND, ""), (f"M170 {my(3)} H220", GND, ""), (f"M170 {my(8)} H220", GND, ""),
         (f"M170 {my(2)} H395 V{py(6)} H420", INT, ""),
         (f"M170 {my(6)} H395 V{py(11)} H420", I2C, ""),
         (f"M170 {my(7)} H395 V{py(14)} H420", I2C, ' stroke-dasharray="6 3"')]
for d, c, extra in wires:
    o.append(f'<path d="{d}" fill="none" stroke="{c}" stroke-width="1.8"{extra}/>')
for x, y in [(250, 60), (220, 522), (250, my(1)), (250, my(9)), (220, my(0)), (220, my(3)), (220, my(8))]:
    o.append(f'<circle cx="{x}" cy="{y}" r="3.5" fill="#2C2C2A"/>')
# MPU
o.append('<rect x="50" y="110" width="120" height="315" rx="6" fill="#E6F1FB" stroke="#185FA5"/>')
o.append('<text class="b" x="110" y="132" text-anchor="middle">MPU-92.65</text>')
for i, n in enumerate(MPU):
    cls = "s" if n in ("ECL", "EDA") else "b"
    o.append(f'<circle cx="170" cy="{my(i)}" r="3" fill="#185FA5"/>')
    o.append(f'<text class="{cls}" x="158" y="{my(i) + 5}" text-anchor="end">{n}</text>')
    if n in ("ECL", "EDA"):
        o.append(f'<text class="s" x="178" y="{my(i) + 4}">nc</text>')
# board
o.append('<rect x="420" y="30" width="180" height="570" rx="6" fill="#2C2C2A" stroke="#000"/>')
o.append('<text x="510" y="120" text-anchor="middle" font-size="12" fill="#D3D1C7">ESP32-S3</text>')
o.append('<text x="510" y="138" text-anchor="middle" font-size="12" fill="#D3D1C7">N16R8</text>')
for i, n in enumerate(LEFT):
    used = i in USED_LEFT
    o.append(f'<circle cx="420" cy="{py(i)}" r="3" fill="#B4B2A9"/>')
    o.append(f'<text x="432" y="{py(i) + 5}" font-size="{14 if used else 12}" font-weight="{600 if used else 400}" '
             f'fill="{"#FAC775" if used else "#D3D1C7"}">{n}</text>')
for i, n in enumerate(RIGHT):
    o.append(f'<circle cx="600" cy="{py(i)}" r="3" fill="#B4B2A9"/>')
    o.append(f'<text x="588" y="{py(i) + 4}" font-size="12" fill="#D3D1C7" text-anchor="end">{n}</text>')
for x, lbl in [(440, "USB"), (518, "COM")]:
    o.append(f'<rect x="{x}" y="556" width="62" height="30" rx="4" fill="none" stroke="#B4B2A9"/>')
    o.append(f'<text x="{x + 31}" y="575" font-size="12" fill="#D3D1C7" text-anchor="middle">{lbl}</text>')
# legend
o.append('<text class="s" x="420" y="626">✕ = octal PSRAM, never wire</text>')
for x, y, c, lbl, dash in [(50, 622, RED, "3.3 V", ""), (130, 622, GND, "GND", ""), (210, 622, I2C, "SDA", ""),
                           (290, 622, I2C, "SCL", ' stroke-dasharray="6 3"'), (50, 650, INT, "INT", "")]:
    o.append(f'<line x1="{x}" y1="{y}" x2="{x + 22}" y2="{y}" stroke="{c}" stroke-width="2.5"{dash}/>')
    o.append(f'<text class="s" x="{x + 28}" y="{y + 4}">{lbl}</text>')
o.append('<circle cx="140" cy="650" r="3.5" fill="#2C2C2A"/>')
o.append('<text class="s" x="150" y="654">dot = joined · crossing without dot = not joined</text>')
o.append('</svg>')
out = Path(__file__).resolve().parent.parent / "docs" / "wiring.svg"
out.write_text("\n".join(o), encoding="utf-8")
print("wrote", out)
