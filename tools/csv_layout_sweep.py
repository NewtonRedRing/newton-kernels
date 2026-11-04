#!/usr/bin/env python3
"""Diagnostic-only CSV parser layout sweep, separate from mandatory guards."""
from __future__ import annotations
import argparse
from datetime import datetime,timezone
import hashlib
import json
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import sys

ANCHOR="a3ccc65021181561aec1c8fe284bdeb7c8e1234c"
SEED=1332857405

def pctl(samples, fraction):
    values=sorted(samples)
    pos=(len(values)-1)*fraction
    idx=int(pos)
    return values[idx]+(values[min(idx+1,len(values)-1)]-values[idx])*(pos-idx)

def trial(binary, rows):
    command=[str(binary.resolve(strict=True)), "--operation","csv",
             "--implementation","newton","--size",str(rows),
             "--queries","1","--seed",str(SEED),"--repetitions","11"]
    result=subprocess.run(command,capture_output=True,text=True,timeout=180,check=True)
    d=json.loads(result.stdout)
    if (d.get("operation"),d.get("implementation"),d.get("size"),
        d.get("queries"),d.get("seed"),d.get("repetitions"))!=(
        "csv","newton",rows,1,SEED,11):
        raise ValueError("Fixture or output schema changed")
    if not all(isinstance(d.get(k),(int,float)) and
               math.isfinite(d[k]) and d[k]>0 for k in
               ("median_ms","minimum_ms","maximum_ms")):
        raise ValueError("Invalid benchmark timer")
    if d["median_ms"]<d["minimum_ms"] or d["maximum_ms"]<d["median_ms"]:
        raise ValueError("Invalid min/median/max order")
    return d

def main():
    p=argparse.ArgumentParser()
    p.add_argument("--binary",action="append",nargs=2,metavar=("LABEL","FILE"),required=True)
    p.add_argument("--rounds",type=int,default=24)
    p.add_argument("--out",type=Path,required=True)
    args=p.parse_args()
    if not 12 <= args.rounds <= 120:
        p.error("Rounds must be between 12 and 120")
    entries=dict((label,Path(path)) for label,path in args.binary)
    if len(entries)!=len(args.binary) or "main_a" not in entries or "main_b" not in entries or "anchor" not in entries:
        p.error("Requires unique main_a/main_b/anchor labels")
    if entries["main_a"].resolve()!=entries["main_b"].resolve():
        p.error("A/A labels must point to exactly the same binary path")
    args.out.mkdir(parents=True,exist_ok=True)
    report={
        "generated_utc":datetime.now(timezone.utc).isoformat(),
        "reference_commit":ANCHOR,
        "methodology":"Randomized complete blocks; same-executable A/A; 11 inner benchmark repetitions per process",
        "caveats":[
            "p95/p99 below describe distributions of outer process medians, NOT individual-call tail latency.",
            "Linux GitHub VMs have uncontrolled physical CPU neighbors/frequency.",
            "This research is not a required merge gate and cannot waive a failed official benchmark guard."
        ],
        "binaries_sha256":{label:hashlib.sha256(path.read_bytes()).hexdigest()
                           for label,path in entries.items()},
        "results":[]
    }
    for size in (10000,100000):
        labels=list(entries)
        sample_data={label:[] for label in labels}
        expected=None
        for cycle in range(args.rounds):
            order=labels[:]
            random.Random(SEED+size+cycle*13).shuffle(order)
            for label in order:
                d=trial(entries[label],size)
                signature=(d["checksum"],d["items"],d["input_bytes"])
                if expected is None:
                    expected=signature
                if signature!=expected:
                    raise RuntimeError(f"CSV checksum mismatch: {size}, {label}")
                sample_data[label].append(d)
        aa=[b["median_ms"]/a["median_ms"]-1
            for a,b in zip(sample_data["main_a"],sample_data["main_b"])]
        ratios={label:[sample["median_ms"]/base["median_ms"]-1
                         for sample,base in zip(samples,sample_data["main_a"])]
                for label,samples in sample_data.items() if label!="main_a"}
        row={"rows":size,"output_signature":expected,"samples":sample_data,
             "main_aa_median_pct":statistics.median(aa)*100,
             "comparisons_to_main":{}}
        for label,deltas in ratios.items():
            times=[sample["median_ms"] for sample in sample_data[label]]
            row["comparisons_to_main"][label]={
                "paired_median_change_pct":statistics.median(deltas)*100,
                "round_changes_pct":[100*d for d in deltas],
                "p50_process_median_ms":pctl(times,.5),
                "p95_process_median_ms":pctl(times,.95),
                "p99_process_median_ms":pctl(times,.99),
                "throughput_rows_per_sec":size/(pctl(times,.5)/1000),
            }
        report["results"].append(row)
        print(f"CSV {size}: A/A={row['main_aa_median_pct']:+.2f}%")
        for label,values in row["comparisons_to_main"].items():
            print(f"  {label}: paired delta={values['paired_median_change_pct']:+.2f}% "
                  f"p99(process median)={values['p99_process_median_ms']:.3f}ms")
    (args.out/"csv-layout-sweep.json").write_text(json.dumps(report,indent=2)+"\n",encoding="utf8")

if __name__=="__main__":
    try:main()
    except Exception as e:
        print(f"CSV layout research failed closed: {e}",file=sys.stderr)
        raise
