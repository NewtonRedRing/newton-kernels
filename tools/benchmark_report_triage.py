#!/usr/bin/env python3
"""Read-only cross-run triage of protected NewtonKernels benchmark reports.

This tool never modifies benchmarks, thresholds, fixture inputs or CI gates.
Reports must already have been produced by tools/performance_guard.py.
"""
from __future__ import annotations

import argparse
import json
import math
from pathlib import Path
import statistics


def load(path: Path) -> dict:
    report = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(report, dict) or report.get("case_count") != 22:
        raise ValueError(f"{path}: expected a complete 22-case guard report")
    rows = report.get("results")
    if not isinstance(rows, list) or len(rows) != 22:
        raise ValueError(f"{path}: missing protected cases")
    keys = [row.get("key") for row in rows if isinstance(row, dict)]
    if len(keys) != 22 or len(set(keys)) != 22:
        raise ValueError(f"{path}: duplicate or invalid case keys")
    for row in rows:
        if row.get("status") not in {"PASS", "WARN", "FAIL"}:
            raise ValueError(f"{path}: invalid status in {row.get('key')}")
        comparisons = row.get("comparisons")
        if not isinstance(comparisons, dict) or set(comparisons) != {"base", "anchor"}:
            raise ValueError(f"{path}: missing paired reference comparison")
        for ref in ("base", "anchor"):
            delta = comparisons[ref].get("median_change_pct")
            if type(delta) not in (float, int) or not math.isfinite(delta):
                raise ValueError(f"{path}: invalid change for {row['key']} vs {ref}")
    return report


def summarize(reports: list[dict]) -> list[dict]:
    if not reports:
        raise ValueError("At least one guard report is required")
    key_sets = [set(row["key"] for row in report["results"]) for report in reports]
    if any(keys != key_sets[0] for keys in key_sets[1:]):
        raise ValueError("Reports contain different protected case keys")
    output = []
    for key in sorted(key_sets[0]):
        rows = [next(row for row in rep["results"] if row["key"] == key)
                for rep in reports]
        output.append({
            "case": key,
            "failed_runs": sum(row["status"] == "FAIL" for row in rows),
            "warning_runs": sum(row["status"] == "WARN" for row in rows),
            "total_runs": len(rows),
            "median_delta_vs_base_pct": round(statistics.median(
                row["comparisons"]["base"]["median_change_pct"] for row in rows), 3),
            "median_delta_vs_anchor_pct": round(statistics.median(
                row["comparisons"]["anchor"]["median_change_pct"] for row in rows), 3),
        })
    return sorted(output, key=lambda row: (-row["failed_runs"], row["case"]))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("reports", type=Path, nargs="+",
                        help="One or more raw results.json files")
    parser.add_argument("--json-out", type=Path,
                        help="Optional output path for triage summary")
    args = parser.parse_args()
    try:
        reports = [load(path) for path in args.reports]
        summary = summarize(reports)
        for row in summary:
            if row["failed_runs"] or row["warning_runs"]:
                print(f"{row['case']}: FAIL {row['failed_runs']}/{row['total_runs']}; "
                      f"WARN {row['warning_runs']}/{row['total_runs']}; "
                      f"median vs base {row['median_delta_vs_base_pct']:+.3f}%; "
                      f"vs anchor {row['median_delta_vs_anchor_pct']:+.3f}%")
        if args.json_out is not None:
            args.json_out.write_text(
                json.dumps({"runs": len(reports), "cases": summary}, indent=2) + "\n",
                encoding="utf-8")
    except (ValueError, OSError, json.JSONDecodeError, KeyError, TypeError) as error:
        parser.exit(2, f"TRIAGE INPUT ERROR: {error}\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
