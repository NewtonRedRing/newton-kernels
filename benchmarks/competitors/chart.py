#!/usr/bin/env python3
"""Render a compact static SVG from verified measured results (standard library)."""
import argparse
import html
import json
from pathlib import Path


def render(src: Path, dst: Path):
    document = json.loads(src.read_text())
    entries = document['results']
    if not entries or any(not e.get('verified_equal_output') for e in entries):
        raise ValueError('Refusing to chart absent or unverified comparisons')
    canvas_h = 130 + len(entries) * 57
    canvas_w = 920
    scale = 530 / max(1, max(x['speedup'] for x in entries))
    esc = html.escape
    svg = [f'<svg xmlns="http://www.w3.org/2000/svg" role="img" width="{canvas_w}" height="{canvas_h}" viewBox="0 0 {canvas_w} {canvas_h}">',
           '<rect width="100%" height="100%" fill="#ffffff"/>',
           '<text x="24" y="36" font-family="sans-serif" font-size="22" fill="#222">NewtonKernels vs external C++ implementations</text>',
           '<text x="24" y="60" font-family="sans-serif" font-size="12" fill="#555">Speedup on identical inputs and one CI runner • higher is better • not a universal ranking</text>',
           '<line x1="265" y1="85" x2="265" y2="'+str(canvas_h-40)+'" stroke="#999"/>']
    for i, r in enumerate(entries):
        y = 100 + 57*i
        category = f"{r['operation'].upper()} {r['size']:,}"
        label = f"{r['competitor']}"
        width = max(2, float(r['speedup'])*scale)
        svg += [
            f'<text x="24" y="{y+17}" font-family="sans-serif" font-size="14" fill="#222">{esc(category)}</text>',
            f'<text x="24" y="{y+35}" font-family="sans-serif" font-size="11" fill="#666">vs {esc(label)}</text>',
            f'<rect x="265" y="{y}" rx="3" width="{width:.1f}" height="29" fill="#384c69"/>',
            f'<text x="{min(820, 275+width):.1f}" y="{y+19}" font-family="sans-serif" font-size="15" fill="#222">{r["speedup"]:.2f}×</text>',
        ]
    svg += [f'<text x="24" y="{canvas_h-16}" font-family="sans-serif" font-size="11" fill="#555">Source: results.json • alternating process order • equality checked per fixture • GCC Release</text>', '</svg>']
    dst.write_text('\n'.join(svg) + '\n')

if __name__ == '__main__':
    ap=argparse.ArgumentParser()
    ap.add_argument('json',type=Path)
    ap.add_argument('svg',type=Path)
    args=ap.parse_args()
    render(args.json,args.svg)
