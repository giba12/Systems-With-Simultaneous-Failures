"""End-to-end tests of the experiment runner, the analysis and the plots.
(TC-G-01 .. TC-G-07, EXP-001, EXP-002, EXP-003, NFR-006)"""
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from support import ANALYZE, HYPOTHESES, PYTHON_DIR, analyze_available

import run_experiments as runner

try:
    import pandas  # noqa: F401
    HAVE_PANDAS = True
except ImportError:  # pragma: no cover
    HAVE_PANDAS = False
try:
    import matplotlib  # noqa: F401
    HAVE_MATPLOTLIB = True
except ImportError:  # pragma: no cover
    HAVE_MATPLOTLIB = False


@unittest.skipUnless(analyze_available(), f"analyze executable not found at {ANALYZE}")
class ExperimentRunner(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)

    def run_into(self, folder, names, **kwargs):
        for name in names:
            runner.run_experiment(name, ANALYZE, self.root / folder, HYPOTHESES, runner.DEFAULT_SEED, **kwargs)

    # TC-G-07: no hypothesis, no experiment
    def test_refuses_to_run_without_a_hypothesis(self):
        empty = self.root / "none"
        empty.mkdir()
        with self.assertRaises(runner.ExperimentError) as caught:
            runner.run_experiment("exp1", ANALYZE, self.root / "out", empty, 1)
        self.assertIn("BEFORE", str(caught.exception))
        (empty / "exp1_redundancy.md").write_text("   \n")  # an empty hypothesis does not count either
        with self.assertRaises(runner.ExperimentError):
            runner.run_experiment("exp1", ANALYZE, self.root / "out", empty, 1)
        self.assertFalse((self.root / "out" / "exp1").exists())

    # TC-G-01: the deterministic result file is byte-identical across runs
    def test_results_are_reproducible_byte_for_byte(self):
        names = ["exp1", "exp2", "exp3", "exp4"]
        self.run_into("first", names, max_n=8)
        self.run_into("second", names, max_n=8)
        for name in names:
            first = (self.root / "first" / name / f"{name}.json").read_bytes()
            second = (self.root / "second" / name / f"{name}.json").read_bytes()
            self.assertEqual(first, second, f"{name}.json differs between two runs")

    # TC-G-02 / TC-G-07: what a result file contains
    def test_result_files_carry_their_provenance(self):
        self.run_into("out", ["exp3"])
        folder = self.root / "out" / "exp3"
        deterministic = json.loads((folder / "exp3.json").read_text())
        volatile = json.loads((folder / "exp3.timing.json").read_text())
        hypothesis = HYPOTHESES / "exp3_equal_failure_counts.md"
        self.assertEqual(deterministic["seed"], runner.DEFAULT_SEED)
        self.assertRegex(deterministic["tool_version"], r"^\d+\.\d+\.\d+$")
        self.assertEqual(deterministic["hypothesis"]["sha256"], runner.sha256_of(hypothesis))
        self.assertEqual((folder / "hypothesis.md").read_text(), hypothesis.read_text())
        self.assertTrue((folder / "configs" / "satellite_support.json").is_file())
        self.assertIn("settings", deterministic)
        self.assertNotIn("timing", json.dumps(deterministic))  # volatile numbers live elsewhere
        self.assertTrue(volatile["hypothesis_written_before_run"])
        self.assertLessEqual(volatile["hypothesis_modified_utc"], volatile["started_utc"])

    # TC-G-04: Experiment 1 content
    def test_experiment_1_reports_what_the_plan_asks_for(self):
        self.run_into("out", ["exp1"])
        data = json.loads((self.root / "out" / "exp1" / "exp1.json").read_text())
        labels = [item["label"] for item in data["raw_results"]]
        self.assertEqual(labels, ["series6", "one_pair", "satellite", "parallel6"])
        for item in data["raw_results"]:
            results = item["output"]["results"]
            for key in ("mttf_all_working", "system_signature", "operational_states"):
                self.assertIn(key, results)
            self.assertTrue(item["output"]["verification"]["passed"])
        timing = json.loads((self.root / "out" / "exp1" / "exp1.timing.json").read_text())
        self.assertTrue(all("analysis_ms_median_of_5" in run for run in timing["runs"]))  # runtime is recorded

    # TC-G-05: Experiment 3 finds equal failure counts with different MTTF
    def test_experiment_3_finds_equal_failure_counts_with_different_mttf(self):
        self.run_into("out", ["exp3"])
        data = json.loads((self.root / "out" / "exp3" / "exp3.json").read_text())
        states = data["raw_results"][0]["output"]["results"]["states"]
        two_failed = {s["mttf"] for s in states if s["working_count"] == 4}
        self.assertEqual(len(two_failed), 2)
        self.assertEqual(min(two_failed), 0.0)
        self.assertAlmostEqual(max(two_failed), 5 / 12, places=12)

    # TC-G-06: Experiment 4 records the quantities the plan asks for
    def test_experiment_4_records_scaling_quantities(self):
        self.run_into("out", ["exp4"], max_n=10)
        data = json.loads((self.root / "out" / "exp4" / "exp4.json").read_text())
        timing = json.loads((self.root / "out" / "exp4" / "exp4.timing.json").read_text())
        self.assertEqual(data["settings"]["sizes"], [4, 6, 8, 10])
        for item in data["raw_results"]:
            self.assertEqual(item["output"]["results"]["phi_calls"], 2 ** item["n"])
        self.assertTrue(all(run["analysis_ms"] > 0 and run["peak_rss_kb"] > 0 for run in timing["runs"]))
        self.assertTrue(all(run["verification_ms"] is not None for run in timing["runs"]))  # n <= 14: verified

    def test_command_line_runs_and_reports_errors(self):
        self.assertEqual(runner.main(["exp3", "--analyze", ANALYZE, "--out", str(self.root / "cli")]), 0)
        self.assertTrue((self.root / "cli" / "exp3" / "exp3.json").is_file())
        self.assertEqual(runner.main(["exp3", "--analyze", "/no/such/analyze", "--out", str(self.root / "x")]), 1)
        self.assertEqual(runner.main(["exp3", "--analyze", ANALYZE, "--hypotheses", str(self.root), "--out",
                                      str(self.root / "y")]), 1)


@unittest.skipUnless(analyze_available() and HAVE_PANDAS, "needs the analyze executable and pandas")
class AnalysisAndPlots(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.root = Path(cls.tmp.name)
        for name in ("exp1", "exp2", "exp3", "exp4"):
            runner.run_experiment(name, ANALYZE, cls.root, HYPOTHESES, runner.DEFAULT_SEED, max_n=12)
        import analyze_results
        cls.analysis = analyze_results
        cls.outcomes = {name: analyze_results.analyze(cls.root, name) for name in ("exp1", "exp2", "exp3", "exp4")}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_tables_and_verdict_files_are_written(self):
        for name in ("exp1", "exp2", "exp3", "exp4"):
            self.assertTrue((self.root / name / "hypothesis_check.md").is_file())
            self.assertTrue(list((self.root / name / "tables").glob("*.csv")))

    def test_experiments_1_and_3_support_every_prediction(self):
        for name in ("exp1", "exp3"):
            _, checks, rejected = self.outcomes[name]
            self.assertEqual(rejected, [], f"{name} rejected: {rejected}")
            self.assertGreater(len(checks), 8)

    def test_experiment_2_numbers_match_the_closed_form_and_the_corrected_expectation(self):
        _, checks, _ = self.outcomes["exp2"]
        verdicts = {name: passed for name, _, passed, _ in checks}
        self.assertTrue(verdicts["lethal-shock closed form"])
        self.assertTrue(verdicts["absolute MTTF rises with beta (lethal shock)"])
        self.assertTrue(verdicts["redundancy ratio falls with beta (lethal shock)"])
        table = self.outcomes["exp2"][0]["exp2_curves"]
        lsh = table[(table["model"] == "lsh")].set_index("beta")
        self.assertAlmostEqual(lsh.loc[0.5, "satellite_mttf"], 32 / 35, places=10)
        self.assertAlmostEqual(lsh.loc[0.5, "redundancy_ratio"], 3.2, places=10)

    def test_experiment_4_structural_claims_hold(self):
        _, checks, _ = self.outcomes["exp4"]
        verdicts = {name: passed for name, _, passed, _ in checks}
        self.assertTrue(verdicts["Phi evaluated exactly 2^n times"])
        self.assertTrue(verdicts["peak vectors"])

    def test_results_without_provenance_are_refused(self):
        broken = self.root / "broken"
        (broken / "exp1").mkdir(parents=True)
        (broken / "exp1" / "exp1.json").write_text(json.dumps({"experiment": "exp1"}))
        (broken / "exp1" / "exp1.timing.json").write_text("{}")
        with self.assertRaises(self.analysis.ResultsError):
            self.analysis.load(broken, "exp1")

    @unittest.skipUnless(HAVE_MATPLOTLIB, "needs matplotlib")
    def test_plots_are_drawn_from_the_saved_tables_alone(self):
        import plot_results
        # copy ONLY the tables: no raw results, no analyze executable involved
        only_tables = self.root / "only_tables"
        for name in ("exp1", "exp2", "exp3", "exp4"):
            shutil.copytree(self.root / name / "tables", only_tables / name / "tables")
        self.assertEqual(plot_results.main(["--results", str(only_tables), "--dpi", "60"]), 0)
        pictures = sorted((only_tables / "figures").glob("*.png"))
        self.assertEqual([p.name for p in pictures], ["fig1_redundancy.png", "fig2_common_cause.png",
                                                      "fig3_equal_failure_counts.png", "fig4_scaling.png"])
        for picture in pictures:
            self.assertEqual(picture.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")
            self.assertGreater(picture.stat().st_size, 3000)
        self.assertNotIn("subprocess", (PYTHON_DIR / "plot_results.py").read_text())

    @unittest.skipUnless(HAVE_MATPLOTLIB, "needs matplotlib")
    def test_plotting_without_tables_fails_clearly(self):
        import plot_results
        empty = self.root / "empty_tables"
        (empty / "exp1" / "tables").mkdir(parents=True)
        self.assertEqual(plot_results.main(["--results", str(empty)]), 1)


if __name__ == "__main__":
    unittest.main()
