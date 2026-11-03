"""Tests for the read-only benchmark triage reporter."""
import importlib.util
from pathlib import Path
import tempfile
import unittest
import json

SCRIPT = Path(__file__).resolve().parents[1] / "tools" / "benchmark_report_triage.py"
SPEC = importlib.util.spec_from_file_location("benchmark_report_triage", SCRIPT)
triage = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(triage)


def report(fail=None, delta=0.0):
    rows = []
    for n in range(22):
        key = f"case:{n}:1"
        status = "FAIL" if key == fail else "PASS"
        rows.append({
            "key": key,
            "status": status,
            "comparisons": {
                "base": {"median_change_pct": delta},
                "anchor": {"median_change_pct": delta},
            },
        })
    return {"case_count": 22, "results": rows}


class TriageTests(unittest.TestCase):
    def test_aggregates_failures_without_changing_source(self):
        a = report("case:4:1", 4.0)
        b = report("case:4:1", 6.0)
        summary = triage.summarize([a, b])
        row = next(row for row in summary if row["case"] == "case:4:1")
        self.assertEqual(row["failed_runs"], 2)
        self.assertEqual(row["median_delta_vs_base_pct"], 5.0)

    def test_rejects_missing_or_duplicate_protected_cases(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "results.json"
            data = report()
            data["results"][-1]["key"] = data["results"][0]["key"]
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "duplicate"):
                triage.load(path)

    def test_rejects_nonfinite_data(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "results.json"
            data = report(delta=float("nan"))
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "invalid change"):
                triage.load(path)

    def test_requires_same_case_manifest_across_runs(self):
        data = report()
        shifted = report()
        shifted["results"][0]["key"] = "different:case:1"
        with self.assertRaisesRegex(ValueError, "different protected"):
            triage.summarize([data, shifted])


if __name__ == "__main__":
    unittest.main()
