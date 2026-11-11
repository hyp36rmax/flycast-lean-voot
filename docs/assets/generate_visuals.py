# SPDX-License-Identifier: GPL-2.0-or-later
"""Produce flat, self-contained SVG figures from retained evidence."""
from pathlib import Path
import json, hashlib, csv, re, html

A=Path(__file__).resolve().parent
R=A.parent.parent
M=json.loads((R/'docs/performance/measurements.json').read_text())
S={s['id']:s for s in M['studies']}
F=json.loads((A/'rendering/rd02d-submission.json').read_text())['counts']
assert sum(F['before']['materials'].values())==0
assert sum(F['after']['materials'].values())==576

INK='#172b3a'; MUTED='#425466'; BLUE='#17698a'; TEAL='#176e56'; ORANGE='#a5411e'
class Figure:
    def __init__(self,title,desc,height):
        self.h=height
        self.parts=[f'<svg xmlns="http://www.w3.org/2000/svg" width="720" height="{height}" viewBox="0 0 720 {height}" role="img" aria-labelledby="title desc">',
                    f'<title id="title">{html.escape(title)}</title><desc id="desc">{html.escape(desc)}</desc>',
                    '<style>text{font-family:Arial,Helvetica,sans-serif;fill:#172b3a} .minor{fill:#425466} .before{fill:#425466} .after{fill:#176e56} .warning{fill:#a5411e}</style>',
                    f'<rect width="720" height="{height}" rx="8" fill="#fff" stroke="#c8d1d9"/>']
        self.text(30,42,title,27,bold=True)
    def text(self,x,y,t,size=20,color=INK,bold=False,anchor='start'):
        self.parts.append(f'<text x="{x}" y="{y}" font-size="{size}" style="fill:{color}" font-weight="{600 if bold else 400}" text-anchor="{anchor}">{html.escape(str(t))}</text>')
    def lines(self,x,y,lines,size=20,step=28,**kw):
        for i,t in enumerate(lines): self.text(x,y+i*step,t,size,**kw)
    def rect(self,x,y,w,h,fill='none',stroke='#c8d1d9',dash=False,r=0):
        self.parts.append(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{r}" fill="{fill}" stroke="{stroke}" stroke-width="2"'+(' stroke-dasharray="7 5"' if dash else '')+'/>')
    def line(self,x1,y1,x2,y2,color='#91a0ad',dash=False):
        self.parts.append(f'<path d="M{x1},{y1} L{x2},{y2}" stroke="{color}" stroke-width="2" fill="none"'+(' stroke-dasharray="7 5"' if dash else '')+'/>')
    def arrow(self,x1,y1,x2,y2,color=MUTED):
        self.line(x1,y1,x2,y2,color)
        if y1==y2: p=f'{x2-8},{y2-5} {x2},{y2} {x2-8},{y2+5}'
        else: p=f'{x2-5},{y2-8} {x2},{y2} {x2+5},{y2-8}'
        self.parts.append(f'<polyline points="{p}" stroke="{color}" stroke-width="2" fill="none"/>')
    def save(self,path):
        p=A/path;p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text('\n'.join(self.parts+['</svg>'])+'\n',encoding='utf-8')

f=Figure('Hor+ projection and clipping','Equal-height, exact-ratio 4:3, 16:9 and 32:9 viewports. Guest submission and host clipping are separate stages.',1190)
f.lines(30,79,['Equal vertical framing; added horizontal field of view.','Rectangles show exact aspect ratios, not gameplay captures.'])
for y,w,label,extra in [(158,192,'4:3  •  original','No additional horizontal extent'),(370,256,'16:9  •  Hor+','4/3 of the original horizontal span'),(582,512,'32:9  •  Hor+','8/3 of the original horizontal span')]:
    x=(720-w)/2
    f.text(30,y-14,label,22,bold=True)
    f.rect(x,y,w,144,'#e8f3f7',BLUE)
    if w>192:f.rect(264,y,192,144,'#fff',MUTED,True)
    f.line(x-14,y,x+w+14,y,TEAL)
    f.line(x-14,y+144,x+w+14,y+144,TEAL)
    f.text(360,y+174,extra,20,anchor='middle')
f.lines(30,795,['Dashed: original horizontal span. Green: vertical limits.','Blue sides: wider projection / eligible horizontal clip.','Projection width = original width × aspect / (4/3).'],20,step=28)
f.rect(30,886,660,70,'#f4f7f9',MUTED,r=4)
f.text(48,914,'1  Guest SH-4 visibility decision',22,bold=True)
f.text(48,940,'Guarded VOOT limit-load correction → geometry submission',19)
f.arrow(360,958,360,979)
f.rect(30,983,660,70,'#e8f3f7',BLUE,r=4)
f.text(48,1011,'2  Host projection and PowerVR tile clipping',22,bold=True)
f.text(48,1037,'Eligible full-width clip widens; vertical bounds remain.',19)
f.lines(30,1090,['Partial-width, inverse and render-to-texture clips stay guarded.','A wider viewport cannot recover geometry the guest omitted.','Source: transform_matrix.cpp, rec_x64.cpp; clip fixtures.'],19,step=29)
f.save(Path('rendering/horplus-clipping.svg'))

f=Figure('Geometry returned to submission','Historical R&D-02D matched Solo replay: material groups total zero before and 576 submitted polygons after the visibility correction.',720)
f.lines(30,80,['R&D-02D  •  matched saved Solo replay  •  sample 2','DX11 / x64 diagnostics; 3840 × 1080 rendering.'])
f.text(30,163,'Submitted polygon records in three far-mesh material groups',20,bold=True)
f.text(30,208,'Before correction',21);f.text(680,208,'0',25,bold=True,anchor='end')
f.rect(30,226,660,12,'#f1f4f6','#f1f4f6')
f.text(30,278,'After correction',21);f.text(680,278,'576',25,bold=True,anchor='end',color=TEAL)
colors=[BLUE,TEAL,'#85621b'];x=30
for (key,value),c in zip(F['after']['materials'].items(),colors):
    width=660*value/600
    f.rect(x,296,width,36,c,c);x+=width
for i,((key,value),c) in enumerate(zip(F['after']['materials'].items(),colors)):
    y=396+i*37;f.rect(30,y-15,14,14,c,c);f.text(56,y,f'{key}   0 → {value} polygon records',20)
f.line(30,336,690,336)
for v in [0,200,400,600]:f.text(30+660*v/600,358,str(v),17,anchor='middle')
f.lines(30,506,['Linear scale starts at zero; upper bound 600 polygon records.','Whole sample: 1,687 → 2,411 polygons; these groups add 576.','The first two raw TA samples were byte-identical.','Groups identify local mesh evidence, not formal object IDs.','This is submission evidence, not an FPS or latency result.','Matched gameplay comparison: pending provenance and rights.'],20,step=32)
f.save(Path('rendering/visibility-submission.svg'))

f=Figure('Flycast Lean VOOT runtime boundary','Original Flycast emulation is retained. Lean source modifications and public hosted interfaces are distinguished from external application ownership.',1180)
f.lines(30,79,['Public runtime contract; no external application internals.','Scope: consolidated C++ source, Windows x64 / DX11.'])
f.rect(30,132,660,73,'#f4f7f9',MUTED,r=4)
f.text(48,161,'Original NAOMI 5.66 software',23,bold=True)
f.text(48,188,'User-supplied game and BIOS; not included in this repository.',19)
f.arrow(360,207,360,232)
f.rect(30,236,660,330,'#f4f7f9',MUTED,r=4)
f.text(48,269,'Original Flycast foundation  •  retained functionality',21,bold=True)
f.text(48,308,'SH-4 execution  →  guest commands and device activity',21)
for y,left,right in [(331,'PowerVR rendering','AICA audio'),(417,'JVS input','NAOMI cabinet comms')]:
    f.rect(48,y,297,66,'#fff',MUTED,r=4);f.rect(365,y,307,66,'#fff',MUTED,r=4)
    f.text(64,y+40,left,21);f.text(381,y+40,right,21)
f.text(48,530,'Native executable owns emulation, rendering and audio.',20)
f.rect(30,597,660,155,'#e8f3f7',BLUE,r=4)
f.text(48,629,'Flycast Lean VOOT  •  source modifications',22,bold=True)
f.lines(48,661,['Guarded Hor+ / visibility; DX11 / parser; SH-4 dispatch.','Cabinet readiness wait; distinct opt-in LAN transport.','Lean source selection and hosted runtime interfaces.'],20,step=29)
f.rect(30,783,660,105,'#e8f3f7',BLUE,r=4)
f.text(48,815,'Hosted interfaces  •  explicit integration boundary',21,bold=True)
f.lines(48,847,['Config/data directories; lifecycle events; versioned input.','Aspect V1: Solo preview/commit. LAN: endpoint contract.'],19,step=27)
f.arrow(360,890,360,915)
f.parts.append('<polyline points="355,898 360,890 365,898" stroke="#425466" stroke-width="2" fill="none"/>')
f.rect(30,919,660,84,'#fff',MUTED,True,r=4)
f.text(48,951,'External application responsibilities',22,bold=True)
f.text(48,981,'Configure paths, route input and own session processes.',20)
f.lines(30,1045,['Source-verified profiles  •  FLYCAST_OT_LAN_TRANSPORT','OFF: Solo (one process); Local VS (MASTER + SLAVE).','ON: LAN transport; hosted startup is required.','Diagram groups responsibilities; arrows are not a timing trace.'],20,step=31)
f.save(Path('architecture/runtime-interfaces.svg'))

def panel(f,y,title,values,maxv,unit,fmt,ticks):
    f.text(30,y,title,22,bold=True)
    f.text(30,y+28,unit+'  •  linear axis from zero',19,color=MUTED)
    x=145; width=390
    for i,(label,val,color) in enumerate(values):
        yy=y+53+i*53
        f.text(30,yy+24,label,21)
        f.rect(x,yy,width,30,'#f2f5f7','#f2f5f7')
        f.rect(x,yy,width*val/maxv,30,color,color)
        f.text(555,yy+24,fmt(val),20,bold=True)
    axis=y+153
    f.line(x,axis,x+width,axis)
    for val,label in ticks:
        xx=x+width*val/maxv;f.line(xx,axis,xx,axis+5);f.text(xx,axis+28,label,17,anchor='middle')

s=S['RD03D'];f=Figure('TA parser wall time','Historical R&D-03D palette-base retention study; mean and P99 wall time per frame. This is not a current consolidated-source benchmark.',770)
f.lines(30,80,['Historical R&D-03D  •  saved Solo battle at 3840 × 1080','Controls released; A/B/B/A; 120 warmup frames per run.','1,200 measured frames per variant.'])
for y,key,title in [(195,'mean','Mean'),(420,'p99','P99')]:
    panel(f,y,title,[('Before',s['before'][key],MUTED),('After',s['after'][key],TEAL)],1500,'µs/frame',lambda v:f'{v:,.3f}' if key=='mean' else f'{v:,.1f}',[(0,'0'),(500,'500'),(1000,'1,000'),(1500,'1,500')])
f.lines(30,666,['Lower parser cost in this workload; no whole-game FPS claim.','Not a benchmark of the current consolidated source.','Exact original CPU / GPU / OS linkage remains unverified.'],20,step=30)
f.save(Path('performance/rd03d-parser.svg'))

s=S['RD03E'];f=Figure('SH-4 dispatch: cost and cadence','Historical R&D-03E reduced emulation-thread mean cycles, while render-start P99 wall time increased. Separate axes and units.',795)
f.lines(30,80,['Historical R&D-03E  •  matched saved Solo at 3840 × 1080','120 warmup frames; 600 measured frames per variant.'])
panel(f,170,'Emulation thread mean',[('Before',s['before']['mean'],MUTED),('After',s['after']['mean'],TEAL)],22000000,'cycles/frame',lambda v:f'{v:,.0f}',[(0,'0'),(10000000,'10M'),(20000000,'20M')])
panel(f,405,'Render-start P99  •  counterexample',[('Before',s['renderStartP99BeforeMicroseconds']/1000,MUTED),('After',s['renderStartP99AfterMicroseconds']/1000,ORANGE)],30,'ms',lambda v:f'{v:.4f}',[(0,'0'),(10,'10'),(20,'20'),(30,'30')])
f.lines(30,654,['Cycles and elapsed time describe different host costs.','Render-start P99 worsened; no unconditional speed-up claim.','Historical workload, not current-source or GPU/FPS timing.','Exact original CPU / GPU / OS linkage remains unverified.'],20,step=31)
f.save(Path('performance/rd03e-dispatch.svg'))

s=S['RD04A'];f=Figure('NAOMI receive waiting: active cycles','Historical R&D-04A original cabinet transport. SLAVE and MASTER mean active thread cycles per sample, shown on separate labeled scales.',810)
f.lines(30,80,['Historical R&D-04A  •  linked original-aspect workload','120 warmup; 600 measured samples per cabinet.'])
panel(f,170,'SLAVE mean',[('Before',s['before']['mean'],MUTED),('After',s['after']['mean'],TEAL)],12000000,'active cycles/sample',lambda v:f'{v:,.0f}',[(0,'0'),(4000000,'4M'),(8000000,'8M'),(12000000,'12M')])
panel(f,405,'MASTER mean  •  separate role and scale',[('Before',s['masterMeanBefore'],MUTED),('After',s['masterMeanAfter'],ORANGE)],60000,'active cycles/sample',lambda v:f'{v:,.0f}',[(0,'0'),(20000,'20k'),(40000,'40k'),(60000,'60k')])
f.lines(30,654,['Role workloads differ; compare before/after within each panel.','Active CPU cycles exclude descheduled waiting.','No wall-clock latency or direct gameplay FPS result.','Original transport study; later LAN worker not benchmarked.','Exact original CPU / GPU / OS linkage remains unverified.'],20,step=29)
f.save(Path('performance/rd04a-link-wait.svg'))

print('Created six SVG figures from retained aggregate records.')
