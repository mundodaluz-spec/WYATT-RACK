# Gera as telas de referência do WMusic Rack (HTML -> PNG) para o Heitor modernizar fora.
import math, os, random, json
import numpy as np

OUT = '/tmp/claude-0/mock/html'
os.makedirs(OUT, exist_ok=True)
RED = '#d7262b'; CREAM = '#efe6d6'; DIM = '#8d8a85'; GRID = '#1c1c1f'

CSS = r"""
@font-face { font-family: Bc; src: url(../BarlowCondensed-SemiBold.ttf); font-weight: 600; }
@font-face { font-family: Bc; src: url(../BarlowCondensed-Bold.ttf); font-weight: 700; }
@font-face { font-family: Bc; src: url(../BarlowCondensed-Medium.ttf); font-weight: 500; }
* { box-sizing: border-box; margin: 0; padding: 0; }
html, body { width: 1280px; height: 820px; overflow: hidden; background: #000; }
body { font-family: Bc, sans-serif; color: #efe6d6; position: relative;
  background: #0d0d0e url(../metal.jpg) center/cover; }
.abs { position: absolute; }
.panel { position: absolute; border-radius: 10px;
  background: linear-gradient(180deg, rgba(255,255,255,.035), rgba(0,0,0,.25)), url(../metal.jpg) center/1600px 1000px;
  border: 1px solid #2b2b2f;
  box-shadow: 0 0 0 1px #000, inset 0 1px 0 rgba(255,255,255,.08), 0 0 18px rgba(215,38,43,.22), 0 6px 18px rgba(0,0,0,.7); }
.panel.dark { background: linear-gradient(180deg, rgba(0,0,0,.35), rgba(0,0,0,.55)), url(../metal.jpg) center/1600px 1000px; }
.screw { position: absolute; width: 11px; height: 11px; border-radius: 50%;
  background: radial-gradient(circle at 35% 30%, #d8d8d8, #6d6d70 45%, #232325 75%); box-shadow: 0 1px 1px #000; }
.screw::after { content: ''; position: absolute; left: 2px; right: 2px; top: 5px; height: 1.4px; background: #1a1a1b; transform: rotate(35deg); }
.title { position: absolute; font-weight: 700; font-size: 21px; letter-spacing: .4px; color: #efe6d6; }
.title::after { content: ''; position: absolute; left: -4px; bottom: -5px; width: 120%; height: 3px; border-radius: 2px;
  background: linear-gradient(90deg, rgba(215,38,43,0), #d7262b 12%, #d7262b 70%, rgba(215,38,43,0)); }
.lbl { position: absolute; font-weight: 600; font-size: 14px; color: #efe6d6; text-align: center; letter-spacing: .3px; white-space: nowrap; }
.val { position: absolute; font-weight: 500; font-size: 15px; color: #efe6d6; text-align: center; white-space: nowrap; }
.small { font-size: 11.5px; color: #a9a49c; letter-spacing: 1.2px; }
.key { position: absolute; display: flex; align-items: center; justify-content: center; font-weight: 600; font-size: 14px; letter-spacing: .4px;
  border-radius: 6px; background: url(../btn_dark.png) center/100% 100%; color: #efe6d6; }
.key.red { background-image: url(../btn_red.png); color: #fff; }
.key.cream { background-image: url(../btn_cream.png); color: #151515; }
.display { position: absolute; border-radius: 6px; background: #050506;
  box-shadow: inset 0 2px 6px rgba(0,0,0,.9), inset 0 0 0 1px #000, 0 1px 0 rgba(255,255,255,.07); }
.knob { position: absolute; }
.knob img { position: absolute; left: 0; top: 0; width: 100%; height: 100%; }
.knob svg { position: absolute; left: 0; top: 0; overflow: visible; }
.led { position: absolute; border-radius: 50%; }
.seg { position: absolute; display: flex; gap: 2px; }
.seg i { display: block; border-radius: 1.5px; }
"""

def screw(x, y): return f'<div class="screw" style="left:{x}px;top:{y}px"></div>'

def panel(x, y, w, h, cls='panel', screws=True):
    s = f'<div class="{cls}" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px"></div>'
    if screws:
        for sx, sy in [(x+8, y+8), (x+w-19, y+8), (x+8, y+h-19), (x+w-19, y+h-19)]: s += screw(sx, sy)
    return s

def title(x, y, t, size=21): return f'<div class="title" style="left:{x}px;top:{y}px;font-size:{size}px">{t}</div>'
def lbl(x, y, w, t, size=14, color=CREAM, align='center', weight=600):
    return f'<div class="lbl" style="left:{x}px;top:{y}px;width:{w}px;font-size:{size}px;color:{color};text-align:{align};font-weight:{weight}">{t}</div>'
def val(x, y, w, t, size=15, color=CREAM, align='center'):
    return f'<div class="val" style="left:{x}px;top:{y}px;width:{w}px;font-size:{size}px;color:{color};text-align:{align}">{t}</div>'
def key(x, y, w, h, t, kind='', size=14):
    return f'<div class="key {kind}" style="left:{x}px;top:{y}px;width:{w}px;height:{h}px;font-size:{size}px">{t}</div>'

def knob(cx, cy, d, pos, arc=True):
    """knob centrado em (cx,cy), diâmetro d, posição 0..1"""
    a0, a1 = -135, 135
    ang = a0 + pos * (a1 - a0)
    r = d / 2 + d * 0.11
    def pt(a, rr): return (d/2 + rr * math.sin(math.radians(a)), d/2 - rr * math.cos(math.radians(a)))
    def arcpath(a, b, rr):
        x0, y0 = pt(a, rr); x1, y1 = pt(b, rr); large = 1 if b - a > 180 else 0
        return f'M{x0:.1f},{y0:.1f} A{rr:.1f},{rr:.1f} 0 {large} 1 {x1:.1f},{y1:.1f}'
    sw = max(2.2, d * 0.05)
    svg = f'<svg width="{d}" height="{d}" viewBox="0 0 {d} {d}">'
    svg += f'<path d="{arcpath(a0, a1, r)}" stroke="#e9dfcd" stroke-opacity=".85" stroke-width="{sw:.1f}" fill="none" stroke-linecap="round"/>'
    if arc and pos > 0.01:
        svg += f'<path d="{arcpath(a0, ang, r)}" stroke="{RED}" stroke-width="{sw*1.15:.1f}" fill="none" stroke-linecap="round" style="filter:drop-shadow(0 0 3px rgba(215,38,43,.8))"/>'
    px0, py0 = pt(ang, d * 0.10); px1, py1 = pt(ang, d * 0.36)
    svg += f'<line x1="{px0:.1f}" y1="{py0:.1f}" x2="{px1:.1f}" y2="{py1:.1f}" stroke="{RED}" stroke-width="{max(2,d*0.045):.1f}" stroke-linecap="round"/>'
    svg += '</svg>'
    return f'<div class="knob" style="left:{cx-d/2}px;top:{cy-d/2}px;width:{d}px;height:{d}px"><img src="../knob.png">{svg}</div>'

def segs(x, y, n, w, h, lit, color_fn):
    s = f'<div class="seg" style="left:{x}px;top:{y}px">'
    for i in range(n):
        c, on = color_fn(i, i < lit)
        s += f'<i style="width:{w}px;height:{h}px;background:{c};opacity:{1 if on else .22};box-shadow:{"0 0 4px "+c if on else "none"}"></i>'
    return s + '</div>'

def led(x, y, d, on=True, color=RED):
    if on: return f'<div class="led" style="left:{x}px;top:{y}px;width:{d}px;height:{d}px;background:radial-gradient(circle at 40% 35%,#ffb3a8,{color} 55%,#5a0d0e);box-shadow:0 0 8px {color}"></div>'
    return f'<div class="led" style="left:{x}px;top:{y}px;width:{d}px;height:{d}px;background:radial-gradient(circle at 40% 35%,#555,#222 60%,#0c0c0c)"></div>'

# ------------------------------------------------------------------ estrutura comum
def left_column(preset='Gospel Church Vocal', mode=1):
    s = panel(14, 14, 262, 792)
    s += '<img src="../logo.png" class="abs" style="left:52px;top:34px;width:186px">'
    s += lbl(30, 352, 230, 'RACK', 46, CREAM, 'center', 700)
    s += f'<div class="abs small" style="left:30px;top:404px;width:230px;text-align:center">AI EFFECTS RACK</div>'
    s += title(36, 446, 'AI ENGINE', 18)
    for i, t in enumerate(['OFFLINE', 'HYBRID', 'ONLINE']):
        s += key(34 + i * 76, 482, 72, 34, t, 'red' if i == mode else '', 13)
    s += led(36, 532, 9, True, '#3fd17a') + f'<div class="abs" style="left:52px;top:528px;font-size:13px;color:#b9b3a8">Advanced AI ready</div>'
    s += key(178, 525, 80, 26, 'AI SETUP', '', 12)
    s += title(36, 576, 'PRESET', 18)
    s += f'<div class="display" style="left:34px;top:610px;width:224px;height:40px"></div>'
    s += val(46, 620, 180, preset, 16, CREAM, 'left') + f'<div class="abs" style="left:232px;top:622px;color:{RED};font-size:14px">&#9660;</div>'
    s += key(34, 660, 108, 32, 'SAVE') + key(150, 660, 108, 32, 'LIBRARY')
    s += key(34, 702, 108, 32, '&#8630; UNDO') + key(150, 702, 108, 32, 'REDO &#8631;')
    s += f'<div class="abs small" style="left:34px;top:772px">WYATT SOUNDS</div><div class="abs small" style="left:200px;top:772px">v2.0.0</div>'
    return s

def prompt_panel(text='warm vocal, up front, with church reverb', reply=None, source='OFFLINE BRAIN'):
    s = panel(290, 14, 976, 168)
    s += title(312, 26, 'DESCRIBE YOUR SOUND')
    s += f'<div class="display" style="left:308px;top:62px;width:800px;height:48px"></div>'
    s += f'<div class="abs" style="left:324px;top:72px;font-size:22px;font-weight:600;color:{CREAM}">{text}<span style="color:{RED}">|</span></div>'
    s += key(1120, 62, 130, 48, 'CREATE', 'red', 18)
    chips = ['gospel church vocal', 'more brightness', 'less reverb', 'telephone', '80s dream', 'surprise me', 'reset']
    x = 308
    for c in chips:
        w = 18 + len(c) * 7.2
        s += key(x, 120, w, 24, c, '', 12.5); x += w + 7
    if reply is None:
        reply = 'Got it: gospel · warm · up front · church reverb. Built: EQ → De-Esser → Compressor → Saturation → Reverb.'
    s += f'<div class="abs" style="left:310px;top:152px;font-size:12px;letter-spacing:1.2px;color:{RED};font-weight:700">{source}</div>'
    s += f'<div class="abs" style="left:420px;top:150px;font-size:14px;color:#cfc8bb;width:830px;white-space:nowrap;overflow:hidden">{reply}</div>'
    return s

def bottom_bar():
    s = panel(290, 742, 976, 64)
    for i, (t, lit) in enumerate([('IN', 26), ('OUT', 29)]):
        y = 756 + i * 22
        s += lbl(306, y - 2, 30, t, 13, CREAM, 'left')
        s += segs(340, y, 36, 9, 12, lit, lambda k, on: (('#efe6d6' if k < 28 else RED), on))
    s += knob(830, 774, 40, 0.5) + lbl(860, 758, 70, 'INPUT', 13, CREAM, 'left') + val(860, 776, 70, '0.0 dB', 14, CREAM, 'left')
    s += knob(960, 774, 40, 0.56) + lbl(990, 758, 70, 'OUTPUT', 13, CREAM, 'left') + val(990, 776, 70, '+1.0 dB', 14, CREAM, 'left')
    s += key(1150, 757, 100, 34, 'BYPASS')
    return s

def page(body, name):
    html = f'<!doctype html><html><head><meta charset="utf-8"><style>{CSS}</style></head><body>{body}</body></html>'
    open(f'{OUT}/{name}.html', 'w').write(html)

# ------------------------------------------------------------------ dados dos blocos
FAM = {'EQ': '#e8dcc0', 'COMPRESSOR': '#ffb238', 'SATURATION': RED, 'REVERB': '#6fb6ff', 'DELAY': '#6fb6ff', 'CHORUS': '#b98cff',
       'PHASER / FLANGER': '#b98cff', 'FILTER': '#e8dcc0', 'DE-ESSER': '#ffb238', 'GATE': '#ffb238', 'STEREO': '#6fb6ff',
       'TREMOLO': '#b98cff', 'LO-FI': RED, 'LIMITER': '#ffb238'}

# (nome, tagline, [(param, valor, pos 0..1)], mix)
BLOCKS = {
 'eq':      ('EQ', 'Low cut, low shelf, mid bell, high shelf and high cut',
             [('LOW CUT', '90 Hz', .38), ('LOW', '+2.0 dB', .58), ('MID FREQ', '2.8 kHz', .66), ('MID', '+3.0 dB', .62), ('HIGH', '+2.5 dB', .6), ('HIGH CUT', 'OFF', 1)], '100 %'),
 'comp':    ('COMPRESSOR', 'Dynamics control with soft knee',
             [('THRESHOLD', '-22.0 dB', .56), ('RATIO', '4.0:1', .46), ('ATTACK', '10 ms', .5), ('RELEASE', '120 ms', .41), ('MAKE-UP', '+4.0 dB', .17), ('KNEE', '6.0 dB', .5)], '100 %'),
 'sat':     ('SATURATION', 'Tube, tape or transistor harmonics',
             [('DRIVE', '+9.0 dB', .25), ('TYPE', 'Tube', 0), ('TONE', '+1.0 dB', .58), ('OUTPUT', '-2.0 dB', .42)], '100 %'),
 'reverb':  ('REVERB', 'Room, church, plate or cathedral',
             [('TYPE', 'Church', .33), ('SIZE', '80 %', .8), ('DECAY', '3.2 s', .62), ('PRE-DELAY', '35 ms', .18), ('HIGHS', '45 %', .45), ('WIDTH', '100 %', 1)], '22 %'),
 'delay':   ('DELAY', 'Tempo-synced echo with ping-pong',
             [('TIME', '375 ms', .6), ('SYNC', '1/8 D', .57), ('FEEDBACK', '38 %', .4), ('TONE', '5.0 kHz', .5), ('PING-PONG', 'ON', 1), ('MODULATION', '15 %', .15)], '18 %'),
 'chorus':  ('CHORUS', 'Multi-voice chorus and widening',
             [('RATE', '0.8 Hz', .6), ('DEPTH', '45 %', .45), ('VOICES', '3', 1), ('WIDTH', '100 %', 1), ('DELAY', '12 ms', .28), ('FEEDBACK', '0 %', 0)], '40 %'),
 'phaser':  ('PHASER / FLANGER', 'Sweeping notches or comb flange',
             [('TYPE', 'Phaser', 0), ('RATE', '0.4 Hz', .45), ('DEPTH', '60 %', .6), ('FEEDBACK', '+30 %', .67), ('CENTER', '1.0 kHz', .5), ('STEREO', '50 %', .5)], '100 %'),
 'filter':  ('FILTER', 'Resonant filter with LFO and envelope',
             [('TYPE', 'Low-pass', 0), ('CUTOFF', '1.8 kHz', .66), ('RESONANCE', '55 %', .55), ('LFO RATE', '1.0 Hz', .5), ('LFO DEPTH', '35 %', .35), ('ENVELOPE', '+20 %', .6)], '100 %'),
 'deesser': ('DE-ESSER', 'Tames sibilance and harsh highs',
             [('FREQUENCY', '6.5 kHz', .55), ('THRESHOLD', '-32.0 dB', .47), ('MAX REDUCTION', '10.0 dB', .42), ('MODE', 'Split', 0)], '100 %'),
 'gate':    ('GATE', 'Closes the sound in the silences',
             [('THRESHOLD', '-45.0 dB', .44), ('ATTACK', '1.0 ms', .3), ('HOLD', '50 ms', .1), ('RELEASE', '150 ms', .45), ('RANGE', '40.0 dB', .5)], '100 %'),
 'stereo':  ('STEREO', 'Width, mono bass, pan and doubler',
             [('WIDTH', '140 %', .7), ('MONO BASS', '120 Hz', .6), ('PAN', 'C', .5), ('DOUBLER', '8.0 ms', .27)], '100 %'),
 'tremolo': ('TREMOLO', 'Pulsing volume or auto-pan',
             [('RATE', '4.0 Hz', .6), ('DEPTH', '50 %', .5), ('SHAPE', 'Sine', 0), ('AUTO-PAN', '60 %', .6), ('SYNC', '1/8', .6)], '100 %'),
 'lofi':    ('LO-FI', 'Bits, sample rate, noise, band and wow',
             [('BITS', '8 bit', .27), ('SAMPLE RATE', '11.0 kHz', .53), ('NOISE', '20 %', .2), ('LOW CUT', '200 Hz', .6), ('HIGH CUT', '5.0 kHz', .5), ('WOW', '35 %', .35)], '100 %'),
 'limiter': ('LIMITER', 'Catches peaks and raises loudness',
             [('CEILING', '-1.0 dB', .92), ('GAIN', '+6.0 dB', .25), ('RELEASE', '80 ms', .63)], '100 %'),
}

# ------------------------------------------------------------------ tela principal
CHAIN = ['eq', 'deesser', 'comp', 'sat', 'delay', 'reverb', 'stereo', 'limiter']
CHAIN_LABEL = {'eq': 'EQ', 'deesser': 'DE-ESSER', 'comp': 'COMPRESSOR', 'sat': 'SATURATION', 'delay': 'DELAY', 'reverb': 'REVERB', 'stereo': 'STEREO', 'limiter': 'LIMITER'}

def strip(i, kind, y, empty=False):
    x0, w, h = 306, 944, 60
    s = f'<div class="abs" style="left:{x0}px;top:{y}px;width:{w}px;height:{h}px;border-radius:7px;background:linear-gradient(180deg,rgba(255,255,255,.05),rgba(0,0,0,.35)),#141416;box-shadow:inset 0 1px 0 rgba(255,255,255,.08),0 0 0 1px #000,0 2px 6px rgba(0,0,0,.6)"></div>'
    s += lbl(x0 + 6, y + 17, 26, str(i + 1), 22, '#d9d1c2' if not empty else '#555', 'center', 700)
    if empty:
        s += f'<div class="display" style="left:{x0+46}px;top:{y+15}px;width:170px;height:30px"></div>'
        s += lbl(x0 + 46, y + 21, 170, '+ ADD BLOCK', 14, DIM)
        s += f'<div class="abs" style="left:{x0+240}px;top:{y+21}px;font-size:14px;color:#6e6a64">free slot — pick a block or ask above</div>'
        return s
    name, tag, params, mix = BLOCKS[kind]
    col = FAM[name]
    s += f'<div class="abs" style="left:{x0+36}px;top:{y+10}px;width:4px;height:40px;border-radius:2px;background:{col};box-shadow:0 0 8px {col}"></div>'
    s += f'<div class="display" style="left:{x0+46}px;top:{y+15}px;width:170px;height:30px"></div>'
    s += lbl(x0 + 56, y + 20, 140, name, 16, CREAM, 'left', 700) + f'<div class="abs" style="left:{x0+198}px;top:{y+22}px;color:{DIM};font-size:11px">&#9660;</div>'
    s += led(x0 + 228, y + 22, 16, True, col)
    SHORT = {'FREQUENCY': 'FREQ', 'THRESHOLD': 'THRESH', 'MAX REDUCTION': 'MAX RED', 'MODULATION': 'MOD', 'PING-PONG': 'PING-P',
             'PRE-DELAY': 'PRE-DLY', 'MONO BASS': 'MONO BS', 'FEEDBACK': 'FEEDBK', 'RESONANCE': 'RESO', 'LFO DEPTH': 'LFO DPT',
             'SAMPLE RATE': 'RATE', 'HIGH CUT': 'HI CUT', 'LOW CUT': 'LO CUT', 'MID FREQ': 'MID FRQ', 'ENVELOPE': 'ENV'}
    kx = x0 + 258
    for (pn, pv, pos) in params[:6]:
        s += knob(kx + 17, y + 30, 32, pos)
        s += lbl(kx + 37, y + 13, 52, SHORT.get(pn, pn), 12, '#b8b1a6', 'left') + val(kx + 37, y + 30, 52, pv, 14, CREAM, 'left')
        kx += 82
    kx = x0 + 258 + 6 * 82
    s += f'<div class="abs" style="left:{kx-4}px;top:{y+12}px;width:1px;height:36px;background:#2a2a2e"></div>'
    s += knob(kx + 17, y + 30, 32, 0.22 if mix != '100 %' else 1) + lbl(kx + 37, y + 13, 44, 'MIX', 12, '#b8b1a6', 'left') + val(kx + 37, y + 30, 44, mix, 14, CREAM, 'left')
    bx = x0 + w - 110
    s += key(bx, y + 14, 50, 32, 'OPEN', 'cream' if False else '', 13)
    s += key(bx + 54, y + 14, 22, 15, '&#9650;', '', 8) + key(bx + 54, y + 31, 22, 15, '&#9660;', '', 8)
    s += key(bx + 80, y + 14, 20, 32, '&#10005;', '', 10)
    # medidor de atividade
    s += f'<div class="abs" style="left:{x0+w-10}px;top:{y+8}px;width:4px;height:44px;background:#0a0a0b;border-radius:2px"></div>'
    s += f'<div class="abs" style="left:{x0+w-10}px;top:{y+22}px;width:4px;height:30px;background:{col};border-radius:2px;box-shadow:0 0 5px {col}"></div>'
    return s

def main_screen():
    s = left_column() + prompt_panel()
    s += panel(290, 194, 976, 538)
    s += title(312, 204, 'SIGNAL CHAIN')
    s += f'<div class="abs small" style="left:480px;top:212px">8 SLOTS · ▲▼ TO REORDER · OPEN TO EXPAND</div>'
    y = 240
    for i in range(8):
        s += strip(i, CHAIN[i] if i < 7 else None, y, empty=(i == 7))
        y += 61.5
    s += bottom_bar()
    page(s, '00-main-rack')

# ------------------------------------------------------------------ visualizações (SVG dentro do display 930x300)
VW, VH = 928, 300

def svg_wrap(inner): return f'<svg width="{VW}" height="{VH}" viewBox="0 0 {VW} {VH}" style="position:absolute;left:0;top:0">{inner}</svg>'

def freq_x(f, x0=50, x1=VW-20): return x0 + (math.log10(f) - math.log10(20)) / 3 * (x1 - x0)

def freq_grid(db_range=18, y0=20, y1=VH-36, x0=50, x1=VW-20, labels=True, dbstep=6):
    g = ''
    for f in [20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000]:
        x = freq_x(f, x0, x1)
        g += f'<line x1="{x:.1f}" y1="{y0}" x2="{x:.1f}" y2="{y1}" stroke="{GRID}" stroke-width="1"/>'
        if labels: g += f'<text x="{x:.1f}" y="{y1+18}" fill="#6f6b66" font-size="12" text-anchor="middle" font-family="Bc">{("%dk" % (f//1000)) if f >= 1000 else f}</text>'
    for db in range(-db_range, db_range + 1, dbstep):
        y = y0 + (db_range - db) / (2 * db_range) * (y1 - y0)
        g += f'<line x1="{x0}" y1="{y:.1f}" x2="{x1}" y2="{y:.1f}" stroke="{"#2a2a2e" if db == 0 else GRID}" stroke-width="1"/>'
        if labels: g += f'<text x="{x0-8}" y="{y+4:.1f}" fill="#6f6b66" font-size="11" text-anchor="end" font-family="Bc">{db:+d}</text>'
    return g

def poly(points, stroke=CREAM, width=2.2, fill='none', extra=''):
    d = 'M' + ' L'.join(f'{x:.1f},{y:.1f}' for x, y in points)
    return f'<path d="{d}" stroke="{stroke}" stroke-width="{width}" fill="{fill}" stroke-linejoin="round" {extra}/>'

def area(points, base_y, color, op=.35):
    d = f'M{points[0][0]:.1f},{base_y} ' + ' '.join(f'L{x:.1f},{y:.1f}' for x, y in points) + f' L{points[-1][0]:.1f},{base_y} Z'
    return f'<path d="{d}" fill="url(#g{abs(hash(color))%999})"/>' + \
           f'<defs><linearGradient id="g{abs(hash(color))%999}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="{color}" stop-opacity="{op}"/><stop offset="1" stop-color="{color}" stop-opacity="0"/></linearGradient></defs>'

def glow(): return 'style="filter:drop-shadow(0 0 4px rgba(215,38,43,.9))"'

def node(x, y, label=None, col=RED):
    s = f'<circle cx="{x:.1f}" cy="{y:.1f}" r="8" fill="#0b0b0c" stroke="{col}" stroke-width="2.5"/><circle cx="{x:.1f}" cy="{y:.1f}" r="3" fill="{CREAM}"/>'
    if label: s += f'<text x="{x:.1f}" y="{y-14:.1f}" fill="{CREAM}" font-size="12" text-anchor="middle" font-family="Bc" font-weight="600">{label}</text>'
    return s

# --- biquad (cookbook) para curvas reais
def biq(kind, f, q, db, sr=48000):
    A = 10 ** (db / 40); w = 2 * math.pi * f / sr; c = math.cos(w); s = math.sin(w); al = s / (2 * q)
    if kind == 'lp': b = [(1-c)/2, 1-c, (1-c)/2]; a = [1+al, -2*c, 1-al]
    elif kind == 'hp': b = [(1+c)/2, -(1+c), (1+c)/2]; a = [1+al, -2*c, 1-al]
    elif kind == 'bp': b = [al, 0, -al]; a = [1+al, -2*c, 1-al]
    elif kind == 'pk': b = [1+al*A, -2*c, 1-al*A]; a = [1+al/A, -2*c, 1-al/A]
    elif kind == 'ls':
        sq = 2*math.sqrt(A)*al
        b = [A*((A+1)-(A-1)*c+sq), 2*A*((A-1)-(A+1)*c), A*((A+1)-(A-1)*c-sq)]; a = [(A+1)+(A-1)*c+sq, -2*((A-1)+(A+1)*c), (A+1)+(A-1)*c-sq]
    else:
        sq = 2*math.sqrt(A)*al
        b = [A*((A+1)+(A-1)*c+sq), -2*A*((A-1)+(A+1)*c), A*((A+1)+(A-1)*c-sq)]; a = [(A+1)-(A-1)*c+sq, 2*((A-1)-(A+1)*c), (A+1)-(A-1)*c-sq]
    return b, a

def mag_db(filters, f, sr=48000):
    w = 2 * math.pi * f / sr; z = complex(math.cos(w), -math.sin(w)); tot = 0
    for b, a in filters:
        h = (b[0] + b[1]*z + b[2]*z*z) / (a[0] + a[1]*z + a[2]*z*z); tot += 20 * math.log10(max(abs(h), 1e-9))
    return tot

FREQS = [20 * 10 ** (3 * i / 400) for i in range(401)]

def spectrum(seed=3, tilt=-4.5, level=-12, y0=20, y1=VH-36, rng_db=60, x0=50, x1=VW-20):
    random.seed(seed); pts = []; v = 0
    for f in FREQS[::2]:
        v = 0.7 * v + 0.3 * random.uniform(-6, 6)
        db = level + tilt * math.log2(f / 1000) + v - (8 if f < 60 else 0) - (14 * max(0, math.log2(f / 12000)))
        y = y0 + (-db) / rng_db * (y1 - y0)
        pts.append((freq_x(f, x0, x1), min(y1, max(y0, y))))
    return pts

def viz_eq():
    fl = [biq('hp', 90, .5412, 0), biq('hp', 90, 1.3066, 0), biq('ls', 120, .7071, 2), biq('pk', 2800, 1, 3), biq('hs', 8000, .7071, 2.5)]
    y0, y1 = 20, VH - 36
    toy = lambda db: y0 + (18 - db) / 36 * (y1 - y0)
    pts = [(freq_x(f), toy(mag_db(fl, f))) for f in FREQS]
    sp = spectrum()
    s = freq_grid()
    s += area(sp, y1, '#8d8a85', .28) + poly(sp, '#5c5a57', 1.2)
    s += area(pts, toy(0), RED, .35) + poly(pts, CREAM, 2.6, extra=glow())
    for f, db, l in [(90, 0, 'LOW CUT'), (120, 2, 'LOW'), (2800, 3, 'MID'), (8000, 2.5, 'HIGH'), (20000, 0, 'HIGH CUT')]:
        s += node(freq_x(min(f, 19000)), toy(mag_db(fl, f) if l not in ('LOW CUT', 'HIGH CUT') else 0), l, '#5c5a57' if l == 'HIGH CUT' else RED)
    s += f'<text x="{VW-26}" y="36" fill="#6f6b66" font-size="12" text-anchor="end" font-family="Bc" letter-spacing="1.5">ANALYZER · POST EQ</text>'
    return svg_wrap(s)

def viz_comp():
    s = ''
    # transferência (quadrado à esquerda)
    X0, Y0, S = 40, 22, 250
    tod = lambda db, a: (X0 + (db + 60) / 60 * S) if a == 'x' else (Y0 + (-db) / 60 * S)
    for db in range(-60, 1, 12):
        s += f'<line x1="{tod(db,"x"):.1f}" y1="{Y0}" x2="{tod(db,"x"):.1f}" y2="{Y0+S}" stroke="{GRID}"/><line x1="{X0}" y1="{tod(db,"y"):.1f}" x2="{X0+S}" y2="{tod(db,"y"):.1f}" stroke="{GRID}"/>'
    s += f'<line x1="{X0}" y1="{Y0+S}" x2="{X0+S}" y2="{Y0}" stroke="#333" stroke-dasharray="4 4"/>'
    thr, ratio, knee = -22, 4, 6
    def out(x):
        o = x - thr
        if 2 * abs(o) <= knee: return x + (1/ratio - 1) * (o + knee/2) ** 2 / (2 * knee)
        return x + (1/ratio - 1) * o if o > 0 else x
    pts = [(tod(x, 'x'), tod(out(x), 'y')) for x in np.linspace(-60, 0, 120)]
    s += poly(pts, CREAM, 2.6, extra=glow())
    s += f'<line x1="{tod(thr,"x"):.1f}" y1="{Y0}" x2="{tod(thr,"x"):.1f}" y2="{Y0+S}" stroke="{RED}" stroke-dasharray="3 4" stroke-opacity=".8"/>'
    s += f'<text x="{tod(thr,"x")+6:.1f}" y="{Y0+S-8}" fill="{RED}" font-size="12" font-family="Bc">THRESHOLD</text>'
    s += f'<circle cx="{tod(-12,"x"):.1f}" cy="{tod(out(-12),"y"):.1f}" r="7" fill="{RED}" {glow()}/>'
    s += f'<text x="{X0}" y="{Y0+S+22}" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">INPUT → OUTPUT</text>'
    # histórico de GR (centro)
    HX, HW = 330, 470
    s += f'<text x="{HX}" y="34" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">LEVEL &amp; GAIN REDUCTION</text>'
    random.seed(5); lv = []; gr = []
    for i in range(160):
        t = i / 160
        e = 0.55 + 0.35 * abs(math.sin(t * 18)) * (0.6 + 0.4 * random.random())
        lv.append((HX + t * HW, 260 - e * 190)); g = max(0, (e - .55)) * 28
        gr.append((HX + t * HW, 50 + g * 4))
    s += area(lv, 262, CREAM, .18) + poly(lv, '#9a948a', 1.3)
    s += f'<path d="M{HX},50 ' + ' '.join(f'L{x:.1f},{y:.1f}' for x, y in gr) + f' L{HX+HW},50 Z" fill="{RED}" opacity=".35"/>' + poly(gr, RED, 2.2, extra=glow())
    s += f'<text x="{HX+HW}" y="34" fill="{RED}" font-size="12" text-anchor="end" font-family="Bc" letter-spacing="1.5">GAIN REDUCTION ↓</text>'
    s += f'<line x1="{HX}" y1="50" x2="{HX+HW}" y2="50" stroke="#2a2a2e"/>'
    # medidor de GR vertical (direita)
    MX = 850
    s += f'<text x="{MX+20}" y="34" fill="#6f6b66" font-size="12" font-family="Bc" text-anchor="middle" letter-spacing="1.5">GR</text>'
    for k in range(20):
        on = k < 7
        s += f'<rect x="{MX}" y="{46+k*11}" width="40" height="8" rx="1.5" fill="{RED if on else "#2a1112"}" {glow() if on else ""}/>'
        if k % 4 == 0: s += f'<text x="{MX-6}" y="{54+k*11}" fill="#6f6b66" font-size="11" text-anchor="end" font-family="Bc">{k}</text>'
    s += f'<text x="{MX+20}" y="290" fill="{CREAM}" font-size="20" text-anchor="middle" font-family="Bc" font-weight="700">-6.4</text>'
    return svg_wrap(s)

def viz_sat():
    s = ''
    # seletor de tipo como 3 "cartas"
    for i, (t, sub) in enumerate([('TUBE', '12AX7 STYLE'), ('TAPE', '15 IPS'), ('TRANSISTOR', 'CLASS A')]):
        x = 24 + i * 112; on = i == 0
        s += f'<rect x="{x}" y="22" width="104" height="160" rx="8" fill="{"#1a0d0d" if on else "#0d0d0f"}" stroke="{RED if on else "#2a2a2e"}" stroke-width="{2 if on else 1}"/>'
        cx = x + 52
        if t == 'TUBE':
            s += f'<path d="M{cx-18},140 L{cx-18},70 Q{cx-18},42 {cx},42 Q{cx+18},42 {cx+18},70 L{cx+18},140 Z" fill="none" stroke="{CREAM}" stroke-width="2"/>'
            s += f'<ellipse cx="{cx}" cy="108" rx="11" ry="26" fill="#ff7a2a" opacity=".55" style="filter:blur(4px)"/><line x1="{cx}" y1="84" x2="{cx}" y2="132" stroke="#ffb066" stroke-width="2"/>'
            for k in range(4): s += f'<line x1="{cx-10+k*7}" y1="140" x2="{cx-10+k*7}" y2="152" stroke="{CREAM}" stroke-width="2"/>'
        elif t == 'TAPE':
            for dx in (-22, 22):
                s += f'<circle cx="{cx+dx}" cy="88" r="20" fill="none" stroke="{CREAM}" stroke-width="2"/><circle cx="{cx+dx}" cy="88" r="5" fill="{CREAM}"/>'
                for a in range(0, 360, 120): s += f'<line x1="{cx+dx}" y1="88" x2="{cx+dx+14*math.cos(math.radians(a)):.1f}" y2="{88+14*math.sin(math.radians(a)):.1f}" stroke="{CREAM}" stroke-width="1.5"/>'
            s += f'<path d="M{cx-42},104 L{cx-30},140 L{cx+30},140 L{cx+42},104" fill="none" stroke="{DIM}" stroke-width="1.5"/>'
        else:
            s += f'<path d="M{cx-16},64 h32 v36 a16,16 0 0 1 -32,0 Z" fill="none" stroke="{CREAM}" stroke-width="2"/>'
            for k in range(3): s += f'<line x1="{cx-10+k*10}" y1="116" x2="{cx-10+k*10}" y2="146" stroke="{CREAM}" stroke-width="2"/>'
        s += f'<text x="{cx}" y="200" fill="{CREAM if on else DIM}" font-size="15" text-anchor="middle" font-family="Bc" font-weight="700">{t}</text>'
        s += f'<text x="{cx}" y="216" fill="#6f6b66" font-size="11" text-anchor="middle" font-family="Bc" letter-spacing="1">{sub}</text>'
    # curva de transferência
    X0, Y0, S = 380, 24, 236
    s += f'<rect x="{X0}" y="{Y0}" width="{S}" height="{S}" fill="none" stroke="{GRID}"/><line x1="{X0}" y1="{Y0+S/2}" x2="{X0+S}" y2="{Y0+S/2}" stroke="{GRID}"/><line x1="{X0+S/2}" y1="{Y0}" x2="{X0+S/2}" y2="{Y0+S}" stroke="{GRID}"/>'
    s += f'<line x1="{X0}" y1="{Y0+S}" x2="{X0+S}" y2="{Y0}" stroke="#333" stroke-dasharray="4 4"/>'
    d = 10 ** (9 / 20); b = .25
    f = lambda u: (math.tanh(d * u * .9 + b) - math.tanh(b)) / (math.tanh(d * .9 + b) - math.tanh(b))
    pts = [(X0 + (u + 1) / 2 * S, Y0 + (1 - (max(-1.2, min(1.2, f(u))) + 1) / 2) * S) for u in np.linspace(-1, 1, 120)]
    s += poly(pts, CREAM, 2.6, extra=glow())
    s += f'<text x="{X0}" y="{Y0+S+20}" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">TRANSFER CURVE</text>'
    # harmônicos
    HX = 660
    s += f'<text x="{HX}" y="36" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">HARMONICS</text>'
    hs = [1.0, .42, .30, .16, .12, .07, .05, .03]
    for i, h in enumerate(hs):
        hh = 200 * (0.15 + 0.85 * (1 + math.log10(max(h, 1e-3)) / 3))
        x = HX + i * 30
        s += f'<rect x="{x}" y="{256-hh:.1f}" width="20" height="{hh:.1f}" rx="2" fill="{CREAM if i==0 else RED}" opacity="{1 if i==0 else .9}"/>'
        s += f'<text x="{x+10}" y="276" fill="#6f6b66" font-size="11" text-anchor="middle" font-family="Bc">{i+1}</text>'
    return svg_wrap(s)

def viz_reverb():
    s = ''
    for i, t in enumerate(['ROOM', 'CHURCH', 'PLATE', 'CATHEDRAL']):
        x = 22 + i * 92; on = i == 1
        s += f'<rect x="{x}" y="20" width="84" height="84" rx="7" fill="{"#0d1520" if on else "#0d0d0f"}" stroke="{"#6fb6ff" if on else "#2a2a2e"}" stroke-width="{2 if on else 1}"/>'
        cx = x + 42; c = CREAM if on else DIM
        if t == 'ROOM': s += f'<path d="M{cx-24},86 V52 L{cx},36 L{cx+24},52 V86 Z" fill="none" stroke="{c}" stroke-width="2"/>'
        elif t == 'CHURCH': s += f'<path d="M{cx-26},88 V58 L{cx-12},46 V30 L{cx},20 L{cx+12},30 V46 L{cx+26},58 V88 Z M{cx-6},88 V72 Q{cx},64 {cx+6},72 V88" fill="none" stroke="{c}" stroke-width="2"/><line x1="{cx}" y1="22" x2="{cx}" y2="30" stroke="{c}" stroke-width="2"/>'
        elif t == 'PLATE': s += f'<rect x="{cx-26}" y="40" width="52" height="34" fill="none" stroke="{c}" stroke-width="2"/><path d="M{cx-20},48 q10,10 20,0 t20,0 M{cx-20},62 q10,10 20,0 t20,0" fill="none" stroke="{c}" stroke-width="1.4"/>'
        else: s += f'<path d="M{cx-30},88 V50 Q{cx-30},28 {cx-14},22 Q{cx},14 {cx+14},22 Q{cx+30},28 {cx+30},50 V88 M{cx-14},88 V54 Q{cx},40 {cx+14},54 V88" fill="none" stroke="{c}" stroke-width="2"/>'
        s += f'<text x="{cx}" y="122" fill="{c}" font-size="13" text-anchor="middle" font-family="Bc" font-weight="700">{t}</text>'
    # resposta ao impulso
    X0, X1, Y = 400, VW - 20, 250
    s += f'<line x1="{X0}" y1="{Y}" x2="{X1}" y2="{Y}" stroke="#2a2a2e"/>'
    for k in range(7):
        x = X0 + k * (X1 - X0) / 6
        s += f'<line x1="{x:.1f}" y1="30" x2="{x:.1f}" y2="{Y}" stroke="{GRID}"/><text x="{x:.1f}" y="{Y+18}" fill="#6f6b66" font-size="11" text-anchor="middle" font-family="Bc">{k*0.6:.1f}s</text>'
    s += f'<line x1="{X0}" y1="{Y}" x2="{X0}" y2="50" stroke="{CREAM}" stroke-width="3"/>'
    pre = X0 + 0.035 / 3.6 * (X1 - X0)
    s += f'<text x="{X0+4}" y="44" fill="{CREAM}" font-size="11" font-family="Bc">DRY</text>'
    random.seed(9)
    for k in range(14):
        x = pre + random.uniform(0, 40); h = random.uniform(40, 150) * (1 - k / 18)
        s += f'<line x1="{x:.1f}" y1="{Y}" x2="{x:.1f}" y2="{Y-h:.1f}" stroke="#6fb6ff" stroke-width="2"/>'
    env = [(pre + 20 + t * (X1 - pre - 20), Y - 150 * math.exp(-t * 3.6 / 3.2 * 2.3) ) for t in np.linspace(0, 1, 100)]
    s += area(env, Y, '#6fb6ff', .45) + poly(env, '#6fb6ff', 2.2, extra='style="filter:drop-shadow(0 0 4px rgba(111,182,255,.8))"')
    s += f'<text x="{pre+8:.1f}" y="{Y-162}" fill="#6fb6ff" font-size="11" font-family="Bc">EARLY</text><text x="{X0+190}" y="{Y-120}" fill="#6fb6ff" font-size="11" font-family="Bc">TAIL · DECAY 3.2 s</text>'
    s += f'<text x="22" y="160" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">STEREO SPACE</text>'
    s += f'<path d="M40,270 Q190,150 340,270" fill="none" stroke="#2a2a2e" stroke-width="2"/><path d="M60,262 Q190,176 320,262" fill="none" stroke="#6fb6ff" stroke-width="3" opacity=".8"/>'
    s += f'<circle cx="190" cy="270" r="6" fill="{CREAM}"/><text x="40" y="290" fill="#6f6b66" font-size="11" font-family="Bc">L</text><text x="334" y="290" fill="#6f6b66" font-size="11" font-family="Bc">R</text>'
    return svg_wrap(s)

def viz_delay():
    s = ''
    X0, X1 = 60, VW - 30
    beats = 8
    for b in range(beats * 2 + 1):
        x = X0 + b * (X1 - X0) / (beats * 2)
        s += f'<line x1="{x:.1f}" y1="30" x2="{x:.1f}" y2="262" stroke="{"#26262a" if b % 2 == 0 else GRID}"/>'
        if b % 2 == 0: s += f'<text x="{x:.1f}" y="282" fill="#6f6b66" font-size="11" text-anchor="middle" font-family="Bc">{b//2+1 if b < beats*2 else ""}</text>'
    s += f'<text x="20" y="96" fill="{CREAM}" font-size="14" font-family="Bc" font-weight="700">L</text><text x="20" y="216" fill="{CREAM}" font-size="14" font-family="Bc" font-weight="700">R</text>'
    s += f'<line x1="{X0}" y1="140" x2="{X1}" y2="140" stroke="#2a2a2e"/><line x1="{X0}" y1="262" x2="{X1}" y2="262" stroke="#2a2a2e"/>'
    s += f'<rect x="{X0-3}" y="40" width="6" height="100" fill="{CREAM}" rx="2"/><rect x="{X0-3}" y="162" width="6" height="100" fill="{CREAM}" rx="2"/>'
    step = 0.75 * (X1 - X0) / (beats)
    for k in range(1, 11):
        x = X0 + k * step
        if x > X1: break
        h = 100 * 0.38 ** (k * 0.55)
        top = k % 2 == 1
        yb = 140 if top else 262
        s += f'<rect x="{x-3:.1f}" y="{yb-h:.1f}" width="6" height="{h:.1f}" fill="#6fb6ff" rx="2" style="filter:drop-shadow(0 0 5px rgba(111,182,255,.9))"/>'
        s += f'<path d="M{x-step+3:.1f},{(140 if not top else 262)-4} Q{x-step/2:.1f},150 {x-3:.1f},{yb-4}" fill="none" stroke="#6fb6ff" stroke-opacity=".25" stroke-dasharray="3 4"/>'
    s += f'<text x="{X1}" y="22" fill="#6f6b66" font-size="12" text-anchor="end" font-family="Bc" letter-spacing="1.5">1/8 DOTTED · 375 ms @ 120 BPM · PING-PONG</text>'
    return svg_wrap(s)

def lfo_lines(n, phase_off, amp, col, x0=40, x1=VW-20, ymid=150, cycles=2):
    s = ''
    for v in range(n):
        pts = [(x0 + t * (x1 - x0), ymid - amp * math.sin(2 * math.pi * (cycles * t + v / n + phase_off))) for t in np.linspace(0, 1, 200)]
        s += poly(pts, col, 2.2, extra=f'opacity="{1 - v*0.22}" style="filter:drop-shadow(0 0 4px {col})"')
    return s

def viz_chorus():
    s = f'<text x="40" y="30" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">VOICE DELAY MODULATION (ms)</text>'
    for k in range(5): s += f'<line x1="40" y1="{60+k*45}" x2="{VW-260}" y2="{60+k*45}" stroke="{GRID}"/>'
    s += lfo_lines(3, 0, 70, '#b98cff', x1=VW - 260)
    s += f'<line x1="{(VW-260)*0.38:.1f}" y1="44" x2="{(VW-260)*0.38:.1f}" y2="256" stroke="{CREAM}" stroke-width="1.5"/>'
    for i, c in enumerate(['V1', 'V2', 'V3']): s += f'<text x="{VW-292}" y="{80+i*18}" fill="#b98cff" opacity="{1-i*0.22}" font-size="12" font-family="Bc" font-weight="700">{c}</text>'
    # campo estéreo
    cx, cy = VW - 130, 170
    s += f'<text x="{cx}" y="30" fill="#6f6b66" font-size="12" text-anchor="middle" font-family="Bc" letter-spacing="1.5">STEREO FIELD</text>'
    s += f'<path d="M{cx-110},{cy+60} A120,120 0 0 1 {cx+110},{cy+60}" fill="none" stroke="#2a2a2e" stroke-width="2"/>'
    for i, a in enumerate([-50, 0, 50]):
        x = cx + 100 * math.sin(math.radians(a)); y = cy + 60 - 100 * math.cos(math.radians(a)) * 0.9
        s += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="11" fill="#b98cff" opacity=".85" style="filter:drop-shadow(0 0 6px #b98cff)"/><text x="{x:.1f}" y="{y+4:.1f}" fill="#111" font-size="11" text-anchor="middle" font-family="Bc" font-weight="700">{i+1}</text>'
    s += f'<circle cx="{cx}" cy="{cy+60}" r="5" fill="{CREAM}"/>'
    return svg_wrap(s)

def viz_phaser():
    s = freq_grid(db_range=24, dbstep=12)
    y0, y1 = 20, VH - 36
    toy = lambda db: y0 + (24 - db) / 48 * (y1 - y0)
    for ph, op in [(0.0, .25), (0.25, 1)]:
        c = 1000 * 2 ** (1.2 * math.sin(2 * math.pi * ph))
        pts = []
        for f in FREQS:
            # 6 passa-tudo -> 3 notches aproximados
            r = 0
            for k, m in enumerate([0.35, 1.0, 2.9]):
                fn = c * m
                r += -20 * math.exp(-((math.log2(f / fn)) ** 2) / 0.04)
            pts.append((freq_x(f), toy(r + 1.5)))
        s += poly(pts, '#b98cff' if op == 1 else '#6c5a8c', 2.6 if op == 1 else 1.6, extra=f'opacity="{op}"' + (' style="filter:drop-shadow(0 0 4px #b98cff)"' if op == 1 else ''))
    s += f'<rect x="{freq_x(450):.1f}" y="{y0}" width="{freq_x(2300)-freq_x(450):.1f}" height="{y1-y0}" fill="#b98cff" opacity=".06"/>'
    s += f'<text x="{(freq_x(450)+freq_x(2300))/2:.1f}" y="{y0+16}" fill="#b98cff" font-size="12" text-anchor="middle" font-family="Bc" letter-spacing="1.5">SWEEP RANGE</text>'
    for i, t in enumerate(['PHASER', 'FLANGER']):
        s += f'<rect x="{VW-190+i*86}" y="{y0+4}" width="80" height="26" rx="5" fill="{"#1a1124" if i==0 else "#0d0d0f"}" stroke="{"#b98cff" if i==0 else "#2a2a2e"}"/><text x="{VW-150+i*86}" y="{y0+22}" fill="{CREAM if i==0 else DIM}" font-size="13" text-anchor="middle" font-family="Bc" font-weight="700">{t}</text>'
    return svg_wrap(s)

def viz_filter():
    s = freq_grid(db_range=24, dbstep=12)
    y0, y1 = 20, VH - 36
    toy = lambda db: y0 + (24 - db) / 48 * (y1 - y0)
    q = 0.5 / (1 - 0.975 * 0.55)
    for fc, col, w, op in [(1800 * 2 ** -1.05, '#5c5a57', 1.4, .6), (1800 * 2 ** 1.05, '#5c5a57', 1.4, .6), (2600, CREAM, 2.6, 1)]:
        fl = [biq('lp', fc, q, 0)]
        pts = [(freq_x(f), toy(max(-26, mag_db(fl, f)))) for f in FREQS]
        if op == 1: s += area(pts, y1, RED, .3)
        s += poly(pts, col, w, extra=(glow() if op == 1 else f'opacity="{op}" stroke-dasharray="5 4"'))
    s += f'<rect x="{freq_x(1800*2**-1.05):.1f}" y="{y1-12}" width="{freq_x(1800*2**1.05)-freq_x(1800*2**-1.05):.1f}" height="6" rx="3" fill="{RED}" opacity=".7"/>'
    s += f'<text x="{freq_x(1800):.1f}" y="{y1-18}" fill="{RED}" font-size="12" text-anchor="middle" font-family="Bc" letter-spacing="1.5">LFO RANGE</text>'
    s += node(freq_x(2600), toy(mag_db([biq('lp', 2600, q, 0)], 2600)), 'CUTOFF NOW')
    for i, t in enumerate(['LOW-PASS', 'HIGH-PASS', 'BAND-PASS']):
        s += f'<rect x="{VW-270+i*86}" y="{y0+4}" width="80" height="26" rx="5" fill="{"#1a1a14" if i==0 else "#0d0d0f"}" stroke="{CREAM if i==0 else "#2a2a2e"}"/><text x="{VW-230+i*86}" y="{y0+22}" fill="{CREAM if i==0 else DIM}" font-size="13" text-anchor="middle" font-family="Bc" font-weight="700">{t}</text>'
    return svg_wrap(s)

def viz_deesser():
    X1 = VW - 130
    s = freq_grid(db_range=30, dbstep=10, x1=X1)
    y0, y1 = 20, VH - 36
    sp = spectrum(seed=11, tilt=-3, level=-14, rng_db=60, x1=X1)
    sp = [(x, y - (40 if freq_x(5500, 50, X1) < x < freq_x(9000, 50, X1) else 0) * math.exp(-((x - freq_x(7000, 50, X1)) / 60) ** 2)) for x, y in sp]
    s += area(sp, y1, CREAM, .2) + poly(sp, '#9a948a', 1.4)
    bx = freq_x(6500, 50, X1)
    s += f'<rect x="{bx:.1f}" y="{y0}" width="{X1-bx:.1f}" height="{y1-y0}" fill="#ffb238" opacity=".09"/>'
    s += f'<line x1="{bx:.1f}" y1="{y0}" x2="{bx:.1f}" y2="{y1}" stroke="#ffb238" stroke-width="2"/>'
    ty = y0 + (30 - 2) / 60 * (y1 - y0)
    s += f'<line x1="{bx:.1f}" y1="{ty:.1f}" x2="{X1}" y2="{ty:.1f}" stroke="{RED}" stroke-width="2" stroke-dasharray="6 4"/><text x="{bx+8:.1f}" y="{ty-8:.1f}" fill="{RED}" font-size="12" font-family="Bc" letter-spacing="1.5">THRESHOLD</text>'
    s += f'<text x="{bx+8:.1f}" y="{y0+18}" fill="#ffb238" font-size="12" font-family="Bc" letter-spacing="1.5">SIBILANCE BAND</text>'
    MX = VW - 90
    s += f'<text x="{MX+20}" y="34" fill="#6f6b66" font-size="12" font-family="Bc" text-anchor="middle" letter-spacing="1.5">GR</text>'
    for k in range(18):
        on = k < 5
        s += f'<rect x="{MX}" y="{46+k*12}" width="40" height="9" rx="1.5" fill="{"#ffb238" if on else "#2a2010"}"/>'
    s += f'<text x="{MX+20}" y="290" fill="{CREAM}" font-size="20" text-anchor="middle" font-family="Bc" font-weight="700">-4.8</text>'
    return svg_wrap(s)

def viz_gate():
    s = ''
    X0, X1, Y0, Y1 = 60, VW - 150, 30, 250
    for db in range(0, -81, -20):
        y = Y0 + (-db) / 80 * (Y1 - Y0)
        s += f'<line x1="{X0}" y1="{y:.1f}" x2="{X1}" y2="{y:.1f}" stroke="{GRID}"/><text x="{X0-8}" y="{y+4:.1f}" fill="#6f6b66" font-size="11" text-anchor="end" font-family="Bc">{db}</text>'
    random.seed(2); pts = []; states = []
    for i in range(240):
        t = i / 240
        burst = (math.sin(t * 2 * math.pi * 4.3) > 0.1)
        lv = (-14 - 10 * random.random()) if burst else (-62 - 8 * random.random())
        pts.append((X0 + t * (X1 - X0), Y0 + (-lv) / 80 * (Y1 - Y0))); states.append(burst)
    # regiões fechadas
    x_start = None
    for i, st in enumerate(states + [True]):
        x = X0 + i / 240 * (X1 - X0)
        if not st and x_start is None: x_start = x
        if st and x_start is not None:
            s += f'<rect x="{x_start:.1f}" y="{Y0}" width="{x-x_start:.1f}" height="{Y1-Y0}" fill="{RED}" opacity=".08"/>'; x_start = None
    s += area(pts, Y1, CREAM, .25) + poly(pts, '#b8b1a6', 1.4)
    ty = Y0 + 45 / 80 * (Y1 - Y0)
    s += f'<line x1="{X0}" y1="{ty:.1f}" x2="{X1}" y2="{ty:.1f}" stroke="#ffb238" stroke-width="2.2" style="filter:drop-shadow(0 0 4px #ffb238)"/><text x="{X0+6}" y="{ty-8:.1f}" fill="#ffb238" font-size="12" font-family="Bc" letter-spacing="1.5">THRESHOLD -45 dB</text>'
    # gate state lane
    s += f'<text x="{X0}" y="{Y1+30}" fill="#6f6b66" font-size="11" font-family="Bc" letter-spacing="1.5">GATE</text>'
    for i, st in enumerate(states):
        if i % 2: continue
        s += f'<rect x="{X0+40+i/240*(X1-X0-40):.1f}" y="{Y1+20}" width="3" height="12" fill="{"#3fd17a" if st else RED}" opacity="{.9 if st else .5}"/>'
    # LED grande
    cx = VW - 70
    s += f'<circle cx="{cx}" cy="110" r="30" fill="#3fd17a" style="filter:drop-shadow(0 0 14px #3fd17a)"/><circle cx="{cx-8}" cy="100" r="8" fill="#c8ffd9" opacity=".7"/>'
    s += f'<text x="{cx}" y="170" fill="{CREAM}" font-size="20" text-anchor="middle" font-family="Bc" font-weight="700">OPEN</text>'
    s += f'<circle cx="{cx}" cy="220" r="12" fill="#3a1112"/><text x="{cx}" y="252" fill="#6f6b66" font-size="12" text-anchor="middle" font-family="Bc">CLOSED</text>'
    return svg_wrap(s)

def viz_stereo():
    s = ''
    cx, cy, r = 190, 160, 120
    s += f'<circle cx="{cx}" cy="{cy}" r="{r}" fill="none" stroke="{GRID}" stroke-width="1.5"/><circle cx="{cx}" cy="{cy}" r="{r/2}" fill="none" stroke="{GRID}"/>'
    s += f'<line x1="{cx-r}" y1="{cy}" x2="{cx+r}" y2="{cy}" stroke="{GRID}"/><line x1="{cx}" y1="{cy-r}" x2="{cx}" y2="{cy+r}" stroke="{GRID}"/>'
    s += f'<line x1="{cx-r*.7:.1f}" y1="{cy-r*.7:.1f}" x2="{cx+r*.7:.1f}" y2="{cy+r*.7:.1f}" stroke="#222"/><line x1="{cx+r*.7:.1f}" y1="{cy-r*.7:.1f}" x2="{cx-r*.7:.1f}" y2="{cy+r*.7:.1f}" stroke="#222"/>'
    random.seed(4)
    for i in range(900):
        m = random.gauss(0, 0.45); sd = random.gauss(0, 0.22)
        x = cx + sd * r; y = cy - m * r
        if (x - cx) ** 2 + (y - cy) ** 2 < r * r:
            s += f'<circle cx="{x:.1f}" cy="{y:.1f}" r="1.3" fill="#6fb6ff" opacity=".55"/>'
    for t, x, y in [('M', cx, cy - r - 8), ('S', cx + r + 10, cy + 4), ('L', cx - r * .72, cy - r * .75), ('R', cx + r * .72, cy - r * .75)]:
        s += f'<text x="{x:.1f}" y="{y:.1f}" fill="#6f6b66" font-size="12" text-anchor="middle" font-family="Bc">{t}</text>'
    # correlação
    X0, X1 = 400, VW - 40
    s += f'<text x="{X0}" y="44" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">CORRELATION</text>'
    s += f'<rect x="{X0}" y="56" width="{X1-X0}" height="14" rx="7" fill="#0d0d0f" stroke="#2a2a2e"/>'
    xc = X0 + (0.62 + 1) / 2 * (X1 - X0)
    s += f'<rect x="{(X0+X1)/2:.1f}" y="56" width="{xc-(X0+X1)/2:.1f}" height="14" rx="7" fill="#6fb6ff" opacity=".8"/>'
    for t, x in [('-1', X0), ('0', (X0 + X1) / 2), ('+1', X1)]: s += f'<text x="{x:.1f}" y="90" fill="#6f6b66" font-size="11" text-anchor="middle" font-family="Bc">{t}</text>'
    # largura
    s += f'<text x="{X0}" y="130" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">WIDTH 140 %</text>'
    mid = (X0 + X1) / 2
    s += f'<path d="M{mid},260 L{mid-180},150 A220,220 0 0 1 {mid+180},150 Z" fill="#6fb6ff" opacity=".12" stroke="#6fb6ff"/>'
    s += f'<path d="M{mid},260 L{mid-120},170 A160,160 0 0 1 {mid+120},170 Z" fill="none" stroke="{CREAM}" stroke-dasharray="4 4" opacity=".6"/>'
    s += f'<text x="{mid+130}" y="200" fill="{CREAM}" font-size="11" font-family="Bc" opacity=".7">100 %</text>'
    s += f'<rect x="{mid-30}" y="250" width="60" height="16" rx="4" fill="{RED}" opacity=".8"/><text x="{mid}" y="262" fill="#fff" font-size="11" text-anchor="middle" font-family="Bc" font-weight="700">MONO &lt;120 Hz</text>'
    return svg_wrap(s)

def viz_tremolo():
    s = ''
    X0, X1 = 60, VW - 30
    for k in range(9): s += f'<line x1="{X0+k*(X1-X0)/8:.1f}" y1="30" x2="{X0+k*(X1-X0)/8:.1f}" y2="270" stroke="{GRID}"/>'
    s += f'<text x="20" y="96" fill="{CREAM}" font-size="14" font-family="Bc" font-weight="700">L</text><text x="20" y="216" fill="{CREAM}" font-size="14" font-family="Bc" font-weight="700">R</text>'
    for lane, (ymid, ph) in enumerate([(90, 0), (210, 0.5)]):
        pts = [(X0 + t * (X1 - X0), ymid - 50 * (0.5 * (1 - 0.5) + 0.5 * 0.5 * math.sin(2 * math.pi * (4 * t + ph))) * 2) for t in np.linspace(0, 1, 300)]
        s += area(pts, ymid + 55, '#b98cff', .3) + poly(pts, '#b98cff', 2.4, extra='style="filter:drop-shadow(0 0 4px #b98cff)"')
    s += f'<line x1="{X0+0.37*(X1-X0):.1f}" y1="30" x2="{X0+0.37*(X1-X0):.1f}" y2="270" stroke="{CREAM}" stroke-width="1.5"/>'
    s += f'<text x="{X1}" y="22" fill="#6f6b66" font-size="12" text-anchor="end" font-family="Bc" letter-spacing="1.5">SINE · 1/8 · AUTO-PAN 60 %</text>'
    return svg_wrap(s)

def viz_lofi():
    s = ''
    X0, X1, ym = 40, 560, 150
    for k in range(-4, 5): s += f'<line x1="{X0}" y1="{ym+k*27}" x2="{X1}" y2="{ym+k*27}" stroke="{GRID}"/>'
    smooth = [(X0 + t * (X1 - X0), ym - 100 * math.sin(2 * math.pi * 2 * t) * (0.85 + 0.15 * math.sin(2 * math.pi * 0.5 * t))) for t in np.linspace(0, 1, 300)]
    s += poly(smooth, '#5c5a57', 1.6, extra='stroke-dasharray="4 4"')
    steps = 26; levels = 8; d = 'M'
    random.seed(1)
    for i in range(steps):
        t0 = i / steps; t1 = (i + 1) / steps
        v = math.sin(2 * math.pi * 2 * t0) * (0.85 + 0.15 * math.sin(2 * math.pi * 0.5 * t0))
        q = round(v * levels / 2) / (levels / 2)
        y = ym - 100 * q + random.uniform(-3, 3)
        d += f'{X0+t0*(X1-X0):.1f},{y:.1f} L{X0+t1*(X1-X0):.1f},{y:.1f} L'
    s += f'<path d="{d[:-2]}" fill="none" stroke="{RED}" stroke-width="2.6" style="filter:drop-shadow(0 0 4px rgba(215,38,43,.9))"/>'
    s += f'<text x="{X0}" y="30" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">8 BIT · 11 kHz</text>'
    for i in range(160):
        s += f'<circle cx="{random.uniform(X0,X1):.1f}" cy="{random.uniform(40,260):.1f}" r=".9" fill="{CREAM}" opacity=".25"/>'
    # banda e wow (direita)
    BX = 600
    s += f'<text x="{BX}" y="30" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">BAND</text>'
    pts = []
    for f in FREQS[::4]:
        db = mag_db([biq('hp', 200, .7071, 0), biq('lp', 5000, .7071, 0)], f)
        pts.append((BX + (math.log10(f) - math.log10(20)) / 3 * 300, 60 - db * 2))
    s += area(pts, 130, CREAM, .2) + poly(pts, CREAM, 2.2)
    s += f'<text x="{BX}" y="170" fill="#6f6b66" font-size="12" font-family="Bc" letter-spacing="1.5">WOW &amp; FLUTTER</text>'
    w = [(BX + t * 300, 225 - 30 * math.sin(2 * math.pi * 1.5 * t) - 8 * math.sin(2 * math.pi * 9 * t)) for t in np.linspace(0, 1, 200)]
    s += poly(w, RED, 2.2, extra=glow())
    return svg_wrap(s)

def viz_limiter():
    s = ''
    X0, X1, Y0, Y1 = 60, VW - 150, 30, 270
    toy = lambda db: Y0 + (-db) / 24 * (Y1 - Y0)
    for db in range(0, -25, 6): s += f'<line x1="{X0}" y1="{toy(db):.1f}" x2="{X1}" y2="{toy(db):.1f}" stroke="{GRID}"/><text x="{X0-8}" y="{toy(db)+4:.1f}" fill="#6f6b66" font-size="11" text-anchor="end" font-family="Bc">{db}</text>'
    random.seed(8); inp = []; outp = []; gr = []
    for i in range(200):
        t = i / 200
        lv = -10 + 9 * abs(math.sin(t * 23)) * random.uniform(.6, 1.0) + (5 if i % 37 == 0 else 0)
        inp.append((X0 + t * (X1 - X0), toy(min(5, lv + 6))))
        o = min(-1, lv + 6); outp.append((X0 + t * (X1 - X0), toy(o)))
        gr.append((X0 + t * (X1 - X0), Y0 + max(0, lv + 6 + 1) * 6))
    s += poly(inp, '#5c5a57', 1.2) + area(outp, Y1, CREAM, .22) + poly(outp, CREAM, 1.8)
    s += f'<line x1="{X0}" y1="{toy(-1):.1f}" x2="{X1}" y2="{toy(-1):.1f}" stroke="{RED}" stroke-width="2.4" {glow()}/><text x="{X1-4}" y="{toy(-1)-8:.1f}" fill="{RED}" font-size="12" text-anchor="end" font-family="Bc" letter-spacing="1.5">CEILING -1.0 dB</text>'
    for x, y in gr[::2]:
        if y > Y0 + 1: s += f'<rect x="{x-1:.1f}" y="{Y0}" width="2.2" height="{y-Y0:.1f}" fill="#ffb238" opacity=".75"/>'
    cx = VW - 70
    s += f'<text x="{cx}" y="44" fill="#6f6b66" font-size="12" text-anchor="middle" font-family="Bc" letter-spacing="1.5">GR</text>'
    s += f'<text x="{cx}" y="96" fill="{CREAM}" font-size="34" text-anchor="middle" font-family="Bc" font-weight="700">-3.2</text>'
    s += f'<text x="{cx}" y="150" fill="#6f6b66" font-size="12" text-anchor="middle" font-family="Bc" letter-spacing="1.5">OUT PEAK</text><text x="{cx}" y="182" fill="{CREAM}" font-size="22" text-anchor="middle" font-family="Bc" font-weight="700">-1.0</text>'
    s += f'<text x="{cx}" y="226" fill="#6f6b66" font-size="12" text-anchor="middle" font-family="Bc" letter-spacing="1.5">IN PEAK</text><text x="{cx}" y="256" fill="{CREAM}" font-size="22" text-anchor="middle" font-family="Bc" font-weight="700">+2.2</text>'
    return svg_wrap(s)

VIZ = {'eq': viz_eq, 'comp': viz_comp, 'sat': viz_sat, 'reverb': viz_reverb, 'delay': viz_delay, 'chorus': viz_chorus,
       'phaser': viz_phaser, 'filter': viz_filter, 'deesser': viz_deesser, 'gate': viz_gate, 'stereo': viz_stereo,
       'tremolo': viz_tremolo, 'lofi': viz_lofi, 'limiter': viz_limiter}

ORDER = ['eq', 'comp', 'sat', 'reverb', 'delay', 'chorus', 'phaser', 'filter', 'deesser', 'gate', 'stereo', 'tremolo', 'lofi', 'limiter']

def expanded(kind, idx):
    name, tag, params, mix = BLOCKS[kind]
    slotn = CHAIN.index(kind) + 1 if kind in CHAIN[:7] else 3
    col = FAM[name]
    s = left_column() + prompt_panel()
    s += panel(290, 194, 976, 538)
    # cabeçalho
    s += f'<div class="abs" style="left:310px;top:206px;width:42px;height:42px;border-radius:8px;background:#0a0a0b;box-shadow:inset 0 0 0 1px #2a2a2e;display:flex;align-items:center;justify-content:center;font-size:24px;font-weight:700;color:{CREAM}">{slotn}</div>'
    s += f'<div class="abs" style="left:358px;top:212px;width:4px;height:30px;border-radius:2px;background:{col};box-shadow:0 0 8px {col}"></div>'
    s += title(372, 206, name, 28)
    s += f'<div class="abs" style="left:374px;top:242px;font-size:13px;color:#a9a49c;letter-spacing:.6px">{tag}</div>'
    s += key(818, 212, 86, 34, '&#9664; PREV') + key(908, 212, 86, 34, 'NEXT &#9654;')
    s += key(1006, 212, 64, 34, 'ON', 'cream') + key(1082, 212, 168, 34, 'BACK TO RACK', 'red')
    # visor
    s += f'<div class="display" style="left:310px;top:268px;width:936px;height:308px;overflow:hidden"><div style="position:absolute;left:4px;top:4px;width:928px;height:300px">{VIZ[kind]()}</div></div>'
    # knobs grandes
    allp = params + [('MIX', mix, 0.22 if mix != '100 %' else 1)]
    n = len(allp)
    slot_w = 936 / n
    for i, (pn, pv, pos) in enumerate(allp):
        cx = 310 + slot_w * i + slot_w / 2
        s += lbl(cx - 70, 588, 140, pn, 15)
        s += knob(cx, 650, 70, pos)
        s += val(cx - 70, 694, 140, pv, 17)
        if i == n - 2: s += f'<div class="abs" style="left:{310+slot_w*(i+1)-1:.0f}px;top:594px;width:1px;height:118px;background:#2a2a2e"></div>'
    s += bottom_bar()
    page(s, f'{idx:02d}-{name.lower().replace(" / ", "-").replace(" ", "-")}')

main_screen()
for i, k in enumerate(ORDER, start=1):
    expanded(k, i)
print('ok', len(os.listdir(OUT)))

# ------------------------------------------------------------------ tela inicial: conversa com a IA (rack vazio)
def start_screen():
    s = left_column(preset='Empty Rack')
    s += panel(290, 14, 976, 718)
    s += title(316, 30, 'WHAT SOUND DO YOU WANT?', 30)
    s += key(1078, 30, 172, 36, 'SIGNAL CHAIN &#9654;', '', 15)
    s += f'<div class="abs" style="left:318px;top:76px;font-size:15px;color:#a9a49c;letter-spacing:.4px">Describe it the way you would tell your engineer — style, instrument, an artist reference, or a quick fix.</div>'
    # campo grande (3 linhas)
    s += f'<div class="display" style="left:312px;top:112px;width:932px;height:132px"></div>'
    s += f'<div class="abs" style="left:334px;top:128px;width:760px;font-size:27px;line-height:36px;font-weight:600;color:{CREAM}">voz de adoração bem espacial, com delay 1/8 pontuado e um brilho em cima, estilo Isadora Pompeo<span style="color:{RED}">|</span></div>'
    s += f'<div class="abs small" style="left:334px;top:220px">WRITE IN PORTUGUESE · ENTER TO CREATE</div>'
    s += key(1100, 186, 130, 46, 'CREATE', 'red', 19)
    # exemplos
    s += title(316, 262, 'TRY ONE', 18)
    ex = [('VOCAL', 'voz gospel com reverb de igreja'), ('VOCAL', 'voz trap estilo Travis Scott'), ('DRUMS', 'bateria suja e larga'),
          ('BASS', 'baixo gordo'), ('GUITAR', 'violão aberto e espacial'), ('MASTER', 'master gospel com pressão'),
          ('FX', 'efeito telefone'), ('FX', 'anos 80 sonhador'), ('?', 'me surpreenda')]
    for i, (tag, t) in enumerate(ex):
        x = 312 + (i % 3) * 312; y = 300 + (i // 3) * 54
        s += f'<div class="abs" style="left:{x}px;top:{y}px;width:304px;height:46px;border-radius:7px;background:linear-gradient(180deg,rgba(255,255,255,.05),rgba(0,0,0,.35)),#141416;box-shadow:inset 0 1px 0 rgba(255,255,255,.08),0 0 0 1px #000"></div>'
        s += f'<div class="abs" style="left:{x+12}px;top:{y+15}px;width:58px;height:17px;border-radius:3px;border:1px solid {RED};color:{RED};font-size:11px;font-weight:700;text-align:center;line-height:15px;letter-spacing:1px">{tag}</div>'
        s += f'<div class="abs" style="left:{x+80}px;top:{y+12}px;font-size:17px;color:{CREAM};font-weight:500">{t}</div>'
    # conversa
    s += title(316, 474, 'CONVERSATION', 18)
    s += f'<div class="display" style="left:312px;top:510px;width:932px;height:206px"></div>'
    msgs = [
        ('you', 'voz gospel com reverb de igreja'),
        ('OFFLINE BRAIN', 'Entendi: gospel · voz · igreja. Montei: EQ → De-Esser → Compressor → Saturação → Reverb (igreja, 3,2 s).'),
        ('you', 'menos reverb e mais brilho'),
        ('OFFLINE BRAIN', 'Reverb 22% → 15%; agudos +2,5 dB.'),
        ('you', 'quero que soe como se eu estivesse cantando sozinho numa caverna'),
        ('ADVANCED AI', 'Montei um eco de caverna escura: EQ com agudos cortados em 5 kHz, delay 1/4 com 55% de repetições e reverb catedral de 8 s.'),
    ]
    y = 522
    for who, txt in msgs:
        if who == 'you':
            w = 24 + len(txt) * 7.6
            s += f'<div class="abs" style="left:{1230-w}px;top:{y}px;width:{w}px;height:26px;border-radius:13px;background:#2a1213;border:1px solid #5a1a1c;font-size:15px;line-height:24px;text-align:center;color:{CREAM}">{txt}</div>'
        else:
            col = '#7cc4ff' if who == 'ADVANCED AI' else RED
            s += f'<div class="abs" style="left:328px;top:{y+4}px;width:110px;font-size:11px;font-weight:700;letter-spacing:1.3px;color:{col}">{who}</div>'
            s += f'<div class="abs" style="left:440px;top:{y+1}px;width:790px;font-size:15px;color:#d8d1c4;white-space:nowrap;overflow:hidden">{txt}</div>'
        y += 32
    s += bottom_bar()
    page(s, '00a-start-ai')

start_screen()
