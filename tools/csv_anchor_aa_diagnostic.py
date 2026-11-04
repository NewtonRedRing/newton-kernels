#!/usr/bin/env python3
"""Read-only A/A and paired A/B CSV benchmark isolation, never a merge gate."""
from __future__ import annotations

import argparse
import hashlib
import itertools
import json
from pathlib import Path
import platform
import random
import statistics
import subprocess
import sys
from datetime import datetime, timezone

ANCHOR = "a3ccc65021181561aec1c8fe284bdeb7c8e1234c"
SEED = 1332857405


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def invoke(binary: Path, size: int) -> dict:
    cmd = [
        str(binary.resolve(strict=True)),
        "--operation", "csv",
        "--implementation", "newton",
        "--size", str(size),
        "--queries", "1",
        "--seed", str(SEED),
        "--repetitions", "11",
    ]
    result = subprocess.run(cmd, capture_output=True, text=True,
                            timeout=180, check=True)
    data = json.loads(result.stdout)
    assert data["operation"] == "csv"
    assert data["implementation"] == "newton"
    assert data["size"] == size
    assert data["repetitions"] == 11
    assert data["seed"] == SEED
    assert data["median_ms"] > 0
    assert data["minimum_ms"] > 0
    assert data["input_bytes"] > 0
    return data


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--current", type=Path, required=True)
    p.add_argument("--anchor", type=Path, required=True)
    p.add_argument("--candidate", type=Path, help="Optional candidate built from PR head")
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--base-sha", required=True)
    p.add_argument("--rounds", type=int, default=12)
    args = p.parse_args()
    labels = 4 if args.candidate is not None else 3
    period = 24 if labels == 4 else 6
    if args.rounds < period or args.rounds % period:
        p.error(f"--rounds must be a positive multiple of {period}")
    args.out.mkdir(parents=True, exist_ok=True)
    try:
        # SAME executable invoked twice under two labels. This is a true A/A
        # control, not two differently compiled but source-identical builds.
        binaries = {"main_a": args.current, "main_b": args.current,
                    "anchor": args.anchor}
        if args.candidate is not None:
            binaries["candidate"] = args.candidate
        report = {
            "note": "Diagnostic-only A/A and anchor A/B. Never a CI merge gate.",
            "timestamp_utc": datetime.now(timezone.utc).isoformat(),
            "platform": platform.platform(),
            "processor": platform.processor(),
            "base_sha": args.base_sha,
            "anchor_sha": ANCHOR,
            "binaries_sha256": {k: sha256(v) for k,v in binaries.items()},
            "results": [],
        }
        for size in (10_000, 100_000):
            orders = list(itertools.permutations(binaries))
            random.Random(SEED + size).shuffle(orders)
            samples = {k: [] for k in binaries}
            signatures = set()
            for i in range(args.rounds):
                order = orders[i % len(orders)]
                for label in order:
                    data = invoke(binaries[label], size)
                    signatures.add((data["checksum"], data["items"],
                                    data["input_bytes"]))
                    samples[label].append(data)
                if len(signatures) != 1:
                    raise ValueError("CSV output signature disagreement")
            aa = [b["median_ms"]/a["median_ms"] - 1.0
                  for a,b in zip(samples["main_a"],samples["main_b"])]
            anchor = [c["median_ms"]/a["median_ms"] - 1.0
                      for a,c in zip(samples["main_a"],samples["anchor"])]
            record = {
                "rows": size,
                "signature": next(iter(signatures)),
                "rounds": args.rounds,
                "aa_main_b_vs_main_a_median_pct": 100*statistics.median(aa),
                "anchor_vs_main_a_median_pct": 100*statistics.median(anchor),
                "aa_round_deltas_pct": [100*x for x in aa],
                "anchor_round_deltas_pct": [100*x for x in anchor],
                "samples": samples,
            }
            if args.candidate is not None:
                candidate_vs_main = [
                    p["median_ms"]/m["median_ms"] - 1.0
                    for m,p in zip(samples["main_a"], samples["candidate"])
                ]
                candidate_vs_anchor = [
                    p["median_ms"]/a["median_ms"] - 1.0
                    for a,p in zip(samples["anchor"], samples["candidate"])
                ]
                record.update({
                    "candidate_vs_main_median_pct": 100*statistics.median(candidate_vs_main),
                    "candidate_vs_anchor_median_pct": 100*statistics.median(candidate_vs_anchor),
                    "candidate_vs_main_round_deltas_pct": [100*x for x in candidate_vs_main],
                    "candidate_vs_anchor_round_deltas_pct": [100*x for x in candidate_vs_anchor],
                })
            report["results"].append(record)
            candidate_msg = (
                f" candidate/main={record['candidate_vs_main_median_pct']:+.3f}%"
                f" candidate/anchor={record['candidate_vs_anchor_median_pct']:+.3f}%"
                if args.candidate is not None else ""
            )
            print(f"csv:{size}: main/main A/A={record['aa_main_b_vs_main_a_median_pct']:+.3f}% "
                  f"anchor/main={record['anchor_vs_main_a_median_pct']:+.3f}%"
                  + candidate_msg, flush=True)
        (args.out/"csv-aa-diagnostic.json").write_text(
            json.dumps(report, indent=2)+"\n", encoding="utf-8")
        return 0
    except Exception as exc:
        (args.out/"csv-aa-diagnostic-FAILURE.txt").write_text(
            f"{type(exc).__name__}: {exc}\n", encoding="utf-8")
        print(f"CSV A/A DIAGNOSTIC FAILED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
