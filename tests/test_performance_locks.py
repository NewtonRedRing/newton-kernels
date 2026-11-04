"""Regression tests for append-only, SHA-pinned performance high-water marks."""
from __future__ import annotations

import json
from pathlib import Path
import sys
import tempfile
from types import SimpleNamespace
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import performance_locks as locks

SHA_A = "a" * 40
SHA_B = "b" * 40
EVIDENCE = "https://github.com/NewtonRedRing/newton-kernels/actions/runs/123456789"


def record(case: str = "analytics_join:3000:512", sha: str = SHA_A,
           evidence: str = EVIDENCE) -> dict:
    return {"case": case, "sha": sha, "evidence": evidence}


def write(path: Path, entries: list[dict], version: int = 1) -> None:
    path.write_text(json.dumps({"schema_version": version, "locks": entries}),
                    encoding="utf-8")


class PerformanceHighWaterTests(unittest.TestCase):
    def test_current_config_and_original_manifest_remain_valid(self):
        manifest = locks.load(ROOT / "tools" / "performance-locks.json")
        self.assertIsInstance(manifest, list)
        self.assertEqual(len(locks.guard.cases()), 22)
        self.assertEqual((locks.ROUNDS, locks.REPETITIONS,
                          locks.MAX_REGRESSION, locks.MIN_SLOW_ROUNDS),
                         (5, 11, 3.0, 4))

    def test_lock_schema_and_unknown_cases_fail_closed(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "locks.json"
            for data in (
                [record("invented:1:1")],
                [record(sha="BAD")],
                [record(evidence="https://example.com/claims")],
                [record(), record()],
            ):
                with self.subTest(data=data):
                    write(p, data)
                    with self.assertRaises(ValueError):
                        locks.load(p)
            write(p, [record()], version=2)
            with self.assertRaises(ValueError):
                locks.load(p)
            p.write_text('{"schema_version":1,"locks":null}', encoding="utf-8")
            with self.assertRaises(ValueError):
                locks.load(p)

    def test_append_only_never_drops_or_replaces_earlier_peak(self):
        before = [record()]
        promoted = record(sha=SHA_B)
        locks.validate_upgrade(before, before + [promoted])
        locks.validate_upgrade([], before)
        for downgraded in ([], [promoted],
                           [promoted, before[0]],
                           [dict(before[0], evidence=EVIDENCE + "1")]):
            with self.subTest(downgraded=downgraded):
                with self.assertRaises(ValueError):
                    locks.validate_upgrade(before, downgraded)
        self.assertEqual(locks.checked_references(before + [promoted, record(
            case="analytics_join:12000:2048", sha=SHA_A)]), [SHA_A, SHA_B])

    def test_five_round_pinned_slowdown_blocks(self):
        pin = record()
        case = locks.guard.Case("extended", "analytics_join", 3000, 512)
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            opts = SimpleNamespace(
                candidate_classic=root / "candidate-classic",
                candidate_extended=root / "candidate-extended",
                build_root=root / "pinned", candidate_sha=SHA_B,
                protected_code_identical=0,
            )

            def benchmark(path, *_):
                return {"median_ms": 110.0 if path == opts.candidate_extended
                        else 100.0, "checksum": "f3a", "items": 23,
                        "input_bytes": 4096}

            with patch.object(locks.guard, "cases", return_value=[case]), \
                 patch.object(locks.guard, "measure", side_effect=benchmark):
                result = locks.measure_lock(opts, [pin])
            self.assertEqual(result["failed_cases"], [case.key])
            self.assertEqual(result["results"][0]["comparisons"][SHA_A]["slower_rounds"], 5)
            opts.protected_code_identical = 1
            with patch.object(locks.guard, "cases", return_value=[case]), \
                 patch.object(locks.guard, "measure", side_effect=benchmark):
                identical = locks.measure_lock(opts, [pin])
            self.assertFalse(identical["failed_cases"])
            self.assertEqual(identical["results"][0]["status"], "WARN")

    def test_exact_result_mismatch_fails_closed(self):
        pin = record()
        case = locks.guard.Case("extended", "analytics_join", 3000, 512)
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            opts = SimpleNamespace(
                candidate_classic=root / "candidate-classic",
                candidate_extended=root / "candidate-extended",
                build_root=root / "pinned", candidate_sha=SHA_B,
                protected_code_identical=0,
            )

            def mismatch(path, *_):
                return {"median_ms": 100.0,
                        "checksum": "f3a" if path == opts.candidate_extended
                        else "bad", "items": 23, "input_bytes": 4096}

            with patch.object(locks.guard, "cases", return_value=[case]), \
                 patch.object(locks.guard, "measure", side_effect=mismatch):
                with self.assertRaisesRegex(ValueError, "CORRECTNESS REGRESSION"):
                    locks.measure_lock(opts, [pin])


    def test_promotions_require_complete_passing_final_sha_and_four_faster_rounds(self):
        cases = locks.guard.cases()
        rows = [{"key": case.key, "status": "PASS",
                 "comparisons": {"base": {
                     "median_change_pct": -1.0,
                     "round_changes_pct": [-1.0] * 5}}}
                for case in cases]
        rows[0]["comparisons"]["base"] = {
            "median_change_pct": -3.5,
            "round_changes_pct": [-3.5, -3.5, -3.5, -3.5, 2.0],
        }
        rows[1]["comparisons"]["base"] = {
            "median_change_pct": -8.0,
            "round_changes_pct": [-8.0, -8.0, -8.0, 2.0, 2.0],
        }
        report = {"revisions": {"candidate": SHA_B}, "case_count": 22,
                  "protected_code_identical_to_base": False,
                  "failed_cases": [], "results": rows}
        with tempfile.TemporaryDirectory() as d:
            root = Path(d)
            manifest = root / "locks.json"
            source = root / "report.json"
            output = root / "proposals.json"
            write(manifest, [])
            source.write_text(json.dumps(report), encoding="utf-8")
            opts = SimpleNamespace(manifest=manifest, report=source,
                                   candidate_sha=SHA_B, evidence=EVIDENCE,
                                   out=output)
            self.assertEqual(locks.propose(opts), 0)
            proposed = json.loads(output.read_text(encoding="utf-8"))[
                "proposed_locks"]
            self.assertEqual(proposed, [record(cases[0].key, SHA_B)])
            write(manifest, proposed)
            self.assertEqual(locks.propose(opts), 0)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))[
                "proposed_locks"], [])
            report["protected_code_identical_to_base"] = True
            source.write_text(json.dumps(report), encoding="utf-8")
            write(manifest, [])
            self.assertEqual(locks.propose(opts), 0)
            self.assertEqual(json.loads(output.read_text(encoding="utf-8"))[
                "proposed_locks"], [])
            report["protected_code_identical_to_base"] = False
            source.write_text(json.dumps(report), encoding="utf-8")
            opts.candidate_sha = SHA_A
            with self.assertRaises(ValueError):
                locks.propose(opts)
            opts.candidate_sha = SHA_B
            report["failed_cases"] = [cases[-1].key]
            source.write_text(json.dumps(report), encoding="utf-8")
            with self.assertRaises(ValueError):
                locks.propose(opts)


if __name__ == "__main__":
    unittest.main()
