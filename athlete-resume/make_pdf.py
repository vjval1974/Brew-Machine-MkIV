#!/usr/bin/env python3
"""Pure-Python single-page A4 PDF generator for Max Goold's surfing resume.
No third-party deps: builds the PDF byte stream directly using the standard
Helvetica / Helvetica-Bold base-14 fonts (no embedding needed)."""

import zlib

W, H = 595.28, 841.89          # A4 points
LEFT, RIGHT = 50, 50
USABLE = W - LEFT - RIGHT       # 495.28

# --- approximate Helvetica advance widths (em fractions), bucketed & safe ---
NARROW = set("ijltfI.,:;'|!()[]/\\ ")
WIDE = set("mwMW@%")
def char_w(c, size):
    if c == ' ': return 0.278 * size
    if c in WIDE: return 0.80 * size
    if c in NARROW: return 0.30 * size
    if c.isupper(): return 0.70 * size
    return 0.53 * size
def text_w(s, size):
    return sum(char_w(c, size) for c in s)

def wrap(s, size, maxw):
    words, lines, cur = s.split(), [], ""
    for wd in words:
        trial = wd if not cur else cur + " " + wd
        if text_w(trial, size) <= maxw:
            cur = trial
        else:
            if cur: lines.append(cur)
            cur = wd
    if cur: lines.append(cur)
    return lines or [""]

def esc(s):
    return s.replace("\\", r"\\").replace("(", r"\(").replace(")", r"\)")

# ---- content stream builder ----
ops = []
def color(r, g, b): ops.append(f"{r:.3f} {g:.3f} {b:.3f} rg")
def scolor(r, g, b): ops.append(f"{r:.3f} {g:.3f} {b:.3f} RG")
def rect_fill(x, y, w, h): ops.append(f"{x:.2f} {y:.2f} {w:.2f} {h:.2f} re f")
def line(x1, y1, x2, y2, lw=1):
    ops.append(f"{lw:.2f} w {x1:.2f} {y1:.2f} m {x2:.2f} {y2:.2f} l S")
def text(x, y, s, size, bold=False):
    f = "F2" if bold else "F1"
    ops.append("BT")
    ops.append(f"/{f} {size:.2f} Tf")
    ops.append(f"1 0 0 1 {x:.2f} {y:.2f} Tm")
    ops.append(f"({esc(s)}) Tj")
    ops.append("ET")

NAVY = (0.043, 0.290, 0.435)
BLUE = (0.043, 0.435, 0.690)
DARK = (0.078, 0.125, 0.169)
GREY = (0.353, 0.416, 0.463)
LGREY = (0.42, 0.47, 0.52)

y = H - 56

# Header
color(*NAVY); text(LEFT, y, "MAX GOOLD", 26, bold=True); y -= 18
color(*BLUE); text(LEFT, y, "Junior Competitive Surfer  |  Shortboard", 11, bold=True); y -= 15
color(*GREY)
text(LEFT, y, "Age 13   -   Home break: Palm Beach, Gold Coast, QLD   -   Club: Palm Beach (formerly Currumbin Alley)", 8.5); y -= 11
text(LEFT, y, "Instagram: @max_and_brad_goold", 8.5); y -= 10
scolor(*NAVY); line(LEFT, y, W-RIGHT, y, 1.4); y -= 18

def heading(t):
    global y
    color(*NAVY); text(LEFT, y, t, 11, bold=True); y -= 4
    scolor(0.8, 0.84, 0.87); line(LEFT, y, W-RIGHT, y, 0.6); y -= 13

# Highlights
heading("CAREER HIGHLIGHTS")
hl = [("Back-to-back 2026 Surf Reflections Junior Series champion (U14 Boys)", BLUE),
      ("2024 Australian Junior Online Surf Championships - U12 Grom Boys national champion", DARK),
      ("2023 Woolworths Surfer Groms Comp (Cronulla) - U12 Boys champion", DARK)]
for txt_, col in hl:
    color(*col)
    for ln in wrap("- " + txt_, 9.5, USABLE):
        text(LEFT, y, ln, 9.5, bold=True); y -= 12
y -= 4

# Results table
heading("COMPETITIVE RESULTS")
rows = [
    ("Feb 2026", "Surf Reflections Junior Series - Round 1", "Bonny Hills, NSW", "U14 Boys", "1st", "Surfing Australia National Series (rated 5500)"),
    ("May 2026", "Surf Reflections Junior Series - Round 2", "Hungry Head, Urunga, NSW", "U14 Boys", "1st", "Back-to-back series wins"),
    ("2024", "Australian Junior Online Surf Champs (mophie)", "Online / national", "U12 Grom Boys", "1st", "Winning score 9.27; Junior Series points"),
    ("Dec 2023", "Woolworths Surfer Groms Comp", "North Cronulla, NSW", "U12 Boys", "1st", "Two-wave total 13.67"),
]
# columns: x positions and widths
cols = [("Date", LEFT, 52), ("Event", LEFT+54, 168), ("Location", LEFT+224, 96),
        ("Division", LEFT+322, 62), ("Result", LEFT+386, 40)]
note_x = LEFT
# header row
hdr_h = 14
color(*NAVY); rect_fill(LEFT, y-2, USABLE, hdr_h)
color(1, 1, 1)
for name, cx, cw in cols:
    text(cx+2, y+1.5, name, 8, bold=True)
y -= hdr_h + 2

fs = 8.5
for i, (date, event, loc, div, res, note) in enumerate(rows):
    # wrap event & location to compute row height
    ev_lines = wrap(event, fs, cols[1][2]-4)
    lc_lines = wrap(loc, fs, cols[2][2]-4)
    body_lines = max(len(ev_lines), len(lc_lines), 1)
    note_lines = wrap("Note: " + note, 7.5, USABLE-4)
    row_h = body_lines*10 + len(note_lines)*9 + 5
    if i % 2 == 1:
        color(0.95, 0.967, 0.98); rect_fill(LEFT, y-row_h+8, USABLE, row_h)
    top = y
    color(*DARK); text(cols[0][1]+2, top, date, fs)
    color(*DARK)
    for j, ln in enumerate(ev_lines): text(cols[1][1]+2, top - j*10, ln, fs)
    for j, ln in enumerate(lc_lines): text(cols[2][1]+2, top - j*10, ln, fs)
    text(cols[3][1]+2, top, div, fs)
    color(*BLUE); text(cols[4][1]+2, top, res, fs, bold=True)
    ny = top - body_lines*10
    color(*LGREY)
    for ln in note_lines:
        text(note_x+2, ny, ln, 7.5); ny -= 9
    y = ny - 4
    scolor(0.85, 0.89, 0.92); line(LEFT, y+2, W-RIGHT, y+2, 0.4)
    y -= 2

color(*GREY); text(LEFT, y, "Every result above is a division win. Full source URLs in results.json / results.csv.", 7.5); y -= 16

# Ranking
heading("NATIONAL RANKING")
color(0.93, 0.965, 0.984); rect_fill(LEFT, y-30, USABLE, 38)
scolor(*BLUE); line(LEFT, y-30, LEFT, y+8, 2.2)
color(*DARK)
rk = ("Surfing Australia Junior Series - U14 Boys: current rank & points to be confirmed via the official "
      "Liveheats rankings (liveheats.com/surfingausranking/rankings -> Series: Surfing Australia Junior Series, "
      "Division: U14 Boys). Max is an active points-earner on the National Series after two 2026 round wins.")
ry = y
for ln in wrap(rk, 8.5, USABLE-12):
    text(LEFT+8, ry, ln, 8.5); ry -= 11
y = ry - 8

# Bio
heading("ABOUT MAX")
bio = [
 ("Max Goold is a 13-year-old competitive surfer from Palm Beach on the Gold Coast, Queensland, who cut his "
  "teeth in the beach and point breaks of the southern Gold Coast around Currumbin Alley and Palm Beach. A "
  "consistent finalist and division winner across Surfing Australia's national junior pathway, Max announced "
  "himself nationally by winning the U12 Boys Woolworths Surfer Groms Comp in Cronulla (Dec 2023), then backed "
  "it up with a national title in the U12 Grom Boys division of the 2024 Australian Junior Online Surf Championships."),
 ("In 2026 he stepped up to the U14 Boys ranks and immediately delivered back-to-back wins at Rounds 1 and 2 of "
  "the Surf Reflections Junior Series (Bonny Hills and Hungry Head), confirming his standing among the country's "
  "most promising junior surfers. He combines a competitive heat brain with a progressive, rail-driven approach "
  "and is actively seeking wetsuit and surf-apparel partners to support his 2026 national campaign."),
]
color(*DARK)
for para in bio:
    for ln in wrap(para, 9, USABLE):
        text(LEFT, y, ln, 9); y -= 11
    y -= 4

y -= 2
scolor(0.8, 0.84, 0.87); line(LEFT, y, W-RIGHT, y, 0.4); y -= 11
color(*LGREY)
disc = ("Athlete identity note: this Max Goold is the Palm Beach / Gold Coast junior surfer "
        "(@max_and_brad_goold) - a different person to the similarly-named Melbourne junior cyclist. "
        "All results shown are surfing results.")
for ln in wrap(disc, 7.5, USABLE):
    text(LEFT, y, ln, 7.5); y -= 9

import sys
print(f"DEBUG final y = {y:.1f} (bottom margin ~40; must stay positive)", file=sys.stderr)

# ---- assemble PDF ----
stream = "\n".join(ops).encode("latin-1")
comp = zlib.compress(stream)

objs = []
objs.append(b"<< /Type /Catalog /Pages 2 0 R >>")
objs.append(b"<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
objs.append(f"<< /Type /Page /Parent 2 0 R /MediaBox [0 0 {W:.2f} {H:.2f}] "
            f"/Resources << /Font << /F1 5 0 R /F2 6 0 R >> >> /Contents 4 0 R >>".encode())
objs.append(b"<< /Length %d /Filter /FlateDecode >>\nstream\n" % len(comp) + comp + b"\nendstream")
objs.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Encoding /WinAnsiEncoding >>")
objs.append(b"<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica-Bold /Encoding /WinAnsiEncoding >>")

out = b"%PDF-1.4\n%\xe2\xe3\xcf\xd3\n"
offsets = []
for i, o in enumerate(objs, 1):
    offsets.append(len(out))
    out += f"{i} 0 obj\n".encode() + o + b"\nendobj\n"
xref_pos = len(out)
out += b"xref\n0 %d\n" % (len(objs) + 1)
out += b"0000000000 65535 f \n"
for off in offsets:
    out += ("%010d 00000 n \n" % off).encode()
out += b"trailer\n<< /Size %d /Root 1 0 R >>\nstartxref\n%d\n%%%%EOF\n" % (len(objs) + 1, xref_pos)

with open("max_goold_resume.pdf", "wb") as f:
    f.write(out)
print("wrote max_goold_resume.pdf  (%d bytes)" % len(out))
