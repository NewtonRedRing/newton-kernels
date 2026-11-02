#!/usr/bin/env python3
"""Original reproducible NewtonKernels external comparison runner (stdlib only).

Each executable invocation is a fresh process. All variants use fixed seeds,
identical input generation and checksums; order alternates between rounds.
Results are screening measurements, not independent third-party validation.
"""
import argparse
import datetime as dt
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import statistics
import subprocess
import sys

CASES = {
    "scan": [(1 << 20, 1), (16 << 20, 1)],
    "graph": [(5000, 32), (20000, 64)],
    "csv": [(10000, 1), (100000, 1)],
}
COMPETITORS = {"scan": "stl", "graph": "boost", "csv": "vince"}
UPSTREAM = {
    "stl": "C++ standard library in recorded toolchain",
    "boost": "Installed Boost headers; record system package version",
    "vince": "vincentlaucsb/csv-parser@86f1cab4fcb50a936475b5f778e8ee8c5dfe2259",
}

def command_text(cmd):
    try:
        p = subprocess.run(cmd, text=True, capture_output=True, timeout=6, check=True)
        return p.stdout.strip().splitlines()[0][:250]
    except (OSError, subprocess.SubprocessError, IndexError):
        return None

def environment(binary):
    cpu = "unknown"
    if sys.platform.startswith("linux"):
        try:
            for line in Path("/proc/cpuinfo").read_text().splitlines():
                if line.startswith("model name"):
                    cpu = line.split(":", 1)[1].strip()
                    break
        except OSError:
            pass
    return {
        "utc_timestamp": dt.datetime.now(dt.timezone.utc).isoformat(),
        "platform": platform.platform(),
        "machine": platform.machine(),
        "cpu_model": cpu,
        "logical_cpus": os.cpu_count(),
        "python": platform.python_version(),
        "compiler": command_text(["c++", "--version"]),
        "cmake": command_text(["cmake", "--version"]),
        "boost_version_macro": boost_version(),
        "binary_sha256": hashlib.sha256(binary.read_bytes()).hexdigest(),
        "cpu_affinity": sorted(os.sched_getaffinity(0)) if hasattr(os, "sched_getaffinity") else None,
        "cpu_governor": cpu_governor(),
        "virtualization": command_text(["systemd-detect-virt"]),
    }

def cpu_governor():
    try:
        return Path("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor").read_text().strip()
    except OSError:
        return None

def boost_version():
    try:
        text = Path("/usr/include/boost/version.hpp").read_text()
        match = re.search(r'^#define BOOST_LIB_VERSION\s+"([^"]+)"', text, re.M)
        return match.group(1) if match else None
    except OSError:
        return None

def execute(binary, case, implementation, size, queries, seed, repetitions):
    cmd = [str(binary), "--operation", case, "--implementation", implementation,
           "--size", str(size), "--queries", str(queries), "--seed", str(seed),
           "--repetitions", str(repetitions)]
    p = subprocess.run(cmd, capture_output=True, text=True, timeout=180)
    if p.returncode:
        raise RuntimeError(f"{' '.join(cmd)} failed ({p.returncode}): {p.stderr[:2000]}")
    try:
        return json.loads(p.stdout)
    except json.JSONDecodeError as exc:
        raise RuntimeError(f"Invalid benchmark JSON: {p.stdout[:2000]}") from exc

def report(rows, env):
    lines = ["# NewtonKernels external performance comparison",
             "", "> Generated from recorded, same-run executions. Lower latency is better.",
             "> Results apply only to these workloads and environment; not a general speed claim.",
             "", "## Environment", "",
             f"- UTC: {env['utc_timestamp']}",
             f"- CPU: {env['cpu_model']} ({env['logical_cpus']} logical CPUs)",
             f"- OS: {env['platform']}",
             f"- Compiler: {env['compiler']}",
             f"- Boost: {env['boost_version_macro'] or 'not detected'}",
             f"- CPU affinity: {env['cpu_affinity']}",
             f"- Governor: {env['cpu_governor'] or 'unavailable'}",
             f"- Virtualization: {env['virtualization'] or 'unavailable'}",
             "", "## Results", "",
             "| Workload | Size | NewtonKernels median (ms) | Competitor | Competitor median (ms) | Newton speedup | Checksum |",
             "|---|---:|---:|---|---:|---:|---|"]
    for row in rows:
        lines.append(f"| {row['operation']} | {row['size']} | "
                     f"{row['newton_median_ms']:.4f} | {row['competitor']} | "
                     f"{row['competitor_median_ms']:.4f} | {row['speedup']:.2f}× | "
                     f"`{row['checksum']}` |")
    lines += ["", "## Interpretation", "",
              "Speedup = competitor median / NewtonKernels median; values below 1× mean NewtonKernels is slower.",
              "The median is calculated across alternating fresh processes, each reporting a median of in-process repetitions.",
              "Each pair was checked for equal digest and item count on the exact same generated fixture.",
              "Memory is captured per process as peak RSS in the raw JSON; it includes input and runtime overhead.",
              "Graph measures target queries with reusable Newton workspace vs Boost's standard distance-map initialization.",
              "CSV compares in-memory processing of normalized cells; 1 thread, no header, fixed comma delimiter; both include their own parser overhead.",
              "Scan compares `newton::simd::count_byte` with optimized `std::count`, counting the same delimiter.",
              "Input construction, graph build, output serialization, and input storage are excluded from timed sections.",
              "The benchmark is a synthetic screening workload and does not prove superior end-to-end application performance.",
              "", "## Reproduction", "",
              "See the repository `benchmarks/competitors/README.md` for the exact commands and fairness limitations.",
              "Raw results: `results.json`. Keep this file together with the raw data."]
    return "\n".join(lines) + "\n"

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=Path("benchmark-results"))
    parser.add_argument("--suite", choices=["all", "scan", "graph", "csv"], default="all")
    parser.add_argument("--include-vince", action="store_true", help="Require and run Vince comparator (must be linked)")
    parser.add_argument("--rounds", type=int, default=3)
    parser.add_argument("--repetitions", type=int, default=7)
    parser.add_argument("--quick", action="store_true", help="Small fixture only, for CI smoke testing")
    parser.add_argument("--seed", type=int, default=1332857405)
    args = parser.parse_args()
    if args.rounds < 2 or args.repetitions < 3:
        parser.error("Require at least 2 rounds and 3 inner repetitions")
    binary = args.binary.resolve(strict=True)
    env = environment(binary)
    rows = []
    for case, sizes in CASES.items():
        if args.suite not in ("all", case):
            continue
        if case == "csv" and not args.include_vince:
            print("CSV comparison SKIPPED: pass --include-vince after building with NEWTON_COMP_BENCH_WITH_VINCE=ON", file=sys.stderr)
            continue
        for size, queries in sizes[:1] if args.quick else sizes:
            competitor = COMPETITORS[case]
            samples = {"newton": [], competitor: []}
            for round_index in range(args.rounds):
                order = ("newton", competitor) if round_index % 2 == 0 else (competitor, "newton")
                for impl in order:
                    sample = execute(binary, case, impl, size, queries, args.seed, args.repetitions)
                    samples[impl].append(sample)
            checksums = {(s["checksum"], s["items"], s["input_bytes"])
                         for both in samples.values() for s in both}
            if len(checksums) != 1:
                raise RuntimeError(f"INCORRECT COMPARISON {case} size={size}: {checksums}")
            a = statistics.median(s["median_ms"] for s in samples["newton"])
            b = statistics.median(s["median_ms"] for s in samples[competitor])
            row = {"operation": case, "size": size, "queries": queries,
                   "competitor": competitor, "competitor_revision": UPSTREAM[competitor],
                   "seed": args.seed, "rounds": args.rounds, "repetitions": args.repetitions,
                   "newton_median_ms": a, "competitor_median_ms": b,
                   "speedup": b / a, "checksum": samples["newton"][0]["checksum"],
                   "verified_equal_output": True, "samples": samples}
            rows.append(row)
            print(f"{case:<6} {size:>10}: newton {a:.4f} ms, {competitor} {b:.4f} ms, {b/a:.2f}x; output MATCH", flush=True)
    if not rows:
        raise RuntimeError("No comparisons were run")
    args.out.mkdir(parents=True, exist_ok=True)
    (args.out / "results.json").write_text(json.dumps({"environment": env, "results": rows}, indent=2) + "\n")
    (args.out / "REPORT.md").write_text(report(rows, env))
    print(f"Wrote {args.out.resolve() / 'REPORT.md'}")

if __name__ == "__main__":
    main()
