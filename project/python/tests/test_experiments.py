"""End-to-end tests of the experiment runner, the analysis and the plots.
(TC-G-01 .. TC-G-07, EXP-001, EXP-002, EXP-003, NFR-006)"""
import json
import shutil
import tempfile
import unittest
from pathlib import Path

from support import ANALYZE, HYPOTHESES, PYTHON_DIR, SATELLITE, analyze_available, run_json, without_timing

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

    # Traceability: TC-G-10
    def test_python_layer_only_calls_the_cli(self):
        import ast
        import sys
        tree = ast.parse((PYTHON_DIR / "run_experiments.py").read_text())
        imported = set()
        for node in ast.walk(tree):
            if isinstance(node, ast.Import):
                imported |= {alias.name.split(".")[0] for alias in node.names}
            elif isinstance(node, ast.ImportFrom) and node.module:
                imported.add(node.module.split(".")[0])
        self.assertLessEqual(imported, set(sys.stdlib_module_names),
                             f"run_experiments.py imports non-standard modules: {imported - set(sys.stdlib_module_names)}")
        # every number in the saved results is exactly what the tool printed
        self.run_into("out", ["exp3"])
        saved = json.loads((self.root / "out" / "exp3" / "exp3.json").read_text())["raw_results"][0]["output"]
        direct = without_timing(run_json("--config", SATELLITE, "--model", "exp", "--lambda", "1.0",
                                         "--per-state", "--verify"))
        direct["config_file"] = SATELLITE.name
        self.assertEqual(saved, direct)

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
    # Traceability: TC-G-08
    def test_results_are_reproducible_byte_for_byte(self):
        names = ["exp1", "exp2", "exp3", "exp4", "exp5", "exp6", "exp7"]
        self.run_into("first", names, max_n=8, systems_per_size=2)
        self.run_into("second", names, max_n=8, systems_per_size=2)
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

    # Traceability: TC-G-08
    # Traceability: TC-G-11
    def test_experiment_5_covers_the_cost_grid_and_the_paper_reproduction(self):
        self.run_into("out", ["exp5"])
        data = json.loads((self.root / "out" / "exp5" / "exp5.json").read_text())
        grid = [item for item in data["raw_results"] if item["label"] != "fig1_paper_costs"]
        self.assertEqual(len(grid), (3 + 3) * 45)  # satellite under 3 models + 3 asymmetric systems, 45 cost pairs each
        self.assertEqual({item["label"] for item in grid}, {"satellite", "fig1", "bridge", "eight"})
        self.assertEqual(sorted({item["rho"] for item in grid}), sorted(runner.RHOS))
        self.assertEqual(sorted({item["sigma"] for item in grid}), sorted(runner.SIGMAS))
        # the cost setting of the paper's Fig. 1: three threshold policies, 97/105, 11/15, 1/10
        paper = [item for item in data["raw_results"] if item["label"] == "fig1_paper_costs"][0]
        costs = [p["long_term_mean_cost"] for p in paper["output"]["policies"]["threshold"]["policies"]]
        for got, want in zip(costs, [97 / 105, 11 / 15, 1 / 10]):
            self.assertAlmostEqual(got, want, places=12)
        # every run used c_sys = 1 and c_comp = rho / n
        for item in grid:
            c = item["output"]["policies"]["costs"]
            self.assertEqual(c["system"], 1.0)
            self.assertAlmostEqual(c["component"], item["rho"] / item["output"]["system"]["n"], places=15)
            self.assertAlmostEqual(c["setup"], item["sigma"], places=15)

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
        for name in ("exp1", "exp2", "exp3", "exp4", "exp5", "exp6", "exp7"):
            runner.run_experiment(name, ANALYZE, cls.root, HYPOTHESES, runner.DEFAULT_SEED, max_n=12, systems_per_size=2)
        import analyze_results
        cls.analysis = analyze_results
        cls.outcomes = {name: analyze_results.analyze(cls.root, name) for name in ("exp1", "exp2", "exp3", "exp4", "exp5", "exp6", "exp7")}

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    # Traceability: TC-G-09
    def test_tables_and_verdict_files_are_written(self):
        for name in ("exp1", "exp2", "exp3", "exp4", "exp5", "exp6", "exp7"):
            self.assertTrue((self.root / name / "hypothesis_check.md").is_file())
            self.assertTrue(list((self.root / name / "tables").glob("*.csv")))

    # Predictions about run time depend on the machine and the build type (a Debug build with sanitizers is
    # legitimately slower than 1 ms), so a unit test must not require them; every structural prediction is required.
    TIMING_PREDICTIONS = {"run time"}

    def test_experiments_1_and_3_support_every_structural_prediction(self):
        for name in ("exp1", "exp3"):
            _, checks, rejected = self.outcomes[name]
            structural_rejections = [r for r in rejected if r not in self.TIMING_PREDICTIONS]
            self.assertEqual(structural_rejections, [], f"{name} rejected: {structural_rejections}")
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

    # Traceability: TC-G-11
    def test_experiment_5_predictions_hold_and_the_paper_costs_reproduce(self):
        tables, checks, rejected = self.outcomes["exp5"]
        self.assertEqual(rejected, [], rejected)  # deterministic: no timing involved
        names = [name for name, _, _, _ in checks]
        self.assertTrue(any(name.startswith("reproduction") for name in names))
        self.assertEqual(sum(1 for name in names if name.startswith("5 asymmetric")), 3)
        frame = tables["exp5_policies"]
        sat = frame[(frame["system"] == "satellite") & (frame["model"] == "exp") & (frame["rho"] == 10)]
        self.assertTrue((sat["best_r"] == 4).all())  # expensive components: never repair early

    # Traceability: TC-G-12
    def test_experiment_6_reports_the_rejected_ranking_prediction_as_found(self):
        tables, checks, rejected = self.outcomes["exp6"]
        # five predictions and the two bookkeeping checks hold; prediction 5 was rejected by the data and stays rejected
        self.assertEqual(rejected, ["5: series and satellite rank the models identically"])
        self.assertEqual(len(checks), 7)
        cases = tables["exp6_cases"]
        self.assertEqual(len(cases), 18)  # 6 models x 3 betas
        self.assertTrue(cases["verified"].all())
        # the rejection is not noise: the two rankings are exactly opposite at every beta
        self.assertTrue(tables["exp6_rankings"]["exactly_reversed"].all())
        # the independent Python Psi reproduces the tool's series MTTF, and the lethal-shock satellite is exact
        lsh = cases[(cases["model"] == "lsh") & (cases["beta"] == 0.5)].iloc[0]
        self.assertAlmostEqual(lsh["satellite_mttf"], 32 / 35, places=12)
        self.assertAlmostEqual(lsh["series6_expected"], 1 / (0.5 * 6 + 0.5), places=12)

    # Traceability: TC-G-13
    def test_experiment_7_optimum_is_a_lower_bound_and_the_random_systems_are_reproducible(self):
        tables, checks, _ = self.outcomes["exp7"]
        verdicts = {name: passed for name, _, passed, _ in checks}
        self.assertTrue(verdicts["1: the optimum is a lower bound"])  # theory, independent of the sample drawn
        combos = tables["exp7_combinations"]
        # reduced run: 2 random systems per size x 3 models x 45 cost pairs, plus the satellite (3 models x 45)
        self.assertEqual(len(combos), 135 + 2 * 2 * 3 * 45)
        self.assertTrue((combos["excess"] >= -1e-9).all())
        self.assertLessEqual(int(combos["dinkelbach_passes"].max()), 10)
        # the same seed draws the same systems; another seed draws different ones
        import random
        self.assertEqual(runner.random_path_sets(random.Random(5), 10), runner.random_path_sets(random.Random(5), 10))
        self.assertNotEqual(runner.random_path_sets(random.Random(5), 10), runner.random_path_sets(random.Random(6), 10))
        for n in (6, 10):
            for paths in (runner.random_path_sets(random.Random(s), n) for s in range(20)):
                self.assertTrue(3 <= len(paths) <= 6)
                self.assertTrue(all(2 <= len(p) <= (n + 1) // 2 and p == sorted(set(p)) and 1 <= p[0] and p[-1] <= n for p in paths))

    def test_experiment_4_structural_claims_hold(self):
        _, checks, _ = self.outcomes["exp4"]
        verdicts = {name: passed for name, _, passed, _ in checks}
        self.assertTrue(verdicts["Phi evaluated exactly 2^n times"])
        self.assertTrue(verdicts["peak vectors"])

    # Traceability: TC-G-09
    def test_results_without_provenance_are_refused(self):
        broken = self.root / "broken"
        (broken / "exp1").mkdir(parents=True)
        (broken / "exp1" / "exp1.json").write_text(json.dumps({"experiment": "exp1"}))
        (broken / "exp1" / "exp1.timing.json").write_text("{}")
        with self.assertRaises(self.analysis.ResultsError):
            self.analysis.load(broken, "exp1")

    @unittest.skipUnless(HAVE_MATPLOTLIB, "needs matplotlib")
    # Traceability: TC-G-03, TC-G-09
    def test_plots_are_drawn_from_the_saved_tables_alone(self):
        import plot_results
        # copy ONLY the tables: no raw results, no analyze executable involved
        only_tables = self.root / "only_tables"
        for name in ("exp1", "exp2", "exp3", "exp4", "exp5", "exp6", "exp7"):
            shutil.copytree(self.root / name / "tables", only_tables / name / "tables")
        self.assertEqual(plot_results.main(["--results", str(only_tables), "--dpi", "60"]), 0)
        pictures = sorted((only_tables / "figures").glob("*.png"))
        self.assertEqual([p.name for p in pictures], ["fig1_redundancy.png", "fig2_common_cause.png",
                                                      "fig3_equal_failure_counts.png", "fig4_scaling.png",
                                                      "fig5_repair_policies.png", "fig6_six_models.png", "fig7_distance_to_optimal.png"])
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
