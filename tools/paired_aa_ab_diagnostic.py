#!/usr/bin/env python3
"""Diagnostic-only controlled A/A and A/B of protected 22-case benchmarks.

Uses the *unchanged* official manifest, runner and decision function from
performance_guard.py. It cannot override a failed production regression guard.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import platform
import random
import statistics
import sys

from performance_guard import cases, decide, measure, SEED


def run(paths: dict[str, dict[str, Path]], rounds: int, repetitions: int) -> dict:
    labels = ("base_a", "base_b", "pr24", "pr25", "anchor")
    if set(paths) != set(labels):
        raise ValueError("Need two identical-source controls and three targets")
    if rounds < 5 or repetitions < 3:
        raise ValueError("Insufficient diagnostic repetitions")
    for label in labels:
        if set(paths[label]) != {"classic", "extended"}:
            raise ValueError(f"Incomplete build for {label}")
        if any(not binary.is_file() for binary in paths[label].values()):
            raise FileNotFoundError(f"Missing benchmark binaries for {label}")
    rows = []
    for i, case in enumerate(cases(), start=1):
        samples = {label: [] for label in labels}
        signature = None
        for round_number in range(rounds):
            # Deterministic balanced rotation controls time-of-run order, not
            # hardware scheduling. Use the same ordering for every revision.
            order = list(labels)
            order = order[round_number % len(order):] + order[:round_number % len(order)]
            if round_number % 2:
                order.reverse()
            for label in order:
                sample = measure(paths[label][case.suite], case, repetitions, SEED)
                current = (sample["checksum"], sample["items"], sample["input_bytes"])
                if signature is None:
                    signature = current
                if current != signature:
                    raise ValueError(f"CORRECTNESS REGRESSION {case.key}: {label}")
                samples[label].append(sample)
        # The same reference SHA is compiled twice in independent build dirs.
        control = decide(samples["base_a"], samples["base_b"], .03, 4)
        against_base = {
            label: decide(samples[label], samples["base_a"], .03, 4)
            for label in ("pr24", "pr25")
        }
        against_anchor = {
            label: decide(samples[label], samples["anchor"], .03, 4)
            for label in ("pr24", "pr25")
        }
        rows.append({
            "case": case.key,
            "control_aa": control,
            "candidate_vs_base": against_base,
            "candidate_vs_anchor": against_anchor,
            "checksums_equal": True,
            "samples": samples,
        })
        print(f"[{i:02d}/22] {case.key}: A/A {control['median_change_pct']:+.2f}% "
              f"{control['status']}; #24 {against_base['pr24']['median_change_pct']:+.2f}% "
              f"#25 {against_base['pr25']['median_change_pct']:+.2f}%", flush=True)
    aa_fail = [row["case"] for row in rows if row["control_aa"]["status"] == "FAIL"]
    return {
        "disclaimer": "DIAGNOSTIC ONLY. A/A failure never waives production guard.",
        "run_environment": {"platform": platform.platform(), "python": platform.python_version()},
        "policy_unchanged": {"max_regression_pct": 3, "min_slow_rounds": 4,
                             "rounds": rounds, "repetitions": repetitions,
                             "seed": SEED},
        "manifest_case_count": len(rows),
        "aa_failed_cases": aa_fail,
        "results": rows,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=5)
    parser.add_argument("--repetitions", type=int, default=11)
    args = parser.parse_args()
    binaries = {
        label: {name: args.root / f"build-{label}" / "benchmarks" / "competitors" / exe
                for name, exe in (("classic", "newton_compare"),
                                  ("extended", "newton_compare_extended"))}
        for label in ("base_a", "base_b", "pr24", "pr25", "anchor")
    }
    args.out.mkdir(parents=True, exist_ok=True)
    try:
        report = run(binaries, args.rounds, args.repetitions)
        (args.out / "paired-aa-ab.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8")
        lines = ["# Independent same-runner A/A and A/B diagnostic", "",
                 "**This report does not bypass or modify the protected 22+6 guards.**", "",
                 f"A/A blocking-case-like variation: {len(report['aa_failed_cases'])} / 22.",
                 "", "| Case | A/A base vs base | #24 vs base | #25 vs base |",
                 "|---|---:|---:|---:|"]
        for row in report["results"]:
            def pct(obj):
                return f"{obj['median_change_pct']:+.2f}% ({obj['status']})"
            lines.append(f"| {row['case']} | {pct(row['control_aa'])} | "
                         f"{pct(row['candidate_vs_base']['pr24'])} | "
                         f"{pct(row['candidate_vs_base']['pr25'])} |")
        (args.out / "REPORT.md").write_text("\n".join(lines) + "\n", encoding="utf-8")
        return 0
    except Exception as error:
        (args.out / "ERROR.txt").write_text(f"{type(error).__name__}: {error}\n")
        print(f"DIAGNOSTIC FAILED CLOSED: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
