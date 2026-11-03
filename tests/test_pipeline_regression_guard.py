"""Regression gate policy unit tests for current CSV pipeline benchmarks."""
import importlib.util
from pathlib import Path
import sys
import unittest

FILE = Path(__file__).resolve().parents[1] / "tools" / "pipeline_regression_guard.py"
spec = importlib.util.spec_from_file_location("pipeline_guard", FILE)
guard = importlib.util.module_from_spec(spec)
sys.modules[spec.name] = guard
spec.loader.exec_module(guard)


class PipelineRegressionPolicyTests(unittest.TestCase):
    def test_six_existing_pipeline_cases(self):
        self.assertEqual(len(guard.CASES), 6)
        self.assertEqual(set(m for _, m in guard.CASES),
                         {"owned", "fused", "fast"})
        self.assertEqual(set(size for size, _ in guard.CASES),
                         {100_000, 1_000_000})

    def test_confirmed_regression_blocks(self):
        self.assertEqual(guard.verdict([0.10] * 7, .03, 5), "FAIL")

    def test_small_drift_visible(self):
        self.assertEqual(guard.verdict([0.01] * 7, .03, 5), "WARN")

    def test_speedup_passes(self):
        self.assertEqual(guard.verdict([-0.05] * 7, .03, 5), "PASS")

    def test_missing_data_rejected(self):
        with self.assertRaises(ValueError):
            guard.verdict([], .03, 5)

    def test_boolean_and_nonfinite_deltas_fail_closed(self):
        for bad in (True, False, float("inf"), float("-inf"),
                    float("nan"), "0.01"):
            with self.subTest(bad=repr(bad)):
                with self.assertRaises(ValueError):
                    guard.verdict([0.0, bad, 0.0], .03, 2)


if __name__ == "__main__":
    unittest.main()
