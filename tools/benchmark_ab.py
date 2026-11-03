#!/usr/bin/env python3
"""Alternating, checksum-verified A/B process benchmark. Stdlib only.

Each program must print one JSON object containing 'checksum' (or 'digest')
and optionally 'items'. Both commands run on the same host. Wall time includes
process startup, work and JSON output. No automatic machine tuning.
"""
import argparse
import json
import math
import os
import shlex
import statistics
import subprocess
import sys
import time
from pathlib import Path

def run(command, timeout):
    start = time.perf_counter_ns()
    completed = subprocess.run(shlex.split(command), text=True,
                               capture_output=True, timeout=timeout, check=False)
    elapsed = (time.perf_counter_ns() - start) / 1_000_000
    if completed.returncode:
        raise RuntimeError(f"{command}: exit {completed.returncode}: {completed.stderr[-1500:]}")
    try:
        payload = json.loads(completed.stdout.strip())
    except json.JSONDecodeError as error:
        raise RuntimeError(f"{command}: stdout must be a single JSON object") from error
    if not isinstance(payload, dict):
        raise RuntimeError("Benchmark output must be an object")
    checksum = payload.get("checksum", payload.get("digest"))
    if checksum is None:
        raise RuntimeError("Benchmark output missing checksum/digest")
    return {"elapsed_ms": elapsed, "checksum": str(checksum),
            "items": payload.get("items", None)}

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--before", required=True, help="Quoted command with argv")
    p.add_argument("--after", required=True, help="Quoted command with argv")
    p.add_argument("--cycles", type=int, default=12)
    p.add_argument("--warmup", type=int, default=2)
    p.add_argument("--timeout", type=float, default=180)
    p.add_argument("--output", required=True)
    a = p.parse_args()
    if a.cycles <= 0 or a.warmup < 0 or a.timeout <= 0:
        p.error("cycles and timeout must be positive; warmup nonnegative")
    for _ in range(a.warmup):
        run(a.before, a.timeout)
        run(a.after, a.timeout)
    samples = {"before": [], "after": []}
    expected = None
    for cycle in range(a.cycles):
        sequence = ("before", "after", "after", "before") if cycle % 2 == 0 \
                   else ("after", "before", "before", "after")
        for label in sequence:
            result = run(getattr(a, label), a.timeout)
            canonical = (result["checksum"], result["items"])
            if expected is None:
                expected = canonical
            elif canonical != expected:
                raise RuntimeError(f"Output mismatch: {canonical!r} != {expected!r}")
            samples[label].append(result["elapsed_ms"])
    out = {
        "schema_version": 1,
        "before_command": a.before,
        "after_command": a.after,
        "cycles": a.cycles, "warmup": a.warmup,
        "checksum": expected[0], "items": expected[1],
        "measurement": "whole process wall time, ms, alternating ABBA/BAAB",
        "hostname": os.uname().nodename if hasattr(os, "uname") else None,
        "python": sys.version,
        "samples_ms": samples,
        "median_ms": {k: statistics.median(v) for k, v in samples.items()},
        "min_ms": {k: min(v) for k, v in samples.items()},
        "max_ms": {k: max(v) for k, v in samples.items()},
    }
    out["speedup_before_over_after"] = (
        out["median_ms"]["before"] / out["median_ms"]["after"])
    path = Path(a.output)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(out, indent=2, sort_keys=True) + "\n")
    print(json.dumps({k: out[k] for k in ("median_ms", "speedup_before_over_after", "checksum")}, indent=2))

if __name__ == "__main__":
    main()
