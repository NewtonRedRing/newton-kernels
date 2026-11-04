#!/usr/bin/env python3
"""Issue-scoped, independently run smoke checks and evidence capture.

These lanes provide evidence toward resolution, not a claim that an issue's
entire acceptance checklist has been met. Never mutate GitHub issue state.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import platform
import subprocess
import sys
import time
from urllib.request import Request, urlopen

ROOT = Path(__file__).resolve().parent.parent
ANCHOR = "a3ccc65021181561aec1c8fe284bdeb7c8e1234c"
REPO = "NewtonRedRing/newton-kernels"

# Issue: (canonical implementation or evidence path, focused CTest executable)
MODULES = {
    33: ("src/core/cpu_features.cpp", "newton_unit_tests"),
    34: ("src/simd/byte_scan.cpp", "newton_unit_tests"),
    35: ("include/newton/parsing/structural_mask.hpp", "newton_portfolio_tests"),
    36: ("include/newton/parsing/csv_fields.hpp", "newton_portfolio_tests"),
    37: ("include/newton/parsing/csv_records.hpp", "newton_primitive_tests"),
    38: ("include/newton/analytics/csv_numeric_fast.hpp", "newton_portfolio_tests"),
    39: ("include/newton/analytics/group_by.hpp", "newton_unit_tests"),
    40: ("include/newton/analytics/flat_hash.hpp", "newton_portfolio_tests"),
    41: ("include/newton/analytics/hash_join.hpp", "newton_unit_tests"),
    42: ("include/newton/analytics/fused_join.hpp", "newton_portfolio_tests"),
    43: ("src/index/inverted_index.cpp", "newton_algorithm_tests"),
    44: ("include/newton/index/hybrid_postings.hpp", "newton_portfolio_tests"),
    45: ("include/newton/graph/shortest_path.hpp", "newton_algorithm_tests"),
    46: ("include/newton/graph/radix_heap.hpp", "newton_radix_heap_fault_tests"),
    47: ("src/parallel/executor.cpp", "newton_audit_edge_tests"),
    48: ("src/memory/mapped_file.cpp", "newton_audit_edge_tests"),
    49: ("include/newton/memory/aligned_buffer.hpp", "newton_unit_tests"),
    50: ("include/newton/columnar/views.hpp", "newton_columnar_tests"),
    51: ("src/c_api.cpp", "newton_c_api_tests"),
    52: ("tools/performance_guard.py", None),
}
# Coordination lanes are separately scoped to their existing gate/test
# rather than running the same unconditional test 12 times.
COORDINATION = {
    53: ("docs/open-issue-resolution-2026-11-04.md", "performance_guard"),
    54: ("docs/production-hardening-prep.md", "docs"),
    55: ("tools/benchmark_report_triage.py", "report_triage"),
    56: ("tests/consumer/CMakeLists.txt", "consumer"),
    57: (".github/workflows/ci.yml", "docs"),
    58: ("tests/unit/test_algorithms.cpp", "newton_algorithm_tests"),
    59: ("tests/unit/test_main.cpp", "newton_unit_tests"),
    60: ("tests/unit/test_native_portfolio.cpp", "newton_portfolio_tests"),
    61: ("tools/paired_aa_ab_diagnostic.py", "report_triage"),
    62: ("tests/unit/test_audit_edges.cpp", "newton_audit_edge_tests"),
    63: (".github/workflows/performance-guard.yml", "locks"),
    64: ("docs/production-hardening-prep.md", "docs"),
}
ALL_ISSUES = tuple(sorted((*MODULES, *COORDINATION, 82)))
assert len(ALL_ISSUES) == 33 and len(set(ALL_ISSUES)) == 33


def issue_snapshot(issue: int) -> dict:
    """Fetch live acceptance criteria; never treat an API failure as closure."""
    url = "https://api.github.com/repos/" + REPO + "/issues/" + str(issue)
    headers = {"Accept": "application/vnd.github+json",
               "User-Agent": "newton-issue-verification/1"}
    if os.environ.get("GITHUB_TOKEN"):
        headers["Authorization"] = "Bearer " + os.environ["GITHUB_TOKEN"]
    try:
        request = Request(url, headers=headers)
        with urlopen(request, timeout=15) as response:
            obj = json.load(response)
        body = obj.get("body") or ""
        pending = [line.strip() for line in body.splitlines()
                   if line.lstrip().startswith("- [ ]")]
        done = [line.strip() for line in body.splitlines()
                if line.lstrip().lower().startswith("- [x]")]
        return {"title": obj.get("title"), "state": obj.get("state"),
                "open_acceptance_items": pending,
                "checked_acceptance_items": done,
                "html_url": obj.get("html_url")}
    except Exception as exc:
        return {"fetch_error": type(exc).__name__ + ": " + str(exc),
                "open_acceptance_items": None,
                "state": "unknown; do not close"}


def run_command(record: dict, command: list[str], timeout: int = 900) -> None:
    start = time.monotonic()
    result = {"command": command}
    record["commands"].append(result)
    try:
        proc = subprocess.run(command, cwd=ROOT, text=True, timeout=timeout,
                              stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        result["return_code"] = proc.returncode
        result["duration_s"] = round(time.monotonic() - start, 3)
        result["output_tail"] = proc.stdout[-16000:]
        if proc.returncode:
            raise RuntimeError("command exited " + str(proc.returncode))
    except Exception as exc:
        result["error"] = type(exc).__name__ + ": " + str(exc)
        raise


def build_and_test(record: dict, target: str) -> None:
    directory = "build-issue-" + str(record["issue"])
    run_command(record, [
        "cmake", "-S", ".", "-B", directory, "-DCMAKE_BUILD_TYPE=Debug",
        "-DNEWTON_BUILD_TESTS=ON", "-DNEWTON_BUILD_EXAMPLES=OFF",
        "-DNEWTON_BUILD_C_API=ON", "-DNEWTON_WARNINGS_AS_ERRORS=ON",
    ])
    run_command(record, ["cmake", "--build", directory, "--target", target,
                         "--parallel", "2"])
    run_command(record, ["ctest", "--test-dir", directory, "--output-on-failure",
                         "-R", "^" + target + "$"], timeout=300)


def python_test(record: dict, pattern: str) -> None:
    run_command(record, ["python3", "-m", "unittest", "discover", "-s", "tests",
                         "-p", pattern, "-v"], timeout=180)


def consumer_check(record: dict) -> None:
    d = "build-issue-" + str(record["issue"])
    run_command(record, ["cmake", "-S", ".", "-B", d,
                         "-DCMAKE_BUILD_TYPE=Release",
                         "-DNEWTON_BUILD_TESTS=OFF",
                         "-DNEWTON_BUILD_EXAMPLES=OFF"])
    run_command(record, ["cmake", "--build", d, "--parallel", "2"])
    run_command(record, ["cmake", "--install", d, "--prefix", "stage-issue"])
    run_command(record, ["cmake", "-S", "tests/consumer",
                         "-B", "consumer-issue",
                         "-DCMAKE_PREFIX_PATH=stage-issue"])
    run_command(record, ["cmake", "--build", "consumer-issue", "--parallel", "2"])
    run_command(record, ["ctest", "--test-dir", "consumer-issue",
                         "--output-on-failure"])


def anchor_diagnostic(record: dict) -> None:
    if not (ROOT / "pinned-anchor" / "CMakeLists.txt").is_file():
        raise FileNotFoundError("historical anchor checkout is absent")
    for name, source in (("current", "."), ("anchor", "pinned-anchor")):
        run_command(record, ["cmake", "-S", source, "-B", "build-" + name,
                             "-DCMAKE_BUILD_TYPE=Release",
                             "-DNEWTON_BUILD_TESTS=OFF",
                             "-DNEWTON_BUILD_EXAMPLES=OFF",
                             "-DNEWTON_BUILD_BENCHMARKS=ON",
                             "-DNEWTON_EXTERNAL_BENCHMARKS=ON",
                             "-DNEWTON_COMP_BENCH_WITH_BOOST=OFF",
                             "-DNEWTON_COMP_BENCH_WITH_VINCE=OFF"])
        run_command(record, ["cmake", "--build", "build-" + name,
                             "--target", "newton_compare",
                             "--parallel", "2"])
    run_command(record, [
        "python3", "tools/csv_anchor_aa_diagnostic.py",
        "--current", "build-current/benchmarks/competitors/newton_compare",
        "--anchor", "build-anchor/benchmarks/competitors/newton_compare",
        "--out", "reports/csv-anchor-raw",
        "--base-sha", os.environ.get("GITHUB_SHA", "unknown"),
        "--rounds", "12",
    ], timeout=540)


def verify(issue: int, *, execute: bool) -> dict:
    if issue not in ALL_ISSUES:
        raise ValueError("Unknown issue; refuse to invent a stream")
    source, check = ((MODULES[issue] if issue in MODULES
                      else COORDINATION[issue] if issue in COORDINATION
                      else ("tools/csv_anchor_aa_diagnostic.py", "csv_anchor_aa")))
    result = {"issue": issue, "source_path": source,
              "focused_test": check, "status": "not-run",
              "head": os.environ.get("GITHUB_SHA"),
              "platform": platform.platform(),
              "timestamp_utc": datetime.now(timezone.utc).isoformat(),
              "commands": [], "acceptance": issue_snapshot(issue)
              if execute else {"not_fetched": True}}
    if not (ROOT / source).is_file():
        raise FileNotFoundError("Issue source path missing: " + source)
    if not (ROOT / "docs/open-issue-resolution-2026-11-04.md").is_file():
        raise FileNotFoundError("Missing complete issue register")
    if execute:
        try:
            if issue == 82:
                anchor_diagnostic(result)
            elif check in (None, "performance_guard"):
                python_test(result, "test_performance_guard.py")
                python_test(result, "test_performance_locks.py")
            elif check == "locks":
                python_test(result, "test_performance_locks.py")
            elif check == "report_triage":
                python_test(result, "test_benchmark_report_triage.py")
            elif check == "docs":
                run_command(result, ["python3", "tools/check_docs.py"])
            elif check == "consumer":
                consumer_check(result)
            else:
                build_and_test(result, check)
            result["status"] = "targeted-checks-passed; full issue not certified"
        except Exception as exc:
            result["status"] = "targeted-checks-failed; issue remains open"
            result["error"] = type(exc).__name__ + ": " + str(exc)
    else:
        result["status"] = "manifest-self-check"
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--issue", type=int)
    parser.add_argument("--output", type=Path, default=Path("reports/issue.json"))
    parser.add_argument("--self-check", action="store_true")
    options = parser.parse_args()
    if options.self_check:
        rows = [verify(n, execute=False) for n in ALL_ISSUES]
        print("Validated exactly " + str(len(rows)) + " issue mappings")
        return 0
    if options.issue is None:
        parser.error("--issue is required unless --self-check")
    result = verify(options.issue, execute=True)
    options.output.parent.mkdir(parents=True, exist_ok=True)
    options.output.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    print(f"Issue #{options.issue}: {result['status']}")
    for cmd in result["commands"]:
        print(" ".join(cmd["command"]), "exit=", cmd.get("return_code"),
              "seconds=", cmd.get("duration_s"))
        if cmd.get("error"):
            print(cmd["output_tail"][-2500:], file=sys.stderr)
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as stream:
            stream.write(f"### Issue #{options.issue} independent validation\n\n")
            stream.write(f"**{result['status']}** — {result['source_path']}\n\n")
            stream.write(f"Targeted test: {result['focused_test'] or 'policy suite'}\n\n")
            acceptance = result["acceptance"].get("open_acceptance_items")
            stream.write("Unresolved issue checklist items: " +
                         (str(len(acceptance)) if acceptance is not None
                          else "unknown (API unavailable)") + "\n\n")
            stream.write("Passing this lane is not permission to close the issue.\n")
    return 0 if "passed" in result["status"] else 1


if __name__ == "__main__":
    sys.exit(main())
