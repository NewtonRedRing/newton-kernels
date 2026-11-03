"""Policy tests: the guard must fail rather than silently omit regressions."""
import importlib.util
from pathlib import Path
import sys
import unittest

FILE = Path(__file__).resolve().parents[1] / "tools" / "performance_guard.py"
spec = importlib.util.spec_from_file_location("newton_performance_guard", FILE)
guard = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = guard
spec.loader.exec_module(guard)


def times(values):
    return [{"median_ms": value} for value in values]


class PerformanceGuardPolicyTests(unittest.TestCase):
    def test_all_original_cases_are_immutable(self):
        cs = guard.cases()
        self.assertEqual(len(cs), 22)
        self.assertEqual(len({c.key for c in cs}), 22)
        self.assertEqual(sum(c.suite == "classic" for c in cs), 6)
        self.assertEqual(sum(c.suite == "extended" for c in cs), 16)
        self.assertEqual(guard.PUBLISHED_ANCHOR,
                         "a3ccc65021181561aec1c8fe284bdeb7c8e1234c")

    def test_confirmed_regression_blocks(self):
        result = guard.decide(times([110] * 5), times([100] * 5), .03, 4)
        self.assertEqual(result["status"], "FAIL")
        self.assertEqual(result["slower_rounds"], 5)

    def test_small_regression_is_visible(self):
        result = guard.decide(times([101] * 5), times([100] * 5), .03, 4)
        self.assertEqual(result["status"], "WARN")

    def test_one_outlier_does_not_block(self):
        result = guard.decide(times([100, 100, 110, 100, 100]),
                              times([100] * 5), .03, 4)
        self.assertEqual(result["status"], "PASS")

    def test_consistent_gain_passes(self):
        result = guard.decide(times([95] * 5), times([100] * 5), .03, 4)
        self.assertEqual(result["status"], "PASS")

    def test_missing_round_fails_closed(self):
        with self.assertRaises(ValueError):
            guard.decide(times([100] * 4), times([100] * 5), .03, 4)

    def test_report_does_not_hide_large_slowdowns(self):
        result = guard.decide(times([140] * 5), times([100] * 5), .03, 4)
        self.assertEqual(result["status"], "FAIL")

    def test_all_reference_data_must_be_real(self):
        case = guard.Case("classic", "scan", 1024, 1)
        self.assertEqual(case.key, "scan:1024:1")

    def test_unusable_timing_samples_fail_closed(self):
        for bad in (0, -1, float("nan"), float("inf"), True, "10"):
            with self.subTest(bad=repr(bad)):
                with self.assertRaises(ValueError):
                    guard.decide(times([bad] * 5), times([100] * 5), .03, 4)
                with self.assertRaises(ValueError):
                    guard.decide(times([100] * 5), times([bad] * 5), .03, 4)

    def test_invalid_result_signatures_fail_closed(self):
        import json
        from types import SimpleNamespace
        from unittest.mock import patch
        case = guard.Case("classic", "scan", 1024, 1)
        result = {
            "operation": "scan", "implementation": "newton",
            "size": 1024, "queries": 1, "seed": 7, "repetitions": 3,
            "checksum": "fabc123", "items": 12, "input_bytes": 1024,
            "median_ms": 1.0, "minimum_ms": 0.5, "maximum_ms": 1.5
        }
        with patch.object(guard.subprocess, "run") as invoke:
            for key, bad in (("checksum", 42), ("checksum", "XX"),
                             ("items", True), ("items", -1),
                             ("input_bytes", "1024"), ("input_bytes", -1),
                             ("median_ms", True), ("median_ms", float("nan"))):
                sample = result.copy()
                sample[key] = bad
                invoke.return_value = SimpleNamespace(
                    returncode=0, stdout=json.dumps(sample), stderr="")
                with self.subTest(key=key, bad=repr(bad)):
                    with self.assertRaises(ValueError):
                        guard.measure(Path(sys.executable), case, 3, 7)
            invoke.return_value = SimpleNamespace(
                returncode=0, stdout=json.dumps(result), stderr="")
            self.assertEqual(guard.measure(Path(sys.executable), case, 3, 7),
                             result)


if __name__ == "__main__":
    unittest.main()
