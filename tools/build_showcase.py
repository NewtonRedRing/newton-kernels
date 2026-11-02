#!/usr/bin/env python3
"""Generate auditable showcase inputs from committed benchmark evidence. No fake rows."""
import argparse
import json
import shutil
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SOURCE = ROOT / 'benchmarks/results/github-actions-2026-11-02-5889b44/results.json'
DESTINATION = ROOT / 'showcase/data/results.json'
EXTRA_SOURCE = ROOT / 'benchmarks/results/local-extended-2026-11-02/results.json'
EXTRA_DESTINATION = ROOT / 'showcase/data/extended-results.json'

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--check', action='store_true', help='Fail if the checked-in showcase data are stale')
    opts = p.parse_args()
    source = json.loads(SOURCE.read_text(encoding='utf-8'))
    if not source.get('results') or not all(row.get('verified_equal_output') for row in source['results']):
        raise SystemExit('Benchmark evidence must contain verified comparison pairs')
    for row in source['results']:
        competitor = row['competitor']
        if not row['samples'].get('newton') or not row['samples'].get(competitor):
            raise SystemExit(f'Incomplete raw samples: {row["operation"]} {row["size"]}')
        if abs(row['speedup'] - row['competitor_median_ms']/row['newton_median_ms']) > 1e-7:
            raise SystemExit('Inconsistent speedup calculation')
    extended = json.loads(EXTRA_SOURCE.read_text(encoding='utf-8'))
    if len(extended.get('results', [])) != 16 or any(not x.get('verified_equal_output') for x in extended['results']):
        raise SystemExit('Expected 16 verified five-module pairs')
    serialized = json.dumps(source, indent=2, ensure_ascii=False) + '\n'
    extended_serialized = json.dumps(extended, indent=2, ensure_ascii=False) + '\n'
    if opts.check:
        if not DESTINATION.is_file() or DESTINATION.read_text(encoding='utf-8') != serialized or not EXTRA_DESTINATION.is_file() or EXTRA_DESTINATION.read_text(encoding='utf-8') != extended_serialized:
            raise SystemExit('Stale showcase data: run python3 tools/build_showcase.py')
        print(f'Validated {len(source["results"])} verified pairs; showcase data current')
    else:
        DESTINATION.parent.mkdir(parents=True, exist_ok=True)
        DESTINATION.write_text(serialized, encoding='utf-8')
        EXTRA_DESTINATION.write_text(extended_serialized, encoding='utf-8')
        print(f'Generated {DESTINATION.relative_to(ROOT)} from verified raw evidence')

if __name__ == '__main__':
    main()
