// main.cpp - the command-line tool "analyze".                (Plan: Day 8; INT-001, INT-003, INT-004)
//
// USAGE
//   analyze --config configs/satellite_support.json [--model exp|lsh|bfr] [--lambda L] [--beta B]
//           [--per-state] [--verify] [--out results/run1.json]
//
// WHAT IT DOES
//   1. load and validate the system from the JSON configuration
//   2. choose the failure model: the command line wins over the file's "failure_model" section
//   3. run ONE level sweep (paper Algorithm 1); at every state compute the MTTF (Eq. (7))
//   4. optionally (--verify) check the result against the independent brute-force reference, the
//      direct solution of Eq. (6), and some numerical sanity checks
//   5. write everything as JSON; the volatile numbers (run times, memory) live under "timing" so the
//      rest of the file is byte-for-byte reproducible
//
// EXIT CODES   0 success · 1 invalid input or model (message on stderr) · 2 usage error ·
//              3 verification failed (the JSON is still written)
#include <sys/resource.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "reliability/errors.hpp"
#include "reliability/failure_model.hpp"
#include "reliability/mttf.hpp"
#include "reliability/reference.hpp"
#include "reliability/signature.hpp"
#include "reliability/state.hpp"
#include "reliability/system_model.hpp"

#ifndef RELIABILITY_VERSION
#define RELIABILITY_VERSION "unknown"
#endif

namespace {

using json = nlohmann::json;
using namespace reliability;

constexpr int kExitOk = 0;
constexpr int kExitInvalid = 1;
constexpr int kExitUsage = 2;
constexpr int kExitVerificationFailed = 3;

// Tolerances of the verification step. The MTTF values of different routes agree to ~1e-12 for the
// exponential model and to ~2e-10 at the largest simultaneous-failure size (n = 16).
constexpr double kMttfTolerance = 1e-9;
constexpr double kSignatureTolerance = 1e-12;

class UsageError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

struct Options {
  std::string config;
  std::optional<std::string> model;
  std::optional<double> lambda;
  std::optional<double> beta;
  std::optional<std::string> out;
  bool per_state = false;
  bool verify = false;
  bool help = false;
};

const char* kUsage =
    "usage: analyze --config <file.json> [--model exp|lsh|bfr] [--lambda L] [--beta B]\n"
    "               [--per-state] [--verify] [--out <file.json>]\n"
    "\n"
    "  --config     system definition (see configs/satellite_support.json)\n"
    "  --model      failure model: exp (independent exponential), lsh (lethal shock),\n"
    "               bfr (binomial failure rate). Default: the file's \"failure_model\", else exp\n"
    "  --lambda     rate of the exponential model (default 1)\n"
    "  --beta       pairwise beta factor of lsh / bfr, in [0,1] (bfr: in (0,1])\n"
    "  --per-state  include the counting vector, signature and MTTF of every state\n"
    "  --verify     cross-check against the brute-force reference and Eq. (6)\n"
    "  --out        write the JSON here instead of to standard output\n";

double parse_number(const std::string& flag, const std::string& text) {
  try {
    std::size_t used = 0;
    const double value = std::stod(text, &used);
    if (used != text.size()) {
      throw std::invalid_argument("trailing characters");
    }
    return value;
  } catch (const std::exception&) {
    throw UsageError("the value of " + flag + " must be a number, got '" + text + "'");
  }
}

Options parse_args(int argc, char** argv) {
  Options options;
  for (int i = 1; i < argc; ++i) {
    const std::string flag = argv[i];
    auto value = [&]() -> std::string {
      if (i + 1 >= argc) {
        throw UsageError("missing value after " + flag);
      }
      return argv[++i];
    };
    if (flag == "--help" || flag == "-h") {
      options.help = true;
    } else if (flag == "--config") {
      options.config = value();
    } else if (flag == "--model") {
      options.model = value();
    } else if (flag == "--lambda") {
      options.lambda = parse_number(flag, value());
    } else if (flag == "--beta") {
      options.beta = parse_number(flag, value());
    } else if (flag == "--out") {
      options.out = value();
    } else if (flag == "--per-state") {
      options.per_state = true;
    } else if (flag == "--verify") {
      options.verify = true;
    } else {
      throw UsageError("unknown option '" + flag + "'");
    }
  }
  if (!options.help && options.config.empty()) {
    throw UsageError("--config is required");
  }
  if (options.model && *options.model != "exp" && *options.model != "lsh" && *options.model != "bfr") {
    throw UsageError("--model must be exp, lsh or bfr, got '" + *options.model + "'");
  }
  return options;
}

json read_json_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw ModelError("cannot open configuration file '" + path + "'");
  }
  try {
    return json::parse(in);
  } catch (const json::parse_error& e) {
    throw ModelError("malformed JSON in '" + path + "': " + e.what());
  }
}

// The failure model section of the configuration, or null.
json config_failure_model(const json& config) {
  if (config.is_object() && config.contains("failure_model") && config.at("failure_model").is_object()) {
    return config.at("failure_model");
  }
  return json(nullptr);
}

struct ChosenModel {
  std::unique_ptr<FailureModel> model;
  json description;
};

// ALGORITHM
//   type   = --model if given, else the file's failure_model.type, else exp
//   exp    : lambda = --lambda, else the file's lambda, else 1
//   lsh,bfr: beta   = --beta,   else the file's beta, else error (there is no sensible default)
ChosenModel choose_failure_model(const Options& options, const json& section, int n) {
  std::string type = "independent_exponential";
  if (options.model) {
    type = *options.model == "exp" ? "independent_exponential"
                                   : (*options.model == "lsh" ? "lethal_shock" : "binomial_failure_rate");
  } else if (section.is_object() && section.contains("type") && section.at("type").is_string()) {
    type = section.at("type").get<std::string>();
  }
  const bool same_type = section.is_object() && section.contains("type") && section.at("type") == type;
  auto from_file = [&](const char* key) -> std::optional<double> {
    if (same_type && section.contains(key) && section.at(key).is_number()) {
      return section.at(key).get<double>();
    }
    return std::nullopt;
  };

  ChosenModel chosen;
  if (type == "independent_exponential") {
    const double lambda = options.lambda.value_or(from_file("lambda").value_or(1.0));
    chosen.model = make_exponential(lambda);
    chosen.description = {{"type", type}, {"name", chosen.model->name()}, {"parameters", {{"lambda", lambda}}}};
  } else if (type == "lethal_shock" || type == "binomial_failure_rate") {
    const std::optional<double> beta = options.beta ? options.beta : from_file("beta");
    if (!beta) {
      throw ModelError("the " + type + " model needs a beta factor: pass --beta B (or put \"beta\" in the "
                       "configuration's failure_model)");
    }
    chosen.model = type == "lethal_shock" ? make_lethal_shock(*beta, n) : make_binomial_failure_rate(*beta, n);
    chosen.description = {{"type", type}, {"name", chosen.model->name()}, {"parameters", {{"beta", *beta}}}};
  } else {
    throw ModelError("unknown failure model type '" + type +
                     "' (expected independent_exponential, lethal_shock or binomial_failure_rate)");
  }
  return chosen;
}

json failed_ids(const SystemModel& model, StateMask x) {
  json ids = json::array();
  for (int i = 0; i < model.n(); ++i) {
    if (!is_working(x, i)) {
      ids.push_back(model.components()[static_cast<std::size_t>(i)].id);
    }
  }
  return ids;
}

json component_ids(const SystemModel& model, StateMask x) {
  json ids = json::array();
  for (int i = 0; i < model.n(); ++i) {
    if (is_working(x, i)) {
      ids.push_back(model.components()[static_cast<std::size_t>(i)].id);
    }
  }
  return ids;
}

// Peak resident memory of THIS process in kB. getrusage()'s ru_maxrss is inherited across fork+exec on
// Linux, so a process started by a bigger parent (such as the Python experiment runner) would report the
// parent's memory; VmHWM in /proc/self/status belongs to the new address space and is the true figure.
std::int64_t peak_rss_kb() {
  std::ifstream status("/proc/self/status");
  std::string line;
  while (std::getline(status, line)) {
    if (line.rfind("VmHWM:", 0) == 0) {
      std::istringstream fields(line.substr(6));
      std::int64_t kb = 0;
      fields >> kb;
      return kb;
    }
  }
  rusage usage{};
  getrusage(RUSAGE_SELF, &usage);
  return static_cast<std::int64_t>(usage.ru_maxrss);  // fallback (kB on Linux)
}

double elapsed_ms(std::chrono::steady_clock::time_point since) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - since).count();
}

// ALGORITHM   (--verify)
//   skip with a reason if n > kMaxBruteForceComponents
//   brute-force counting vectors must equal the sweep's, on every state
//   Eq. (6) solved directly must equal Eq. (7) within tolerance on every state
//   numerical sanity: every MTTF finite and >= 0; every signature >= 0 summing to 1; phi_calls == 2^n
//   minimal path sets from the sweep must equal the brute-force definition
json verify_result(const SystemModel& model, const FailureModel& fm, const std::vector<CountingVector>& vectors,
                   const std::vector<double>& mttf, const SweepResult& sweep_result,
                   const std::vector<StateMask>& path_sets) {
  json report;
  if (model.n() > kMaxBruteForceComponents) {
    report["performed"] = false;
    report["skipped_reason"] = "the brute-force reference is limited to " +
                               std::to_string(kMaxBruteForceComponents) + " components";
    report["passed"] = nullptr;
    return report;
  }
  report["performed"] = true;

  const std::vector<CountingVector> reference = brute_force_counting_vectors(model);
  bool vectors_match = reference.size() == vectors.size();
  json first_mismatch = nullptr;
  for (std::size_t x = 0; vectors_match && x < reference.size(); ++x) {
    if (reference[x] != vectors[x]) {
      vectors_match = false;
      first_mismatch = {{"state", x}, {"sweep", vectors[x]}, {"reference", reference[x]}};
    }
  }
  report["counting_vectors_match_reference"] = vectors_match;
  report["first_mismatch"] = first_mismatch;

  const std::vector<double> direct = solve_mttf_direct(model, fm);
  double worst = 0.0;
  for (std::size_t x = 0; x < direct.size(); ++x) {
    worst = std::max(worst, std::fabs(mttf[x] - direct[x]) / std::max(1.0, std::fabs(direct[x])));
  }
  report["max_relative_mttf_difference_vs_eq6"] = worst;
  report["mttf_matches_eq6"] = worst <= kMttfTolerance;

  bool finite = true;
  for (double m : mttf) {
    finite = finite && std::isfinite(m) && m >= 0.0;
  }
  bool signatures_ok = true;
  double worst_sum_error = 0.0;
  for (const CountingVector& w : vectors) {
    if (w.empty() || w[0] == 0U) {
      continue;
    }
    std::vector<double> s;
    try {
      s = signature(w);
    } catch (const std::invalid_argument&) {
      signatures_ok = false;  // an impossible vector (e.g. W[k] > C(w,k)) is a failed verification, not a crash
      continue;
    }
    double sum = 0.0;
    for (double entry : s) {
      sum += entry;
      signatures_ok = signatures_ok && entry >= -kSignatureTolerance;
    }
    worst_sum_error = std::max(worst_sum_error, std::fabs(sum - 1.0));
  }
  signatures_ok = signatures_ok && worst_sum_error <= kSignatureTolerance;
  const bool phi_calls_ok = sweep_result.phi_calls == (std::uint64_t{1} << model.n());

  std::vector<StateMask> sorted_paths = path_sets;
  std::sort(sorted_paths.begin(), sorted_paths.end());
  const bool paths_match = sorted_paths == brute_force_minimal_path_sets(model);

  report["numerical_checks"] = {{"mttf_finite_and_nonnegative", finite},
                                {"signatures_nonnegative_and_sum_to_one", signatures_ok},
                                {"max_signature_sum_error", worst_sum_error},
                                {"phi_called_exactly_2_pow_n_times", phi_calls_ok},
                                {"minimal_path_sets_match_reference", paths_match}};
  report["passed"] = vectors_match && worst <= kMttfTolerance && finite && signatures_ok && phi_calls_ok && paths_match;
  return report;
}

int run(const Options& options) {
  const auto started = std::chrono::steady_clock::now();
  const json config_doc = read_json_file(options.config);
  const SystemModel model = SystemModel::from_json_file(options.config);
  const ChosenModel chosen = choose_failure_model(options, config_failure_model(config_doc), model.n());
  const FailureModel& fm = *chosen.model;

  // one sweep: collect the MTTF of every state, the vectors only if somebody needs them
  const int n = model.n();
  const StateMask all = all_working(n);
  std::vector<double> mttf(std::size_t{1} << n, 0.0);
  CountingVector full_vector;
  std::uint64_t operational = 0;
  const bool keep_vectors = options.per_state || options.verify;
  SweepOptions sweep_options;
  sweep_options.keep_all_vectors = keep_vectors;

  const auto analysis_start = std::chrono::steady_clock::now();
  SweepResult sweep_result = sweep(
      model,
      [&](StateMask x, const CountingVector& w) {
        mttf[x] = mttf_from_counting_vector(w, fm);
        operational += w[0];  // W0 = Phi(x)
        if (x == all) {
          full_vector = w;
        }
      },
      sweep_options);
  const double analysis_ms = elapsed_ms(analysis_start);

  json components = json::array();
  for (const Component& c : model.components()) {
    components.push_back({{"id", c.id}, {"name", c.name}, {"role", c.role}, {"bit", c.bit}});
  }
  json paths = json::array();
  for (StateMask x : sweep_result.minimal_path_sets) {
    paths.push_back({{"mask", x}, {"components", component_ids(model, x)}});
  }

  json results = {{"mttf_all_working", mttf[all]},
                  {"counting_vector_all_working", full_vector},
                  {"tail_signature_all_working", tail_signature(full_vector)},
                  {"system_signature", signature(full_vector)},
                  {"operational_states", operational},
                  {"minimal_path_sets", paths},
                  {"phi_calls", sweep_result.phi_calls},
                  {"peak_vectors_held", sweep_result.peak_vectors_held}};
  if (options.per_state) {
    json states = json::array();
    for (std::uint64_t xi = 0; xi < mttf.size(); ++xi) {
      const StateMask x = static_cast<StateMask>(xi);
      const CountingVector& w = sweep_result.vectors[x];
      states.push_back({{"mask", x},
                        {"working_count", working_count(x)},
                        {"failed", failed_ids(model, x)},
                        {"operational", w[0] == 1U},
                        {"counting_vector", w},
                        {"signature", signature(w)},
                        {"mttf", mttf[x]}});
    }
    results["states"] = std::move(states);
  }

  // FAULT INJECTION, for testing the verification itself. A healthy engine can never make --verify fail,
  // so without this the "verification failed" path (exit code 3) could not be tested. It only acts together
  // with --verify, and only when the environment variable below is set to one of the values listed.
  if (options.verify) {
    if (const char* fault = std::getenv("ANALYZE_FAULT_INJECTION")) {
      const std::string kind = fault;
      if (kind == "counting_vector" && !sweep_result.vectors.empty()) {
        // pretend the sweep miscounted: change one entry of the all-working vector to a plausible wrong value
        CountingVector& v = sweep_result.vectors[all];
        const int size = static_cast<int>(v.size()) - 1;
        for (int k = 1; k <= size; ++k) {
          if (v[static_cast<std::size_t>(k)] < binomial(size, k)) {
            ++v[static_cast<std::size_t>(k)];
            break;
          }
        }
      } else if (kind == "mttf") {
        mttf[all] *= 1.001;  // pretend the MTTF formula drifted
      } else if (!kind.empty()) {
        throw ModelError("unknown ANALYZE_FAULT_INJECTION value '" + kind + "' (expected counting_vector or mttf)");
      }
    }
  }

  json verification = {{"performed", false}};
  double verification_ms = 0.0;
  bool verification_ok = true;
  if (options.verify) {
    std::vector<StateMask> path_sets = sweep_result.minimal_path_sets;
    const auto verification_start = std::chrono::steady_clock::now();
    verification = verify_result(model, fm, sweep_result.vectors, mttf, sweep_result, path_sets);
    verification_ms = elapsed_ms(verification_start);
    verification_ok = !verification["performed"].get<bool>() || verification["passed"].get<bool>();
  }

  json system_info = {{"name", config_doc.value("name", std::string())},
                      {"n", n},
                      {"total_states", std::uint64_t{1} << n},
                      {"components", components}};
  json output = {{"tool", {{"name", "analyze"}, {"version", RELIABILITY_VERSION}}},
                 {"config_file", options.config},
                 {"system", system_info},
                 {"failure_model", chosen.description},
                 {"results", results},
                 {"verification", verification},
                 {"timing", {{"analysis_ms", analysis_ms},
                             {"verification_ms", verification_ms},
                             {"total_ms", elapsed_ms(started)},
                             {"peak_rss_kb", peak_rss_kb()}}}};

  const std::string text = output.dump(2) + "\n";
  if (options.out) {
    std::ofstream file(*options.out);
    if (!file) {
      throw ModelError("cannot write to '" + *options.out + "'");
    }
    file << text;
  } else {
    std::cout << text;
  }
  if (!verification_ok) {
    std::cerr << "analyze: VERIFICATION FAILED (see the \"verification\" section of the output)\n";
    return kExitVerificationFailed;
  }
  return kExitOk;
}

}  // namespace

int main(int argc, char** argv) {
  try {
    const Options options = parse_args(argc, argv);
    if (options.help) {
      std::cout << kUsage;
      return kExitOk;
    }
    return run(options);
  } catch (const UsageError& e) {
    std::cerr << "analyze: " << e.what() << "\n\n" << kUsage;
    return kExitUsage;
  } catch (const reliability::ModelError& e) {
    std::cerr << "analyze: " << e.what() << "\n";
    return kExitInvalid;
  } catch (const std::exception& e) {
    std::cerr << "analyze: internal error: " << e.what() << "\n";
    return kExitInvalid;
  }
}
