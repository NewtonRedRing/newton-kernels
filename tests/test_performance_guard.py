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

    def test_all_reference_data_must_be_real(self):
        case = guard.Case("classic", "scan", 1024, 1)
        self.assertEqual(case.key, "scan:1024:1")


if __name__ == "__main__":
    unittest.main()
