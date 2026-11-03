#!/usr/bin/env python3
"""Fail closed on reproducible performance regressions against two frozen references.

Runs the original 22 NewtonKernels cases on the *same runner* from three
independently compiled revisions: candidate, merge base, and the 2026-11-02
published-evidence commit. All fixtures, parameters, and expected outputs are
validated across revisions. No third-party benchmark dependencies are needed.
"""
from __future__ import annotations

import argparse
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import sys

PUBLISHED_ANCHOR = "a3ccc65021181561aec1c8fe284bdeb7c8e1234c"
SEED = 1332857405

# Do NOT derive the protected fixture list from candidate code or new runner
# scripts. Extending this fixed manifest requires a deliberate policy review.
CLASSIC = {
    "scan": ((1 << 20, 1), (16 << 20, 1)),
    "graph": ((5000, 32), (20000, 64)),
    "csv": ((10000, 1), (100000, 1)),
}
EXTENDED = {
    "core_features": ((2000, 1), (200000, 1)),
    "memory_align": ((1024, 64), (131072, 16)),
    "memory_map": ((1 << 20, 1), (16 << 20, 1)),
    "index_query": ((4000, 80), (12000, 120)),
    "index_build": ((1000, 24), (4000, 60)),
    "analytics_group": ((20000, 128), (200000, 1024)),
    "analytics_join": ((3000, 512), (12000, 2048)),
    "parallel_tasks": ((50, 64), (20000, 64)),
}


@dataclass(frozen=True)
class Case:
    suite: str
    operation: str
    size: int
    queries: int

    @property
    def key(self) -> str:
        return f"{self.operation}:{self.size}:{self.queries}"


def cases() -> list[Case]:
    fixtures = [Case("classic", op, size, queries)
                for op, sizes in CLASSIC.items() for size, queries in sizes]
    fixtures += [Case("extended", op, size, queries)
                 for op, sizes in EXTENDED.items() for size, queries in sizes]
    assert len(fixtures) == 22, "The existing 22 cases must never silently disappear"
    assert len({case.key for case in fixtures}) == 22
    return fixtures


def measure(binary: Path, case: Case, repetitions: int, seed: int) -> dict:
    cmd = [
        str(binary.resolve(strict=True)), "--operation", case.operation,
        "--implementation", "newton", "--size", str(case.size),
        "--queries", str(case.queries), "--seed", str(seed),
        "--repetitions", str(repetitions),
    ]
    completed = subprocess.run(cmd, text=True, capture_output=True, timeout=180,
                               check=False)
    if completed.returncode:
        raise RuntimeError(f"{case.key}: {' '.join(cmd)} returned "
                           f"{completed.returncode}: {completed.stderr[:1500]}")
    try:
        result = json.loads(completed.stdout)
    except json.JSONDecodeError as error:
        raise ValueError(f"{case.key}: invalid JSON from {binary}") from error
    if not isinstance(result, dict):
        raise ValueError(f"{case.key}: expected a JSON object")
    required = {
        "operation": case.operation, "implementation": "newton",
        "size": case.size, "queries": case.queries,
        "seed": seed, "repetitions": repetitions,
    }
    for key, expected in required.items():
        if type(result.get(key)) is not type(expected) or result[key] != expected:
            raise ValueError(f"{case.key}: invalid {key} in {binary}: "
                             f"{result.get(key)!r} != {expected!r}")
    checksum = result.get("checksum")
    if (not isinstance(checksum, str) or not 1 <= len(checksum) <= 16 or
            any(ch not in "0123456789abcdefABCDEF" for ch in checksum)):
        raise ValueError(f"{case.key}: missing/invalid 64-bit hexadecimal checksum")
    for key in ("items", "input_bytes"):
        value = result.get(key)
        if type(value) is not int or value < 0:
            raise ValueError(f"{case.key}: missing/invalid {key}={value!r}")
    for key in ("median_ms", "minimum_ms", "maximum_ms"):
        value = result.get(key)
        if type(value) not in (float, int) or not math.isfinite(value) or value <= 0:
            raise ValueError(f"{case.key}: invalid time {key}={value!r}")
    if not result["minimum_ms"] <= result["median_ms"] <= result["maximum_ms"]:
        raise ValueError(f"{case.key}: inconsistent timing statistics")
    return result


def decide(candidate: list[dict], reference: list[dict],
           limit: float, min_slow_rounds: int) -> dict:
    """Paired median ratio, plus repeated direction confirmation.

    A small noisy slowdown is always reported, but cannot block on one spike.
    This is not a proof that zero regression is possible on virtualized hosts.
    """
    if not candidate or len(candidate) != len(reference):
        raise ValueError("Paired samples are required for every round")
    for samples in (candidate, reference):
        for sample in samples:
            value = sample.get("median_ms")
            if (type(value) not in (int, float) or
                    not math.isfinite(value) or value <= 0):
                raise ValueError("Missing, nonfinite or nonpositive timing sample")
    deltas = [c["median_ms"] / r["median_ms"] - 1.0
              for c, r in zip(candidate, reference)]
    median_delta = statistics.median(deltas)
    slower_rounds = sum(delta > 0 for delta in deltas)
    status = ("FAIL" if median_delta > limit and slower_rounds >= min_slow_rounds
              else "WARN" if median_delta > 0 else "PASS")
    return {
        "status": status,
        "median_change_pct": round(median_delta * 100, 4),
        "round_changes_pct": [round(change * 100, 4) for change in deltas],
        "slower_rounds": slower_rounds,
        "rounds": len(deltas),
        "candidate_median_ms": statistics.median(s["median_ms"] for s in candidate),
        "reference_median_ms": statistics.median(s["median_ms"] for s in reference),
    }


def run(args: argparse.Namespace) -> dict:
    files = {
        "candidate": {"classic": args.candidate_classic,
                      "extended": args.candidate_extended},
        "base": {"classic": args.base_classic,
                 "extended": args.base_extended},
        "anchor": {"classic": args.anchor_classic,
                   "extended": args.anchor_extended},
    }
    for revision in files.values():
        for path in revision.values():
            if not path.is_file():
                raise FileNotFoundError(f"Required benchmark binary not found: {path}")

    rows = []
    for index, case in enumerate(cases(), start=1):
        samples = {rev: [] for rev in files}
        expected_signature = None
        for round_index in range(args.rounds):
            # Rotate order so a particular revision does not always run hot.
            order = (("base", "candidate", "anchor"),
                     ("candidate", "anchor", "base"),
                     ("anchor", "base", "candidate"))[round_index % 3]
            for revision in order:
                sample = measure(files[revision][case.suite], case,
                                 args.repetitions, args.seed)
                signature = (sample["checksum"], sample["items"], sample["input_bytes"])
                if expected_signature is None:
                    expected_signature = signature
                if signature != expected_signature:
                    raise ValueError(
                        f"CORRECTNESS REGRESSION {case.key}: {revision} round "
                        f"{round_index + 1} produced {signature}; expected {expected_signature}")
                samples[revision].append(sample)
        comparisons = {
            rev: decide(samples["candidate"], samples[rev],
                        args.max_regression_pct / 100.0, args.min_slow_rounds)
            for rev in ("base", "anchor")
        }
        # No production/build/fixture source changed vs base: keep variance
        # visible, but never reject a docs-only PR for VM scheduling noise.
        if args.protected_code_identical:
            for comparison in comparisons.values():
                if comparison["status"] == "FAIL":
                    comparison["status"] = "WARN"
                    comparison["reason"] = "unchanged protected source tree; timing noise"
        overall = "FAIL" if any(x["status"] == "FAIL" for x in comparisons.values()) \
            else "WARN" if any(x["status"] == "WARN" for x in comparisons.values()) \
            else "PASS"
        row = {
            **asdict(case), "key": case.key,
            "correctness_signature": {
                "checksum": expected_signature[0],
                "items": expected_signature[1],
                "input_bytes": expected_signature[2],
            },
            "status": overall, "comparisons": comparisons,
            "samples": samples,
        }
        rows.append(row)
        details = " | ".join(f"{rev}: {c['median_change_pct']:+.2f}% {c['status']}"
                             for rev, c in comparisons.items())
        print(f"[{index:02d}/22] {case.key}: {overall} | {details}", flush=True)

    return {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "protected_code_identical_to_base": bool(args.protected_code_identical),
        "machine": {
            "platform": platform.platform(), "processor": platform.processor(),
            "cpu_count": os.cpu_count(), "python": platform.python_version(),
        },
        "revisions": {
            "candidate": args.candidate_sha,
            "base": args.base_sha,
            "published_anchor": PUBLISHED_ANCHOR,
        },
        "policy": {
            "rounds": args.rounds, "repetitions_per_process": args.repetitions,
            "seed": args.seed, "max_regression_pct": args.max_regression_pct,
            "min_slow_rounds": args.min_slow_rounds,
            "blocking": "paired median slowdown exceeds threshold AND repeats in "
                        "at least min_slow_rounds",
        },
        "case_count": len(rows),
        "failed_cases": [r["key"] for r in rows if r["status"] == "FAIL"],
        "warning_cases": [r["key"] for r in rows if r["status"] == "WARN"],
        "results": rows,
    }


def markdown(report: dict) -> str:
    p = report["policy"]
    lines = [
        "# NewtonKernels performance regression guard",
        "",
        f"Candidate: `{report['revisions']['candidate']}`",
        f"Merge base: `{report['revisions']['base']}`",
        f"Published anchor: `{PUBLISHED_ANCHOR}`",
        "",
        f"**{report['case_count']} protected cases**; "
        f"{len(report['failed_cases'])} blocking regressions; "
        f"{len(report['warning_cases'])} warning cases.",
        "",
        f"Policy: {p['rounds']} alternating rounds, {p['repetitions_per_process']} "
        f"inner repetitions, block >{p['max_regression_pct']}% paired-median "
        f"slowdown confirmed in at least {p['min_slow_rounds']} rounds.",
        "",
        "| Operation | Size | vs previous main | vs published anchor | Decision |",
        "|---|---:|---:|---:|---|",
    ]
    for row in report["results"]:
        c = row["comparisons"]
        cell = lambda rev: (f"{c[rev]['median_change_pct']:+.2f}% "
                            f"({c[rev]['status']})")
        lines.append(f"| {row['operation']} | {row['size']:,} | "
                     f"{cell('base')} | {cell('anchor')} | {row['status']} |")
    lines.extend([
        "",
        "A positive percentage means the proposed revision is **slower**.",
        "Every candidate and reference output matched across all rounds (checksum, "
        "result count, input size); a mismatch fails the job immediately.",
        "Raw per-process samples are preserved in results.json.",
        "GitHub-hosted runners are noisy: this guards repeatable degradations, "
        "not mathematical zero-regression. Minor WARN results require manual review.",
        "Memory, allocation and full application tail latency are not blocking "
        "metrics in this initial guard; expand the policy as new suites mature.",
        "",
    ])
    return "\n".join(lines)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    for revision in ("candidate", "base", "anchor"):
        for suite in ("classic", "extended"):
            parser.add_argument(f"--{revision}-{suite}", type=Path, required=True)
    parser.add_argument("--protected-code-identical", type=int, choices=(0, 1), default=0)
    parser.add_argument("--candidate-sha", required=True)
    parser.add_argument("--base-sha", required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=11)
    parser.add_argument("--max-regression-pct", type=float, default=3.0)
    parser.add_argument("--min-slow-rounds", type=int, default=4)
    parser.add_argument("--seed", type=int, default=SEED)
    args = parser.parse_args()
    if not (5 <= args.rounds <= 30 and 3 <= args.repetitions <= 101
            and 1 <= args.min_slow_rounds <= args.rounds
            and 0 <= args.max_regression_pct <= 10
            and args.seed >= 0):
        parser.error("Invalid benchmark policy arguments")
    args.out.mkdir(parents=True, exist_ok=True)
    try:
        report = run(args)
        (args.out / "results.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8")
        (args.out / "REPORT.md").write_text(markdown(report), encoding="utf-8")
    except Exception as error:
        (args.out / "FAILURE.txt").write_text(
            f"{type(error).__name__}: {error}\n", encoding="utf-8")
        print(f"PERFORMANCE GUARD FAILED CLOSED: {error}", file=sys.stderr)
        return 2
    if report["failed_cases"]:
        print("BLOCKED: regressions in " + ", ".join(report["failed_cases"]),
              file=sys.stderr)
        return 1
    print("PERFORMANCE GUARD PASSED: no confirmed regressions across 22 cases.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
