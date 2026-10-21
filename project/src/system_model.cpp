// system_model.cpp - building, checking and querying a system.    (Plan: Day 2)
#include "reliability/system_model.hpp"

#include "reliability/errors.hpp"

namespace reliability {

// PSEUDO-CODE
//   text   = read whole file (error: "cannot open <path>")
//   json   = parse(text)     (error: "malformed JSON at line L: ...", REQ INT-004)
//   require fields: components[], structure{type,...}
//   components = [Component{id,name,role,bit} for each entry]
//   check: ids unique, bits unique, bits cover exactly 0..n-1
//   build the structure from json.structure (see system_model.hpp)
//   model.validate()
//   return model
SystemModel SystemModel::from_json_file(const std::string& /*path*/) {
  not_implemented("SystemModel::from_json_file");
}

// PSEUDO-CODE
//   components = n components named "C1".."Cn", bit i = i
//   structure  = x -> (x == all_working(n))
SystemModel SystemModel::series(int /*n*/) { not_implemented("SystemModel::series"); }

// PSEUDO-CODE
//   components = as above
//   structure  = x -> (x != 0)
SystemModel SystemModel::parallel(int /*n*/) { not_implemented("SystemModel::parallel"); }

// PSEUDO-CODE
//   groups are lists of component INDICES; every index must be 0..n-1 (else ModelError)
//   group_mask[g] = OR of (1 << index) over the members of group g
//   structure = x -> for every group g: (x & group_mask[g]) != 0
//   (a component in no group is irrelevant: allowed)
SystemModel SystemModel::and_of_or_groups(std::vector<Component> /*components*/,
                                          const std::vector<std::vector<int>>& /*groups*/) {
  not_implemented("SystemModel::and_of_or_groups");
}

// PSEUDO-CODE
//   components = n generic components; structure = fn
//   (no validation here: tests call validate() themselves, including on bad functions)
SystemModel SystemModel::from_function(int /*n*/, StructureFn /*fn*/) {
  not_implemented("SystemModel::from_function");
}

// PSEUDO-CODE
//   if x >= 2^n: throw ModelError("state mask <x> out of range for n = <n>")
//   phi_calls_ += 1
//   return structure_(x)
bool SystemModel::phi(StateMask /*x*/) const { not_implemented("SystemModel::phi"); }

// PSEUDO-CODE
//   if n < 2 or n > kMaxComponents:          ModelError (the paper needs n > 1)
//   if phi(0) != false:                      ModelError("system works with every component failed")
//   if phi(all_working(n)) != true:          ModelError("system fails with every component working")
//   for every state x, for every working bit i:
//       if phi(x) is false and phi(without(x, i)) is true:
//           ModelError("not monotone: failing component <i> in state <x> repairs the system")
//   reset_phi_calls()          // validation must not count towards the sweep's 2^n
void SystemModel::validate() const { not_implemented("SystemModel::validate"); }

}  // namespace reliability
