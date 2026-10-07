import copy
import unittest
import io
import json
import tempfile
from pathlib import Path
from contextlib import redirect_stdout
from unittest.mock import patch
import battle_test
from battle_test import summarize_metrics


def client():
    return {"network_binding": "ab" * 32, "attempted_operations": 5, "submitted_operations": 3,
            "finalized_operations": 2, "attempted": {"files": 5}, "submitted": {"files": 3},
            "finalized": {"files": 2}, "measurement_window_s": 10,
            "submitted_ops_per_s": 0.3, "finalized_ops_per_s": 0.2}


class BenchmarkMetricsTests(unittest.TestCase):
    def test_common_controller_window(self):
        result = summarize_metrics([("a", client()), ("b", client())], 25)
        self.assertEqual(result["attempted_operations"], 10)
        self.assertEqual(result["submitted_operations"], 6)
        self.assertEqual(result["finalized_operations"], 4)
        self.assertEqual(result["finalized_ops_per_s"], 4 / 25)

    def test_rejects_missing_or_old_contract(self):
        with self.assertRaises(KeyError):
            summarize_metrics([("old", {"operations": 3, "operations_per_s": 1})], 10)

    def test_rejects_invalid_evidence(self):
        for key, value in (("finalized_operations", 4), ("submitted_operations", -1),
                           ("measurement_window_s", 0), ("finalized_ops_per_s", 0.9),
                           ("finalized", {"files": 1})):
            with self.subTest(key=key):
                m = copy.deepcopy(client())
                m[key] = value
                with self.assertRaises(ValueError):
                    summarize_metrics([("a", m)], 10)

    def test_rejects_mixed_networks(self):
        other = client()
        other["network_binding"] = "cd" * 32
        with self.assertRaises(ValueError):
            summarize_metrics([("a", client()), ("b", other)], 10)

    def test_rejects_empty_or_invalid_window(self):
        for window in (0, -1, float("nan"), float("inf")):
            with self.assertRaises(ValueError):
                summarize_metrics([("a", client())], window)
        with self.assertRaises(ValueError):
            summarize_metrics([], 10)

    def test_missing_client_metrics_is_failure_with_unknown_counts(self):
        with tempfile.TemporaryDirectory(prefix="cybou-report-test-") as directory:
            root=Path(directory)
            battle=battle_test.Battle.__new__(battle_test.Battle)
            battle.dir=root/"run"; battle.dir.mkdir()
            battle.run_id="20261007-120000"
            battle.samples=[]; battle.provenance={}
            battle.win_clients=lambda: [(battle.dir/"client",("127.0.0.1",29461))]
            battle.wsl_clients=lambda: []
            with patch.object(battle_test,"REPO",root), redirect_stdout(io.StringIO()):
                battle.report()
            self.assertFalse(battle.passed)
            result=json.loads((battle.dir/"benchmark.json").read_text())
            self.assertEqual(result["result"],"FAIL")
            self.assertNotIn("finalized_operations",result)
            self.assertIn("Unknown",(battle.dir/"report.md").read_text())


if __name__ == "__main__":
    unittest.main()
