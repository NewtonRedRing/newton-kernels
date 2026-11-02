#!/usr/bin/env python3
"""Generate an honest two-direction speed-ratio SVG. Below 1x means STL wins."""
import html
import json
import math
import sys
from pathlib import Path

def main(source, target):
    data=json.loads(Path(source).read_text())
    rows=data['results']
    if not rows or any(not r.get('verified_equal_output') for r in rows):
        raise ValueError('Refusing to chart unverified data')
    height=140+len(rows)*44
    midpoint=520
    scale=180/max(0.7,max(abs(math.log2(r['speedup'])) for r in rows))
    svg=[f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 1080 {height}" width="1080" height="{height}" role="img" aria-label="NewtonKernels vs C++ standard-library reference; blue bars right of 1x indicate Newton faster; ochre bars left indicate reference faster">',
         '<rect width="100%" height="100%" fill="#111827"/>',
         '<g font-family="system-ui, sans-serif">',
         '<text x="35" y="38" fill="#f8fafc" font-size="23" font-weight="700">Five-module benchmark • every result</text>',
         '<text x="35" y="63" fill="#94a3b8" font-size="13">Same input and process environment per pair • log2 speed ratio • synthetic workload • lower time is better</text>',
         f'<line x1="{midpoint}" y1="90" x2="{midpoint}" y2="{height-35}" stroke="#b5bdcd" stroke-width="2" stroke-dasharray="4 6"/>',
         f'<text x="{midpoint+7}" y="88" fill="#cbd5e1" font-size="12">1.00×</text>',
         '<text x="330" y="86" fill="#f8b454" font-size="12">← STL reference faster</text>',
         '<text x="720" y="86" fill="#5fd7dd" font-size="12">NewtonKernels faster →</text>']
    for i,r in enumerate(rows):
        y=103+i*44
        ratio=r['speedup']
        distance=abs(math.log2(ratio))*scale
        start=midpoint if ratio>=1 else midpoint-distance
        color='#4fcbd3' if ratio>=1 else '#e8a74c'
        svg += [f'<text x="35" y="{y+19}" fill="#e2e8f0" font-size="14">{html.escape(r["operation"].replace("_"," "))}</text>',
                f'<text x="315" y="{y+19}" fill="#94a3b8" font-size="12">{r["size"]:,}</text>',
                f'<rect x="{start:.2f}" y="{y+2}" width="{max(1,distance):.2f}" height="23" rx="4" fill="{color}"/>',
                f'<text x="{midpoint+235}" y="{y+20}" fill="{color}" font-size="14" font-weight="600">{ratio:.2f}×</text>',
                f'<text x="{midpoint+315}" y="{y+20}" fill="#cbd5e1" font-size="12">{r["newton_median_ms"]:.3f} / {r["competitor_median_ms"]:.3f} ms</text>']
    svg += [f'<text x="35" y="{height-12}" fill="#94a3b8" font-size="12">Source: results.json | three alternating fresh processes | seven inner repetitions | ratios below 1.0× are real reference wins</text>','</g>','</svg>']
    Path(target).write_text('\n'.join(svg)+'\n')

if __name__=='__main__':
    if len(sys.argv)!=3:raise SystemExit('usage: chart_extended.py results.json comparison.svg')
    main(sys.argv[1],sys.argv[2])
