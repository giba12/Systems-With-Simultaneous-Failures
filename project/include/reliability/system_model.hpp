// system_model.hpp - the system: its components and the structure function Phi.
//                                         (Plan: Day 2; SYS-001, SYS-003, SYS-004, SYS-005)
//
// IDEA
//   The structure function Phi answers one question: "given which components work,
//   does the whole system work?"  The reference satellite is three redundant pairs:
//       Phi(x) = (P1 or P2) and (C1 or C2) and (K1 or K2)
//   The class also counts how many times Phi is asked (the paper promises exactly 2^n
//   calls for the whole sweep - Theorem 2 - and we test that).
//
// ALGORITHM of the factory functions
//   from_json_file(path):
//       read the JSON text; parse it           (library choice pending: nlohmann/json)
//       components := list of {id, name, role, bit}
//       reject: duplicate ids, duplicate bits, bits not 0..n-1, unknown ids in "structure"
//       switch structure.type:
//           "and_of_or_groups" -> and_of_or_groups(components, groups)
//           "series"           -> series(components)
//           "parallel"         -> parallel(components)
//           otherwise          -> ModelError("unknown structure type ...")
//       model.validate();  return model
//   series(n)              : Phi(x) = (x == all_working(n))        (every component needed)
//   parallel(n)            : Phi(x) = (x != 0)                     (any one component is enough)
//   and_of_or_groups(g)    : Phi(x) = for every group, at least one member bit is 1
//   or_of_and_groups(p)    : Phi(x) = for some path set, every member bit is 1   (JSON type "path_sets")
//   from_function(n, fn)   : wrap any user function (used by tests, e.g. the paper's Fig. 1
//                            system  max{x3, min{x1, x2}}  and random monotone systems)
//
// ALGORITHM of validate()   (the paper's preconditions, Section I-A)
//   n must satisfy 2 <= n <= kMaxComponents               (the paper needs n > 1)
//   Phi(0) must be 0 and Phi(all working) must be 1       (non-trivial system)
//   monotone: for every state x and every working bit i, Phi(without(x,i)) <= Phi(x)
//             (checking single steps is enough; costs n * 2^n calls)
//   do NOT reject irrelevant components (the paper allows semi-coherent systems)
//   reset the call counter to 0 afterwards, so validation never pollutes the sweep's count
//
// ALGORITHM of phi(x)
//   if x >= 2^n: throw ModelError("mask out of range")
//   phi_calls_ += 1
//   return structure_(x)
#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "reliability/state.hpp"

namespace reliability {

struct Component {
  std::string id;    // "P1"
  std::string name;  // "Power unit A"
  std::string role;  // "Primary power source"
  int bit = 0;       // position in the state mask
};

class SystemModel {
 public:
  using StructureFn = std::function<bool(StateMask)>;

  // Factories. Group members are bit positions (0 .. n-1). The factories refuse n > 31 (the
  // mask is 32 bits wide) but do NOT check the paper's preconditions: call validate() for that.
  static SystemModel from_json_file(const std::string& path);
  static SystemModel series(int n);
  static SystemModel parallel(int n);
  static SystemModel and_of_or_groups(std::vector<Component> components,
                                      const std::vector<std::vector<int>>& groups);
  // Phi(x) = 1 if ALL components of SOME path set work (an OR of ANDs): the natural form for minimal path sets.
  // Every path set must be non-empty and name valid bit positions.
  static SystemModel or_of_and_groups(std::vector<Component> components,
                                      const std::vector<std::vector<int>>& path_sets);
  static SystemModel from_function(int n, StructureFn fn);

  int n() const { return static_cast<int>(components_.size()); }
  const std::vector<Component>& components() const { return components_; }

  // Phi(x). Counts the call.
  bool phi(StateMask x) const;

  std::uint64_t phi_calls() const { return phi_calls_; }
  void reset_phi_calls() const { phi_calls_ = 0; }

  // Throws ModelError if the system breaks the paper's assumptions.
  void validate() const;

 private:
  std::vector<Component> components_;
  StructureFn structure_;
  mutable std::uint64_t phi_calls_ = 0;
};

}  // namespace reliability
