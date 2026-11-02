#!/usr/bin/env python3
"""Run all five additional module comparisons, reporting wins and losses equally.

Reuses the existing process-isolated runner; no third-party source is copied.
"""
import argparse
import json
from pathlib import Path
import statistics
import sys
import run as base

# (units per fixture, extra work parameter). All fixtures use identical seeds.
CASES = {
    "core_features": [(2000, 1), (200000, 1)],
    "memory_align": [(1024, 64), (131072, 16)],
    "memory_map": [(1 << 20, 1), (16 << 20, 1)],
    "index_query": [(4000, 80), (12000, 120)],
    "index_build": [(1000, 24), (4000, 60)],
    "analytics_group": [(20000, 128), (200000, 1024)],
    "analytics_join": [(3000, 512), (12000, 2048)],
    "parallel_tasks": [(50, 64), (20000, 64)],
}
DISCLOSURES = {
    "core_features": "CPU capability discovery vs identical compiler builtins; x86 GCC/Clang only. No actual vectorized computation.",
    "memory_align": "Both allocate 64-byte aligned buffers, initialize and sample identical elements; includes allocation and deallocation.",
    "memory_map": "Read-only mmap vs 64KiB buffered std::ifstream, including open, read and cleanup. Input file generated off-clock; warm OS page cache.",
    "index_query": "Same ASCII positional inverted index semantics, 2-term AND, full result digest; both build indexes off-clock.",
    "index_build": "Both build positional indexes and answer 2-term AND queries within the timer; baseline uses STL postings with term positions.",
    "analytics_group": "Group-count API vs a reserved and manually optimized std::unordered_map; sorting/digest included for both.",
    "analytics_join": "Multimap hash join vs unordered_map-of-vectors hash join with exact duplicate multiplicity; unordered checksum.",
    "parallel_tasks": "Four persistent Executor workers vs std::async(std::launch::async) in bounded groups of four; identical task computation.",
}
UNITS = {
    "core_features": "feature probes", "memory_align": "buffer allocations", "memory_map": "bytes",
    "index_query": "AND queries", "index_build": "documents indexed", "analytics_group": "rows grouped",
    "analytics_join": "left-side input rows", "parallel_tasks": "tasks completed",
}

def units(op, size, queries):
    if op in {"core_features", "memory_map", "index_build", "analytics_group", "analytics_join"}: return size
    if op == "memory_align": return queries
    return queries

def markdown(rows, env):
    out = ["# NewtonKernels: five-module external-reference benchmarks", "",
           "> Every result is reported, including cases where the C++ standard-library baseline is faster.",
           "> Paired implementations were tested in alternating fresh processes with identical fixture seeds.", "",
           "## Environment", "", f"- UTC: {env['utc_timestamp']}",
           f"- Host: {env['cpu_model']} / {env['logical_cpus']} logical processors",
           f"- OS: {env['platform']}", f"- Compiler: {env['compiler']}",
           f"- Affinity: {env['cpu_affinity']}", f"- Governor: {env['cpu_governor']}",
           f"- Virtualization: {env['virtualization']}", "", "## All results", "",
           "| Module / operation | Size | Newton (ms) | STL reference (ms) | Newton ratio | Faster | Equal digest |",
           "|---|---:|---:|---:|---:|---|---|"]
    for row in rows:
        ratio = row['speedup']
        faster = 'Newton' if ratio > 1.02 else ('STL baseline' if ratio < .98 else 'Within 2%')
        out.append(f"| {row['operation']} | {row['size']:,} | {row['newton_median_ms']:.5f} | {row['competitor_median_ms']:.5f} | {ratio:.2f}× | {faster} | Yes |")
    out += ["", "## Complete supported resource evidence", "",
            "| Operation | Size | Newton RSS peak (KiB) | STL RSS peak (KiB) | Newton min–max (ms) | STL min–max (ms) |",
            "|---|---:|---:|---:|---|---|"]
    for row in rows:
        a=row['samples']['newton'];b=row['samples']['stl']
        rng=lambda x: f"{min(s['minimum_ms'] for s in x):.4f}–{max(s['maximum_ms'] for s in x):.4f}"
        out.append(f"| {row['operation']} | {row['size']:,} | {max(s['peak_process_rss_kib'] for s in a)} | {max(s['peak_process_rss_kib'] for s in b)} | {rng(a)} | {rng(b)} |")
    out += ["", "## Operational definitions and limitations", ""]
    for key, desc in DISCLOSURES.items(): out.append(f"- **{key}:** {desc}")
    out += ["", "**Important:** 'STL reference' means an original C++ baseline built from standard facilities; it is not a third-party library benchmark or a market ranking.",
            "Peak RSS and CPU resource data include runtime and fixture preparation, not incremental allocation attributed to each library.",
            "Process-wide resource counters are available in `results.json`; hardware cache/IPC counters, energy and isolated allocation counts are **not measured**.",
            "The timings are synthetic warm-cache benchmarks, not production throughput, full feature equivalence or cold-start tests.",
            "Build-source code is original. No competitor source was imported. Run `run_extended.py` again on your machine and compare pairs within the same environment.", ""]
    return '\n'.join(out)

def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--binary',type=Path,required=True)
    p.add_argument('--out',type=Path,default=Path('build-ext/extended'))
    p.add_argument('--rounds',type=int,default=3)
    p.add_argument('--repetitions',type=int,default=7)
    p.add_argument('--seed',type=int,default=1332857405)
    p.add_argument('--quick',action='store_true')
    p.add_argument('--operation',choices=['all',*CASES],default='all')
    a=p.parse_args()
    if a.rounds<2 or a.repetitions<3: p.error('need at least 2 rounds and 3 repetitions')
    binary=a.binary.resolve(strict=True)
    env=base.environment(binary)
    env['benchmark_campaign']='extended-five-modules'
    entries=[]
    for case, sizes in CASES.items():
        if a.operation not in ('all',case):continue
        for size,queries in sizes[:1] if a.quick else sizes:
            samples={'newton':[],'stl':[]}
            for round_idx in range(a.rounds):
                order=('newton','stl') if round_idx%2==0 else ('stl','newton')
                for impl in order:
                    sample=base.execute(binary,case,impl,size,queries,a.seed,a.repetitions)
                    samples[impl].append(sample)
            signatures={(s['checksum'],s['items'],s['input_bytes']) for seq in samples.values() for s in seq}
            if len(signatures)!=1:raise RuntimeError(f'output mismatch: {case} {size}: {signatures}')
            n=statistics.median(s['median_ms'] for s in samples['newton'])
            b=statistics.median(s['median_ms'] for s in samples['stl'])
            if n<=0 or b<=0:raise RuntimeError('Invalid measured timing')
            row={'operation':case,'module':case.split('_')[0] if not case.startswith('analytics_') else 'analytics',
                 'size':size,'queries':queries,'throughput_units':units(case,size,queries),
                 'throughput_unit_label':UNITS[case],
                 'competitor':'stl','competitor_revision':'Original C++ reference using standard-library facilities, same compiler and binary',
                 'seed':a.seed,'rounds':a.rounds,'repetitions':a.repetitions,
                 'newton_median_ms':n,'competitor_median_ms':b,'speedup':b/n,
                 'checksum':samples['newton'][0]['checksum'],'verified_equal_output':True,
                 'samples':samples,'limitations':DISCLOSURES[case]}
            entries.append(row)
            print(f"{case:17s} {size:>9}: Newton {n:.5f} ms, STL {b:.5f} ms, {b/n:.2f}x; MATCH",flush=True)
    a.out.mkdir(parents=True,exist_ok=True)
    (a.out/'results.json').write_text(json.dumps({'environment':env,'results':entries},indent=2)+'\n')
    (a.out/'REPORT.md').write_text(markdown(entries,env))
    print('Published:',a.out.resolve())

if __name__=='__main__':main()
