#!/usr/bin/env python3
"""Append-only, per-case, same-runner high-water performance protection.

Ordinary guard: candidate vs previous main and the November 2026 published anchor.
Additional locks: candidate vs each approved, immutable, *merged* winning commit.
A prior lock is never removed when a faster revision is promoted. All output
signatures and timing rounds are compared again on the current CI runner.
"""
from __future__ import annotations

import argparse
from collections import defaultdict
from datetime import datetime, timezone
import json
from pathlib import Path
import platform
import re
import statistics
import sys

import performance_guard as guard

SHA = re.compile(r"[0-9a-f]{40}\Z")
EVIDENCE = re.compile(
    r"https://github\.com/NewtonRedRing/newton-kernels/actions/runs/[0-9]+\Z"
)
ROUNDS = 5
REPETITIONS = 11
MAX_REGRESSION = 3.0
MIN_SLOW_ROUNDS = 4
MIN_PROMOTION_GAIN = 3.0


def load(path: Path, *, missing_ok: bool = False) -> list[dict]:
    if missing_ok and not path.exists():
        return []
    obj = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(obj, dict) or set(obj) != {"schema_version", "locks"}:
        raise ValueError(f"{path}: expected schema_version and locks only")
    if type(obj["schema_version"]) is not int or obj["schema_version"] != 1:
        raise ValueError(f"{path}: unsupported lock format")
    if not isinstance(obj["locks"], list):
        raise ValueError(f"{path}: locks must be a list")
    valid_cases = {c.key for c in guard.cases()}
    seen = set()
    entries = []
    for entry in obj["locks"]:
        if not isinstance(entry, dict) or set(entry) != {"case", "sha", "evidence"}:
            raise ValueError(f"{path}: lock needs case, sha and evidence")
        key, sha, evidence = entry["case"], entry["sha"], entry["evidence"]
        if type(key) is not str or key not in valid_cases:
            raise ValueError(f"{path}: unknown protected fixture {key!r}")
        if type(sha) is not str or not SHA.fullmatch(sha):
            raise ValueError(f"{path}: invalid full commit SHA for {key}")
        if type(evidence) is not str or not EVIDENCE.fullmatch(evidence):
            raise ValueError(f"{path}: missing immutable GitHub Actions evidence URL")
        if (key, sha) in seen:
            raise ValueError(f"{path}: duplicate high-water lock for {key}/{sha}")
        seen.add((key, sha))
        entries.append({"case": key, "sha": sha, "evidence": evidence})
    return entries


def validate_upgrade(base: list[dict], candidate: list[dict]) -> None:
    """Every accepted lock must remain exact; only new entries may be appended."""
    if candidate[:len(base)] != base:
        raise ValueError("PERFORMANCE LOCK DOWNGRADE: old entries removed, reordered or changed")
    if len(candidate) < len(base):
        raise ValueError("PERFORMANCE LOCK DOWNGRADE: removed prior winning commit")


def checked_references(entries: list[dict]) -> list[str]:
    return list(dict.fromkeys(item["sha"] for item in entries))


def measure_lock(args: argparse.Namespace, entries: list[dict]) -> dict:
    by_case = defaultdict(list)
    for entry in entries:
        by_case[entry["case"]].append(entry)
    lookup = {c.key: c for c in guard.cases()}
    rows = []
    for case_key, pins in by_case.items():
        case = lookup[case_key]
        names = ["candidate"] + [pin["sha"] for pin in pins]
        paths = {
            "candidate": (args.candidate_classic if case.suite == "classic"
                          else args.candidate_extended),
        }
        binary_name = ("newton_compare" if case.suite == "classic"
                       else "newton_compare_extended")
        for pin in pins:
            paths[pin["sha"]] = (args.build_root / pin["sha"] /
                                 "benchmarks" / "competitors" / binary_name)
        samples = {name: [] for name in names}
        expected = None
        for cycle in range(ROUNDS):
            # Rotate and reverse to avoid consistently favorable launch order.
            order = names[cycle % len(names):] + names[:cycle % len(names)]
            if cycle % 2:
                order.reverse()
            for name in order:
                sample = guard.measure(paths[name], case, REPETITIONS, guard.SEED)
                signature = (sample["checksum"], sample["items"], sample["input_bytes"])
                if expected is None:
                    expected = signature
                if signature != expected:
                    raise ValueError(f"CORRECTNESS REGRESSION {case_key} at {name}: "
                                     f"{signature} != {expected}")
                samples[name].append(sample)
        comparisons = {}
        for pin in pins:
            sha = pin["sha"]
            comparison = guard.decide(samples["candidate"], samples[sha],
                                      MAX_REGRESSION / 100, MIN_SLOW_ROUNDS)
            if args.protected_code_identical and comparison["status"] == "FAIL":
                comparison["status"] = "WARN"
                comparison["reason"] = "unchanged protected code vs base"
            comparisons[sha] = comparison
        status = ("FAIL" if any(x["status"] == "FAIL" for x in comparisons.values())
                  else "WARN" if any(x["status"] == "WARN" for x in comparisons.values())
                  else "PASS")
        rows.append({
            "case": case_key,
            "status": status,
            "signatures_equal": True,
            "pins": pins,
            "comparisons": comparisons,
            "samples": samples,
        })
        summary = "; ".join(
            f"{sha[:10]}: {comp['median_change_pct']:+.2f}% {comp['status']}"
            for sha, comp in comparisons.items()
        )
        print(f"LOCK {case_key}: {status} | {summary}", flush=True)
    return {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "candidate_sha": args.candidate_sha,
        "machine": {"platform": platform.platform(),
                    "python": platform.python_version()},
        "policy": {
            "rounds": ROUNDS, "repetitions_per_process": REPETITIONS,
            "max_regression_pct": MAX_REGRESSION,
            "min_slow_rounds": MIN_SLOW_ROUNDS,
            "reference": "immutable commit, rebuilt and measured on same runner",
        },
        "lock_count": len(entries),
        "case_count": len(rows),
        "failed_cases": [x["case"] for x in rows if x["status"] == "FAIL"],
        "warning_cases": [x["case"] for x in rows if x["status"] == "WARN"],
        "results": rows,
    }


def run(args: argparse.Namespace) -> int:
    args.out.mkdir(parents=True, exist_ok=True)
    try:
        entries = load(args.manifest)
        report = measure_lock(args, entries)
        (args.out / "locked-performance-results.json").write_text(
            json.dumps(report, indent=2) + "\n", encoding="utf-8")
        lines = [
            "# Protected high-water performance baselines", "",
            f"Candidate: `{args.candidate_sha}`",
            f"Locked reference entries: {len(entries)}",
            f"Locked cases: {report['case_count']}", "",
            "| Case | Protected commit | Candidate change | Verdict |",
            "|---|---|---:|---|",
        ]
        for row in report["results"]:
            for pin in row["pins"]:
                sha = pin["sha"]
                comp = row["comparisons"][sha]
                lines.append(f"| {row['case']} | `{sha[:12]}` | "
                             f"{comp['median_change_pct']:+.2f}% | {comp['status']} |")
        lines += [
            "", "Positive change is slower. All checks use exact matching output",
            "and the same virtual machine for the candidate and pinned source.",
            "Existing 22-case, published-anchor and six CSV pipeline gates remain separate.",
            "",
        ]
        (args.out / "locked-performance-REPORT.md").write_text(
            "\n".join(lines), encoding="utf-8")
        if report["failed_cases"]:
            print("BLOCKED HIGH-WATER REGRESSION: " +
                  ", ".join(report["failed_cases"]), file=sys.stderr)
            return 1
        print(f"HIGH-WATER GUARD PASSED: {len(entries)} approved locks")
        return 0
    except Exception as exc:
        (args.out / "locked-performance-FAILURE.txt").write_text(
            f"{type(exc).__name__}: {exc}\n", encoding="utf-8")
        print(f"HIGH-WATER GUARD FAILED CLOSED: {exc}", file=sys.stderr)
        return 2


def propose(args: argparse.Namespace) -> int:
    """Suggest promotions from a *passing*, committed source benchmark report.

    The report is untrusted evidence, never a command to modify the manifest.
    A reviewer must confirm final-head CI, pipeline guard and main ancestry.
    """
    entries = load(args.manifest)
    report = json.loads(args.report.read_text(encoding="utf-8"))
    if (report.get("revisions", {}).get("candidate") != args.candidate_sha or
            report.get("case_count") != 22 or report.get("failed_cases") or
            len(report.get("results", [])) != 22):
        raise ValueError("Promotion requires a complete, matching, successful guard report")
    protected_identical = report.get("protected_code_identical_to_base")
    if type(protected_identical) is not bool:
        raise ValueError("Promotion report missing protected-source comparison")
    known = {(entry["case"], entry["sha"]) for entry in entries}
    proposed = []
    for row in report["results"]:
        comp = row["comparisons"]["base"]
        changes = comp["round_changes_pct"]
        if (not protected_identical and row["status"] != "FAIL" and
                len(changes) == ROUNDS and
                comp["median_change_pct"] <= -MIN_PROMOTION_GAIN and
                sum(x < 0 for x in changes) >= MIN_SLOW_ROUNDS and
                (row["key"], args.candidate_sha) not in known):
            proposed.append({
                "case": row["key"], "sha": args.candidate_sha,
                "evidence": args.evidence,
            })
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(
        json.dumps({"existing_locks": entries, "proposed_locks": proposed,
                    "instructions": "Review source commit and full CI; append "
                    "approved entries to benchmarks/performance-locks.json "
                    "in a separate PR only after its source commit is on main."},
                   indent=2) + "\n", encoding="utf-8")
    print(f"Proposed {len(proposed)} high-water locks (approval required)")
    return 0


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    sub = p.add_subparsers(dest="command", required=True)

    check = sub.add_parser("validate", help="Check append-only upgrades")
    check.add_argument("--base", type=Path, required=True)
    check.add_argument("--candidate", type=Path, required=True)

    refs = sub.add_parser("refs", help="List unique pinned SHA references")
    refs.add_argument("--manifest", type=Path, required=True)

    locked = sub.add_parser("run", help="Rebuild-paired high-water checks")
    locked.add_argument("--manifest", type=Path, required=True)
    locked.add_argument("--build-root", type=Path, required=True)
    locked.add_argument("--candidate-classic", type=Path, required=True)
    locked.add_argument("--candidate-extended", type=Path, required=True)
    locked.add_argument("--candidate-sha", required=True)
    locked.add_argument("--protected-code-identical", type=int,
                        choices=(0, 1), default=0)
    locked.add_argument("--out", type=Path, required=True)

    promote = sub.add_parser("propose", help="Write review-only promotions")
    promote.add_argument("--manifest", type=Path, required=True)
    promote.add_argument("--report", type=Path, required=True)
    promote.add_argument("--candidate-sha", required=True)
    promote.add_argument("--evidence", required=True)
    promote.add_argument("--out", type=Path, required=True)

    args = p.parse_args()
    try:
        if args.command == "validate":
            validate_upgrade(load(args.base, missing_ok=True), load(args.candidate))
            print("High-water lock manifest: valid append-only update")
            return 0
        if args.command == "refs":
            for sha in checked_references(load(args.manifest)):
                print(sha)
            return 0
        if args.command == "run":
            return run(args)
        if not EVIDENCE.fullmatch(args.evidence):
            raise ValueError("Evidence must be a GitHub Actions run URL")
        if not SHA.fullmatch(args.candidate_sha):
            raise ValueError("Candidate must be a full source commit SHA")
        return propose(args)
    except (ValueError, KeyError, TypeError, OSError, json.JSONDecodeError) as exc:
        print(f"LOCK POLICY REJECTED: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
