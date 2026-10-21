// main.cpp - the command-line tool "analyze".                (Plan: Days 6-8; INT-001, INT-003, INT-004)
//
// USAGE (planned)
//   analyze --config configs/satellite_support.json [--model exp|lsh] [--lambda L] [--beta B]
//           [--verify] [--out results/run1.json]
//
// PSEUDO-CODE
//   args = parse the command line               (unknown flag -> usage + exit 2)
//   try:
//       model = SystemModel::from_json_file(args.config)
//       fm    = (args.model == "lsh") ? make_lethal_shock(args.beta, model.n())
//                                     : make_exponential(args.lambda)
//       vectors = []; paths = []
//       result  = sweep(model, observer = collect W and m(x) for every state)
//       mttf    = compute_all_mttf(model, *fm)
//       if args.verify:
//           ref = brute_force_counting_vectors(model)
//           compare vectors with ref for every state     -> on mismatch: report state, k, both values; exit 1
//           compare mttf with solve_mttf_direct(model, *fm)
//           numerical checks: no NaN/negative, signatures sum to 1, phi_calls == 2^n
//       write JSON: {config, model parameters, system signature, mttf[], paths[],
//                    phi_calls, tool version}        to args.out or standard output
//       exit 0
//   catch ModelError e:
//       print e.what() to standard error; exit 1        (non-zero status, message names the field)
#include <iostream>

int main(int /*argc*/, char** /*argv*/) {
  std::cerr << "analyze: not implemented yet (project skeleton). See docs/project-plan.md.\n";
  return 2;
}
