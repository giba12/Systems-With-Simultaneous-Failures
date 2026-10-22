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
        self.assertEqual(output["system"]["n"], 6)
        self.assertEqual(output["system"]["total_states"], 64)
        self.assertEqual(output["failure_model"]["type"], "independent_exponential")
        self.assertEqual(output["tool"]["name"], "analyze")
        self.assertRegex(output["tool"]["version"], r"^\d+\.\d+\.\d+$")
        self.assertEqual({"analysis_ms", "verification_ms", "total_ms", "peak_rss_kb"}, set(output["timing"]))
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
        self.assert_fails(["--config", SATELLITE, "--model", "weibull"], 2, "--model must be exp, lsh or bfr")
        self.assert_fails(["--config", SATELLITE, "--lambda", "fast"], 2, "must be a number")
        self.assert_fails(["--config"], 2, "missing value after --config")


if __name__ == "__main__":
    unittest.main()
