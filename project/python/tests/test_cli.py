"""End-to-end tests of the command line tool `analyze`.   (TC-H-04, TC-E-09, INT-001, INT-003, INT-004)"""
import json
import tempfile
import unittest
from pathlib import Path

from support import (ANALYZE, SATELLITE, analyze_available, run, run_json, without_timing, write_json)


@unittest.skipUnless(analyze_available(), f"analyze executable not found at {ANALYZE}")
class AnalyzeCli(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dir = Path(self.tmp.name)

    def config(self, name, components, structure, **extra):
        document = {"name": name, "components": [{"id": c, "bit": i} for i, c in enumerate(components)],
                    "structure": structure, **extra}
        return write_json(self.dir / f"{name}.json", document)

    # ---- the happy path -------------------------------------------------------------------------------
    def test_help_exits_zero_and_prints_usage(self):
        code, out, _ = run("--help")
        self.assertEqual(code, 0)
        self.assertIn("usage: analyze", out)

    # Traceability: TC-H-04
    def test_satellite_defaults(self):
        output = run_json("--config", SATELLITE)
        results = output["results"]
        self.assertAlmostEqual(results["mttf_all_working"], 0.7, places=12)
        self.assertEqual(results["counting_vector_all_working"], [1, 6, 12, 8, 0, 0, 0])
        for got, want in zip(results["system_signature"], [0, 0.2, 0.4, 0.4, 0, 0]):
            self.assertAlmostEqual(got, want, places=12)
        self.assertEqual(results["operational_states"], 27)
        self.assertEqual(len(results["minimal_path_sets"]), 8)
        self.assertEqual(results["phi_calls"], 64)
        self.assertEqual(results["integer_additions"], 672)   # n (n+1) 2^(n-2), paper Theorem 2.3
        self.assertEqual(results["integer_divisions"], 192)    # n 2^(n-1)
        self.assertEqual(output["system"]["n"], 6)
        self.assertEqual(output["system"]["total_states"], 64)
        self.assertEqual(output["failure_model"]["type"], "independent_exponential")
        self.assertEqual(output["tool"]["name"], "analyze")
        self.assertRegex(output["tool"]["version"], r"^\d+\.\d+\.\d+$")
        self.assertEqual({"analysis_ms", "verification_ms", "policies_ms", "total_ms", "peak_rss_kb"}, set(output["timing"]))
        self.assertGreater(output["timing"]["peak_rss_kb"], 0)

    # Traceability: TC-H-04
    def test_per_state_records(self):
        states = run_json("--config", SATELLITE, "--per-state")["results"]["states"]
        self.assertEqual(len(states), 64)
        by_mask = {s["mask"]: s for s in states}
        self.assertEqual(by_mask[62]["failed"], ["P1"])
        self.assertAlmostEqual(by_mask[62]["mttf"], 8 / 15, places=12)
        self.assertEqual(by_mask[62]["counting_vector"], [1, 4, 4, 0, 0, 0])
        self.assertEqual(by_mask[0b111100]["failed"], ["P1", "P2"])
        self.assertFalse(by_mask[0b111100]["operational"])
        self.assertEqual(by_mask[0b111100]["mttf"], 0.0)
        self.assertEqual(by_mask[0b111100]["signature"], [])

    def test_verify_passes_and_reports(self):
        code, out, err = run("--config", SATELLITE, "--verify")
        self.assertEqual(code, 0, err)
        report = json.loads(out)["verification"]
        self.assertTrue(report["performed"])
        self.assertTrue(report["passed"])
        self.assertTrue(report["counting_vectors_match_reference"])
        self.assertTrue(report["mttf_matches_eq6"])
        self.assertLess(report["max_relative_mttf_difference_vs_eq6"], 1e-9)
        checks = report["numerical_checks"]
        self.assertTrue(all(value for value in checks.values() if isinstance(value, bool)), checks)
        self.assertLess(checks["max_signature_sum_error"], 1e-12)

    def test_verification_is_skipped_beyond_the_brute_force_limit(self):
        names = [f"C{i + 1}" for i in range(17)]
        config = self.config("big", names, {"type": "parallel"})
        output = run_json("--config", config, "--verify")
        self.assertFalse(output["verification"]["performed"])
        self.assertIn("limited to 16", output["verification"]["skipped_reason"])

    # Traceability: TC-H-04
    def test_output_is_deterministic_apart_from_timing(self):
        first = run_json("--config", SATELLITE, "--per-state", "--verify")
        second = run_json("--config", SATELLITE, "--per-state", "--verify")
        self.assertEqual(without_timing(first), without_timing(second))

    def test_out_file_matches_standard_output(self):
        target = self.dir / "result.json"
        code, out, _ = run("--config", SATELLITE, "--out", target)
        self.assertEqual(code, 0)
        self.assertEqual(out, "")
        self.assertEqual(without_timing(json.loads(target.read_text())), without_timing(run_json("--config", SATELLITE)))

    # Traceability: TC-H-09
    def test_all_six_common_cause_models_run_verify_and_compare_policies(self):
        names = {"lsh": "lethal_shock", "bfr": "binomial_failure_rate", "lfmo-c": "compound_poisson",
                 "lfmo-g": "gamma_process", "lfmo-ig": "inverse_gaussian", "lfmo-s": "stable_subordinator"}
        mttf = {}
        for short, long_name in names.items():
            code, out, err = run("--config", SATELLITE, "--model", short, "--beta", "0.2", "--verify",
                                 "--c-comp", "0.1", "--c-sys", "1", "--c-set", "0.05")
            self.assertEqual(code, 0, f"{short}: {err}")
            output = json.loads(out)
            self.assertEqual(output["failure_model"]["type"], long_name)
            self.assertTrue(output["verification"]["passed"], short)       # Eq. (7) = Eq. (6), reference, sanity
            self.assertEqual(output["policies"]["threshold"]["policy_count"], 4)
            mttf[short] = output["results"]["mttf_all_working"]
        self.assertEqual(len({round(v, 6) for v in mttf.values()}), 6)     # six different models, six different answers
        # the lethal shock keeps its exact value; the others are calibrated to the same pairwise beta
        self.assertAlmostEqual(mttf["lsh"], 6003 / 7735, places=12)

    # Traceability: TC-H-09
    def test_new_model_parameter_ranges_are_enforced_by_the_tool(self):
        self.assert_fails(["--config", SATELLITE, "--model", "lfmo-ig", "--beta", "0.6"], 1, "beta must lie in (0, 0.585786")
        self.assert_fails(["--config", SATELLITE, "--model", "lfmo-g", "--beta", "1"], 1, "beta must lie in (0, 1)")
        self.assert_fails(["--config", SATELLITE, "--model", "lfmo-c"], 1, "needs a beta factor")
        # the model can come from the configuration file as well
        config = self.config("stable_in_file", ["A", "B", "C"], {"type": "series"},
                             failure_model={"type": "stable_subordinator", "beta": 0.5})
        output = run_json("--config", config)
        self.assertEqual(output["failure_model"]["type"], "stable_subordinator")
        self.assertAlmostEqual(output["results"]["mttf_all_working"], 1 / (3 ** 0.5849625007211562), places=9)  # series: 1/Psi(3)

    # ---- repair policies -------------------------------------------------------------------------------------
    def fig1_config(self):
        return write_json(self.dir / "fig1.json", {
            "name": "paper_fig1", "components": [{"id": f"x{i + 1}", "bit": i} for i in range(3)],
            "structure": {"type": "path_sets", "paths": [["x1", "x2"], ["x3"]]}})

    # Traceability: TC-H-08
    def test_policies_reproduce_the_papers_figure_one(self):
        output = run_json("--config", self.fig1_config(), "--c-comp", "0.03333333333333333", "--c-sys", "1", "--c-set", "0")
        policies = output["policies"]
        costs = [p["long_term_mean_cost"] for p in policies["threshold"]["policies"]]
        for got, want in zip(costs, [97 / 105, 11 / 15, 1 / 10]):
            self.assertAlmostEqual(got, want, places=12)
        self.assertEqual(policies["threshold"]["policy_count"], 3)
        self.assertEqual(policies["threshold"]["best_index"], 2)
        self.assertEqual(policies["r_out_of_n"]["best_r"], 1)
        self.assertAlmostEqual(policies["corrective"]["long_term_mean_cost"], 97 / 105, places=12)
        self.assertAlmostEqual(policies["corrective"]["mean_cycle_length"], 7 / 6, places=12)
        self.assertIn("policies_ms", output["timing"])

    # Traceability: TC-H-08
    def test_policies_are_absent_without_costs_and_work_for_every_failure_model(self):
        self.assertIsNone(run_json("--config", SATELLITE)["policies"])
        for model_args in ([], ["--model", "lsh", "--beta", "0.3"], ["--model", "bfr", "--beta", "0.3"]):
            output = run_json("--config", SATELLITE, *model_args, "--c-comp", "0.1", "--c-sys", "1", "--c-set", "0.05")
            p = output["policies"]
            self.assertEqual(p["threshold"]["policy_count"], 4)
            self.assertEqual(len(p["r_out_of_n"]["policies"]), 6)
            self.assertGreater(p["corrective"]["long_term_mean_cost"], 0)
            self.assertAlmostEqual(p["threshold"]["best_long_term_mean_cost"],
                                   p["threshold"]["policies"][p["threshold"]["best_index"]]["long_term_mean_cost"], places=15)

    # Traceability: TC-H-08
    def test_policy_option_errors(self):
        self.assert_fails(["--config", SATELLITE, "--c-comp", "0.1"], 2, "must be given together")
        self.assert_fails(["--config", SATELLITE, "--c-comp", "0.1", "--c-sys", "1"], 2, "must be given together")
        self.assert_fails(["--config", SATELLITE, "--c-comp", "-1", "--c-sys", "1", "--c-set", "0"], 1,
                          "repair costs must be finite and non-negative")
        big = self.config("fifteen", [f"C{i}" for i in range(15)], {"type": "parallel"})
        self.assert_fails(["--config", big, "--c-comp", "0.1", "--c-sys", "1", "--c-set", "0"], 1, "between 2 and 14")

    # ---- the verification must really fail when something is wrong ---------------------------------------
    # Traceability: TC-H-06
    def test_verification_detects_a_miscounted_vector(self):
        code, out, err = run("--config", SATELLITE, "--verify", env={"ANALYZE_FAULT_INJECTION": "counting_vector"})
        self.assertEqual(code, 3, err)
        self.assertIn("VERIFICATION FAILED", err)
        report = json.loads(out)["verification"]  # the JSON is still written
        self.assertFalse(report["passed"])
        self.assertFalse(report["counting_vectors_match_reference"])
        self.assertEqual(report["first_mismatch"]["state"], 63)
        self.assertNotEqual(report["first_mismatch"]["sweep"], report["first_mismatch"]["reference"])

    # Traceability: TC-H-06
    def test_verification_failure_exit_code_is_3_not_an_internal_error(self):
        code, out, err = run("--config", SATELLITE, "--verify", env={"ANALYZE_FAULT_INJECTION": "counting_vector"})
        self.assertEqual(code, 3, err)
        self.assertNotIn("internal error", err)

    # Traceability: TC-H-06
    def test_verification_detects_a_drifting_mttf(self):
        code, out, err = run("--config", SATELLITE, "--verify", env={"ANALYZE_FAULT_INJECTION": "mttf"})
        self.assertEqual(code, 3, err)
        report = json.loads(out)["verification"]
        self.assertFalse(report["passed"])
        self.assertFalse(report["mttf_matches_eq6"])
        self.assertGreater(report["max_relative_mttf_difference_vs_eq6"], 1e-4)
        self.assertTrue(report["counting_vectors_match_reference"])  # only the MTTF was damaged

    # Traceability: TC-H-06
    def test_fault_injection_is_inert_without_verify_and_rejects_unknown_values(self):
        code, out, _ = run("--config", SATELLITE, env={"ANALYZE_FAULT_INJECTION": "mttf"})
        self.assertEqual(code, 0)
        self.assertAlmostEqual(json.loads(out)["results"]["mttf_all_working"], 0.7, places=12)
        code, _, err = run("--config", SATELLITE, "--verify", env={"ANALYZE_FAULT_INJECTION": "typo"})
        self.assertEqual(code, 1)
        self.assertIn("unknown ANALYZE_FAULT_INJECTION value", err)

    # ---- failure models ---------------------------------------------------------------------------------
    def test_lethal_shock_matches_the_exact_value(self):
        output = run_json("--config", SATELLITE, "--model", "lsh", "--beta", "0.5")
        self.assertAlmostEqual(output["results"]["mttf_all_working"], 32 / 35, places=12)
        self.assertEqual(output["failure_model"]["type"], "lethal_shock")
        self.assertEqual(output["failure_model"]["parameters"], {"beta": 0.5})

    def test_binomial_failure_rate_and_lambda(self):
        bfr = run_json("--config", SATELLITE, "--model", "bfr", "--beta", "0.5")
        self.assertAlmostEqual(bfr["results"]["mttf_all_working"], 0.760266, places=5)
        fast = run_json("--config", SATELLITE, "--model", "exp", "--lambda", "4")
        self.assertAlmostEqual(fast["results"]["mttf_all_working"], 0.7 / 4, places=12)

    def test_failure_model_can_come_from_the_config_file(self):
        names = ["A", "B", "C"]
        config = self.config("lethal_in_file", names, {"type": "series"},
                             failure_model={"type": "lethal_shock", "beta": 0.2})
        output = run_json("--config", config)
        self.assertEqual(output["failure_model"]["type"], "lethal_shock")
        self.assertAlmostEqual(output["results"]["mttf_all_working"], 1 / (0.8 * 3 + 0.2), places=12)
        # the command line wins over the file
        overridden = run_json("--config", config, "--model", "exp", "--lambda", "2")
        self.assertEqual(overridden["failure_model"]["type"], "independent_exponential")
        self.assertAlmostEqual(overridden["results"]["mttf_all_working"], 1 / (3 * 2), places=12)

    def test_series_and_parallel_structures_in_a_config(self):
        for structure, expected in ((  {"type": "series"}, 1 / 4), ({"type": "parallel"}, 25 / 12)):
            config = self.config("shape_" + structure["type"], list("ABCD"), structure)
            self.assertAlmostEqual(run_json("--config", config)["results"]["mttf_all_working"], expected, places=12)

    # ---- errors: exit codes and messages --------------------------------------------------------------
    def assert_fails(self, args, code, message):
        got_code, _, err = run(*args)
        self.assertEqual(got_code, code, err)
        self.assertIn(message, err)

    # Traceability: TC-H-05
    def test_invalid_input_exits_1_with_a_helpful_message(self):
        self.assert_fails(["--config", self.dir / "nope.json"], 1, "cannot open")
        malformed = self.dir / "bad.json"
        malformed.write_text('{"components": [')
        self.assert_fails(["--config", malformed], 1, "malformed JSON")
        single = self.config("single", ["A"], {"type": "series"})
        self.assert_fails(["--config", single], 1, "got 1")
        duplicate = write_json(self.dir / "dup.json", {"components": [{"id": "A", "bit": 0}, {"id": "A", "bit": 1}],
                                                       "structure": {"type": "series"}})
        self.assert_fails(["--config", duplicate], 1, "duplicate component id 'A'")
        self.assert_fails(["--config", SATELLITE, "--model", "lsh"], 1, "needs a beta factor")
        self.assert_fails(["--config", SATELLITE, "--model", "lsh", "--beta", "1.5"], 1, "beta must lie in [0, 1]")
        self.assert_fails(["--config", SATELLITE, "--lambda", "-1"], 1, "lambda must be a positive finite number")
        self.assert_fails(["--config", SATELLITE, "--out", self.dir / "no_such_dir" / "x.json"], 1, "cannot write")

    # Traceability: TC-H-05
    def test_the_simultaneous_failure_models_refuse_large_systems(self):
        config = self.config("seventeen", [f"C{i}" for i in range(17)], {"type": "parallel"})
        self.assert_fails(["--config", config, "--model", "lsh", "--beta", "0.2"], 1, "max_n must be between 1 and 16")

    # Traceability: TC-H-05
    def test_usage_errors_exit_2(self):
        self.assert_fails([], 2, "--config is required")
        self.assert_fails(["--config", SATELLITE, "--bogus"], 2, "unknown option '--bogus'")
        self.assert_fails(["--config", SATELLITE, "--model", "weibull"], 2, "--model must be one of exp, lsh, bfr, lfmo-c")
        self.assert_fails(["--config", SATELLITE, "--lambda", "fast"], 2, "must be a number")
        self.assert_fails(["--config"], 2, "missing value after --config")


if __name__ == "__main__":
    unittest.main()
