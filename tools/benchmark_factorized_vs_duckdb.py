#!/usr/bin/env python3
"""Optional external SQL competitor, not a mandatory performance gate.

Compare exact grouped duplicate-join count/sum over identical deterministic
in-memory rows. Pair-enumerating and explicit factorized DuckDB SQL are both
measured; Newton performs only the aggregate. Fixture construction is outside
the timed region for every contender. Process startup is excluded from Newton
times; DuckDB execute/fetch includes SQL overhead. Do NOT report as universal
library superiority.
"""
from __future__ import annotations
import argparse
from collections import defaultdict
from datetime import datetime, timezone
import json
import math
import os
from pathlib import Path
import random
import statistics
import subprocess
import sys
import time
import resource

import duckdb

GROUPS=16
LEFT_ROWS=32000
RIGHT_ROWS=16000
WIDTHS=(64,256,4096,65536,262144)


def pct(samples: list[float], quantile: float) -> float:
    ordered=sorted(samples)
    v=quantile*(len(ordered)-1)
    lower=int(v)
    return ordered[lower]+(ordered[min(lower+1,len(ordered)-1)]-ordered[lower])*(v-lower)


def newton(binary: Path, width: int) -> tuple[float, list[int], list[int], int, int, int]:
    proc=subprocess.run(
        [str(binary.resolve(strict=True)),str(width),str(LEFT_ROWS),str(RIGHT_ROWS)],
        capture_output=True,text=True,check=True,timeout=90)
    d=json.loads(proc.stdout)
    if (d["width"],d["left_rows"],d["right_rows"]) != (width,LEFT_ROWS,RIGHT_ROWS):
        raise ValueError("Newton fixture parameters changed")
    return (d["elapsed_ms"],d["counts"],d["sums"],d["pairs"],
            d["scratch_bytes"],d["process_peak_rss_kib"])


def result_from_sql(rows) -> tuple[list[int],list[int],int]:
    counts=[0]*GROUPS
    sums=[0]*GROUPS
    for group,count,value in rows:
        g=int(group)
        counts[g]=int(count)
        sums[g]=int(value)
    return counts,sums,sum(counts)


def setup(con, width: int) -> None:
    con.execute("DROP TABLE IF EXISTS newton_left")
    con.execute("DROP TABLE IF EXISTS newton_right")
    con.execute(f"""
        CREATE TEMP TABLE newton_left AS
        SELECT ((i * 17 + 23) % {width})::UBIGINT AS k,
               ((i * 13 + 3) % 16)::UINTEGER AS g,
               ((i * 29 + 5) % 200)::UBIGINT AS v
        FROM range(0, {LEFT_ROWS}) AS rows(i)
    """)
    con.execute(f"""
        CREATE TEMP TABLE newton_right AS
        SELECT ((i * 31 + 7) % {width})::UBIGINT AS k
        FROM range(0, {RIGHT_ROWS}) AS rows(i)
    """)


def run() -> None:
    p=argparse.ArgumentParser()
    p.add_argument("--binary",type=Path,required=True)
    p.add_argument("--out",type=Path,required=True)
    p.add_argument("--rounds",type=int,default=39)
    args=p.parse_args()
    if not 15 <= args.rounds <= 201:
        p.error("rounds must be 15 to 201")
    args.out.mkdir(parents=True,exist_ok=True)
    con=duckdb.connect(database=":memory:")
    raw="""
        SELECT l.g, COUNT(*)::UBIGINT, SUM(l.v)::UBIGINT
        FROM newton_left l
        JOIN newton_right r ON l.k=r.k
        GROUP BY l.g ORDER BY l.g
    """
    fact="""
        WITH right_frequency AS
          (SELECT k, COUNT(*)::UBIGINT AS frequency
           FROM newton_right GROUP BY k)
        SELECT l.g, SUM(f.frequency)::UBIGINT,
               SUM(l.v*f.frequency)::UBIGINT
        FROM newton_left l
        JOIN right_frequency f ON l.k=f.k
        GROUP BY l.g ORDER BY l.g
    """
    report={
        "date_utc":datetime.now(timezone.utc).isoformat(),
        "duckdb_version":duckdb.__version__,
        "compiler_info":"Saved by accompanying workflow",
        "notes":[
            "All methods use identical deterministic in-memory keys, groups and values.",
            "Only build+aggregate is timed; fixture creation/loading is excluded.",
            "Newton timing excludes process startup and output serialization.",
            "DuckDB timing includes SQL execution and fetch of group rows.",
            "Process RSS measures whole different runtimes; never library-only memory.",
            "Default DuckDB factorized and full JOIN plans both included.",
            "Results are exploratory on a shared GitHub VM, not a production claim."
        ],
        "cases":[]
    }
    methods=["newton_dense","duckdb_factorized_1t","duckdb_full_join_1t","duckdb_factorized_4t"]
    with (args.out/"duckdb-vs-newton.csv").open("w",encoding="utf8") as f:
        f.write("domain,method,pairs,p50_ms,p95_ms,p99_ms,rows_per_s,"
                "process_peak_rss_kib,newton_scratch_bytes\n")
        for width in WIDTHS:
            setup(con,width)
            expected=None
            timings={method:[] for method in methods}
            memory={}
            for _ in range(2):
                con.execute("SET threads=1")
                con.execute(fact).fetchall()
                newton(args.binary,width)
            for cycle in range(args.rounds):
                order=methods[:]
                random.Random(0x51d81+width+cycle).shuffle(order)
                for method in order:
                    if method=="newton_dense":
                        elapsed,counts,sums,pairs,scratch,rss=newton(args.binary,width)
                        memory[method]=(rss,scratch)
                    else:
                        con.execute("SET threads=4" if method.endswith("4t") else "SET threads=1")
                        query=raw if method=="duckdb_full_join_1t" else fact
                        start=time.perf_counter_ns()
                        rows=con.execute(query).fetchall()
                        elapsed=(time.perf_counter_ns()-start)/1e6
                        counts,sums,pairs=result_from_sql(rows)
                        memory[method]=(resource.getrusage(resource.RUSAGE_SELF).ru_maxrss,0)
                    signature=(tuple(counts),tuple(sums),pairs)
                    if expected is None:
                        expected=signature
                    if signature!=expected:
                        raise RuntimeError(
                            f"Incorrect result: width={width} method={method}: {signature} != {expected}")
                    if not math.isfinite(elapsed) or elapsed<=0:
                        raise ValueError("Invalid elapsed time")
                    timings[method].append(elapsed)
            case={"domain":width,"pairs":expected[2],"samples_ms":timings}
            for method in methods:
                data=timings[method]
                m50=pct(data,0.5)
                case[method]={
                    "p50_ms":m50,"p95_ms":pct(data,0.95),"p99_ms":pct(data,0.99),
                    "throughput_rows_s":(LEFT_ROWS+RIGHT_ROWS)/(m50/1000),
                    "process_peak_rss_kib":memory[method][0],
                    "newton_scratch_bytes":memory[method][1] if method=="newton_dense" else None,
                }
                f.write(f"{width},{method},{expected[2]},"
                        f"{m50:.6f},{pct(data,0.95):.6f},{pct(data,0.99):.6f},"
                        f"{(LEFT_ROWS+RIGHT_ROWS)/(m50/1000):.1f},"
                        f"{memory[method][0]},{memory[method][1] if method=='newton_dense' else ''}\n")
                print(f"width {width}: {method} p50={m50:.3f}ms p99={pct(data,0.99):.3f}ms",flush=True)
            report["cases"].append(case)
            con.execute("SET threads=1")
            (args.out/f"duckdb-plan-{width}.txt").write_text(
                "Explicit factorized query:\n"+str(con.execute("EXPLAIN "+fact).fetchall())+
                "\nOriginal JOIN query:\n"+str(con.execute("EXPLAIN "+raw).fetchall()),
                encoding="utf-8")
    (args.out/"duckdb-vs-newton.json").write_text(
        json.dumps(report,indent=2)+"\n",encoding="utf-8")


if __name__=="__main__":
    try: run()
    except Exception as error:
        print(f"External competitor failed closed: {error}",file=sys.stderr)
        raise
