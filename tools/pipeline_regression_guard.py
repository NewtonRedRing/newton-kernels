#!/usr/bin/env python3
"""CI regression gate for existing complete-file CSV pipeline implementations.

Compares candidate vs previous main on the *same runner* using identical
baseline-generated fixtures. Includes process startup, file mapping, parsing,
filtering, aggregation and output. No historical cross-machine ms comparison.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import math
from pathlib import Path
import statistics
import subprocess
import sys
import time


CASES = ((100_000, "owned"), (100_000, "fused"), (100_000, "fast"),
         (1_000_000, "owned"), (1_000_000, "fused"), (1_000_000, "fast"))


def invoke(binary: Path, implementation: str, fixture: Path) -> tuple[tuple, float]:
    started = time.perf_counter_ns()
    proc = subprocess.run(
        [str(binary.resolve(strict=True)), f"--impl={implementation}",
         f"--input={fixture.resolve(strict=True)}"],
        text=True, capture_output=True, timeout=180)
    duration_ms = (time.perf_counter_ns() - started) / 1e6
    if proc.returncode != 0:
        raise RuntimeError(
            f"{binary.name} {implementation}: exit {proc.returncode} {proc.stderr[:1500]}")
    obj = json.loads(proc.stdout)
    if not isinstance(obj, dict) or not all(k in obj for k in
                                            ("checksum", "items", "selected")):
        raise ValueError("Missing checksum/items/selected")
    if not isinstance(obj["checksum"], str) or not all(
            isinstance(obj[k], int) and obj[k] >= 0 for k in ("items", "selected")):
        raise ValueError("Invalid pipeline result")
    if not math.isfinite(duration_ms) or duration_ms <= 0:
        raise ValueError("Invalid measured process duration")
    return ((obj["checksum"], obj["items"], obj["selected"]), duration_ms)


def verdict(deltas: list[float], threshold: float, min_slow_rounds: int) -> str:
    if not deltas or any(type(x) not in (int, float) or
                         not math.isfinite(x) for x in deltas):
        raise ValueError("Missing/invalid paired samples")
    if statistics.median(deltas) > threshold and \
            sum(x > 0 for x in deltas) >= min_slow_rounds:
        return "FAIL"
    return "WARN" if statistics.median(deltas) > 0 else "PASS"


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--candidate", type=Path, required=True)
    p.add_argument("--base", type=Path, required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--rounds", type=int, default=7)
    p.add_argument("--max-regression-pct", type=float, default=3)
    p.add_argument("--min-slow-rounds", type=int, default=5)
    p.add_argument("--protected-code-identical", type=int, choices=(0, 1), default=0)
    args = p.parse_args()
    if not (5 <= args.rounds <= 50 and 1 <= args.min_slow_rounds <= args.rounds
            and 0 <= args.max_regression_pct <= 10):
        p.error("Invalid guard policy")
    args.out.mkdir(parents=True, exist_ok=True)
    try:
        binary = {"base": args.base.resolve(strict=True),
                  "candidate": args.candidate.resolve(strict=True)}
        fixtures = {}
        references = {}
        for size in (100_000, 1_000_000):
            fixture = (args.out / f"pipeline-{size}.csv").resolve()
            proc = subprocess.run(
                [str(binary["base"]), f"--save-fixture={fixture}", f"--rows={size}"],
                capture_output=True, text=True, timeout=180)
            if proc.returncode:
                raise RuntimeError(f"Baseline fixture failed: {proc.stderr}")
            fixtures[size] = fixture
            references[size] = invoke(binary["base"], "owned", fixture)[0]
        rows = []
        for size, impl in CASES:
            samples = {"base": [], "candidate": []}
            deltas = []
            fixture = fixtures[size]
            for phase in ("base", "candidate"):  # warm up without timing
                for _ in range(2):
                    signature, _ = invoke(binary[phase], impl, fixture)
                    if signature != references[size]:
                        raise ValueError(f"CORRECTNESS FAIL {size}/{impl} on {phase}")
            for cycle in range(args.rounds):
                order = ("base", "candidate", "candidate", "base") if cycle % 2 == 0 \
                    else ("candidate", "base", "base", "candidate")
                round_samples = {"base": [], "candidate": []}
                for rev in order:
                    signature, elapsed = invoke(binary[rev], impl, fixture)
                    if signature != references[size]:
                        raise ValueError(f"CORRECTNESS FAIL {size}/{impl} on {rev}")
                    samples[rev].append(elapsed)
                    round_samples[rev].append(elapsed)
                deltas.append(
                    statistics.median(round_samples["candidate"]) /
                    statistics.median(round_samples["base"]) - 1.0)
            result = verdict(deltas, args.max_regression_pct / 100,
                             args.min_slow_rounds)
            if args.protected_code_identical and result == "FAIL":
                result = "WARN"  # same protected code, noisy VM timing
            rows.append({
                "rows": size, "implementation": impl, "status": result,
                "median_paired_change_pct": round(statistics.median(deltas) * 100, 4),
                "slower_rounds": sum(x > 0 for x in deltas),
                "deltas_pct": [round(x * 100, 4) for x in deltas],
                "base_median_ms": statistics.median(samples["base"]),
                "candidate_median_ms": statistics.median(samples["candidate"]),
                "samples_ms": samples, "correctness": references[size],
            })
            print(f"{size:>8} {impl:<5}: {result} "
                  f"paired {statistics.median(deltas)*100:+.2f}%", flush=True)
        report = {
            "utc": datetime.now(timezone.utc).isoformat(),
            "fixture_source": "previous-main source revision",
            "measurement": "process wall time, shared fixture, ABBA/BAAB, paired",
            "policy": vars(args) | {"candidate": str(args.candidate),
                                    "base": str(args.base),
                                    "out": str(args.out)},
            "results": rows,
        }
        report["policy"]["rounds"] = args.rounds
        (args.out / "pipeline-regression-results.json").write_text(
            json.dumps(report, indent=2, default=str) + "\n")
        lines = ["# Existing CSV pipeline regression guard", "",
                 "Both revisions process the exact same baseline-generated CSV files.",
                 "Positive change means the candidate is slower.", "",
                 "| Rows | Algorithm | Paired change | Decision |",
                 "|---:|---|---:|---|"]
        for row in rows:
            lines.append(f"| {row['rows']:,} | {row['implementation']} | "
                         f"{row['median_paired_change_pct']:+.2f}% | {row['status']} |")
        lines += ["", "All six output signatures were exact-match validated.",
                  "Raw per-process timing samples are in pipeline-regression-results.json.",
                  "GitHub shared-runner noise limits detection of very small regressions.",
                  ""]
        (args.out / "pipeline-regression-REPORT.md").write_text("\n".join(lines))
        if any(row["status"] == "FAIL" for row in rows):
            print("BLOCKED: one or more established CSV pipeline cases regressed",
                  file=sys.stderr)
            return 1
        print("CSV PIPELINE GUARD PASSED: six established cases checked")
        return 0
    except Exception as exc:
        (args.out / "pipeline-regression-FAILURE.txt").write_text(
            f"{type(exc).__name__}: {exc}\n")
        print(f"CSV PIPELINE GUARD FAILED CLOSED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
