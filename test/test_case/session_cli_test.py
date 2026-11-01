"""Exercise session stores through the real STP command parser."""
import os
import pathlib
import subprocess
import sys
import tempfile
import unittest

STP = pathlib.Path(sys.argv.pop(1)).resolve()


class SessionStoreTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix="stp-session-")
        self.root = pathlib.Path(self.temp.name)
        (self.root / "input.bench").write_text(
            "INPUT(a)\nINPUT(b)\nOUTPUT(y)\ny = LUT 0x6 (a,b)\n"
        )

    def tearDown(self):
        self.temp.cleanup()

    def run_stp(self, commands, errors=False):
        result = subprocess.run(
            [str(STP), "-c", commands], cwd=self.root,
            capture_output=True, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        if not errors:
            self.assertNotIn("[e]", result.stdout)
            self.assertNotIn("[w]", result.stdout)
        return result.stdout

    def dot(self, filename):
        return (self.root / filename).read_text()

    def test_truth_history_and_selection(self):
        out = self.run_stp(
            "read_truth a0; read_truth 0XE8; store -t; "
            "show -o latest.dot; current -t 0; show -o first.dot"
        )
        self.assertIn("Truth [1] 0xe8", out)
        self.assertIn("*  1:", out)
        self.assertIn('label="MAJ"', self.dot("latest.dot"))
        self.assertIn('tooltip="0xa0"', self.dot("first.dot"))

    def test_latest_type_and_explicit_store(self):
        self.run_stp(
            "read_truth e8; read_bench input.bench; show -o bench.dot; "
            "show -t -o truth.dot; show -b -o bench2.dot; "
            "read_truth 8; show -o newest.dot; store -t -b"
        )
        self.assertIn('label="XOR"', self.dot("bench.dot"))
        self.assertIn('label="MAJ"', self.dot("truth.dot"))
        self.assertEqual(self.dot("bench.dot"), self.dot("bench2.dot"))
        self.assertIn('label="AND"', self.dot("newest.dot"))

    def test_snapshots_survive_overwritten_files(self):
        self.run_stp(
            "read_bench input.bench; dsd A0 -o input.bench; "
            "current -b 0; show -o old.dot; current -b 1; show -o new.dot"
        )
        self.assertIn('label="XOR"', self.dot("old.dot"))
        self.assertNotIn('label="XOR"', self.dot("new.dot"))
        self.assertIn('label="AND"', self.dot("new.dot"))

    def test_decompositions_use_stored_truth_and_append_results(self):
        out = self.run_stp(
            "read_truth A0; dsd -o result.bench; lutxx -o result.bench; "
            "approximate_dsd -o result.bench; idsd -o result.bench; "
            "store -b; show -o result.dot; current -b 0; show -o oldest.dot"
        )
        self.assertIn("*  3:", out)
        self.assertIn('label="AND"', self.dot("result.dot"))
        self.assertIn('label="AND"', self.dot("oldest.dot"))

    def test_binary_truth_keeps_its_input_count(self):
        out = self.run_stp(
            "read_truth 0b10; store -t; show -o unary.dot; "
            "lutxx -o unary.bench; store -b"
        )
        self.assertIn("0x2 (1 inputs)", out)
        self.assertIn("(1 PI, 1 PO, 1 LUTs)", out)
        self.assertNotIn("pi1 [", self.dot("unary.dot"))

    def test_invalid_reads_do_not_change_current_entry(self):
        out = self.run_stp(
            "read_truth e8; read_truth xyz; read_bench missing.bench; "
            "show -o kept.dot; store -t; store -b", errors=True
        )
        self.assertIn("[e]", out)
        self.assertIn('label="MAJ"', self.dot("kept.dot"))
        self.assertNotIn("Truth [1]", out)
        self.assertIn("no BENCH circuit in store", out)

    def test_empty_session_reports_errors(self):
        out = self.run_stp("show --no-open; dsd; lutxx; store -t", errors=True)
        self.assertIn("store is empty", out)
        self.assertIn("no truth table in store", out)

    def test_missing_graphviz_falls_back_to_dot(self):
        env = dict(os.environ, PATH=str(self.root))
        result = subprocess.run(
            [str(STP), "-c", "read_truth 8; show; show -o explicit.dot"],
            cwd=self.root, env=env, capture_output=True, text=True, timeout=30,
        )
        self.assertEqual(result.returncode, 0)
        self.assertIn("Only DOT was generated", result.stdout)
        self.assertIn("brew install graphviz", result.stdout)
        self.assertNotIn("[e]", result.stdout)
        self.assertEqual(result.stdout.count("[w]"), 1)
        self.assertEqual(result.stderr, "")
        self.assertTrue((self.root / "truth_0.dot").exists())
        self.assertTrue((self.root / "explicit.dot").exists())
        self.assertFalse((self.root / "truth_0.png").exists())

    def test_installed_renderer_failure_is_still_an_error(self):
        renderer = self.root / "dot"
        renderer.write_text("#!/bin/sh\nexit 2\n")
        renderer.chmod(0o755)
        result = subprocess.run(
            [str(STP), "-c", "read_truth 8; show"], cwd=self.root,
            env=dict(os.environ, PATH=str(self.root)), capture_output=True,
            text=True, timeout=30,
        )
        self.assertIn("[e] Image rendering failed", result.stdout)
        self.assertNotIn("Only DOT was generated", result.stdout)
        self.assertTrue((self.root / "truth_0.dot").exists())


if __name__ == "__main__":
    unittest.main()
