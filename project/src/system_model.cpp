// system_model.cpp - building, checking and querying a system.    (Plan: Day 2)
#include "reliability/system_model.hpp"

#include <algorithm>
#include <fstream>
#include <string>
#include <string_view>
#include <unordered_map>

#include <nlohmann/json.hpp>

#include "reliability/errors.hpp"

namespace reliability {

namespace {

using json = nlohmann::json;

// A state mask is 32 bits wide, so shifts such as (1 << n) are only safe up to n = 31.
constexpr int kMaskBits = 31;

std::uint64_t state_count(int n) { return std::uint64_t{1} << n; }

// ALGORITHM
//   n components named "C1".."Cn", component i uses bit i
std::vector<Component> generic_components(int n) {
  if (n < 0 || n > kMaskBits) {
    throw ModelError("the number of components must be between 0 and " +
                     std::to_string(kMaskBits) + ", got " + std::to_string(n));
  }
  std::vector<Component> components;
  components.reserve(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    components.push_back({"C" + std::to_string(i + 1), "Component " + std::to_string(i + 1), "", i});
  }
  return components;
}

// ALGORITHM
//   n must not exceed 31
//   every id must be unique                       -> ModelError naming the duplicate id
//   the bits must be exactly 0 .. n-1, each once  -> ModelError naming the component(s)
//   sort the components by bit, so components[i].bit == i
void normalise_components(std::vector<Component>& components) {
  const int n = static_cast<int>(components.size());
  if (n > kMaskBits) {
    throw ModelError("too many components: " + std::to_string(n) + " (a state mask holds at most " +
                     std::to_string(kMaskBits) + ")");
  }
  std::unordered_map<std::string, int> seen_ids;
  for (const Component& c : components) {
    if (!seen_ids.emplace(c.id, c.bit).second) {
      throw ModelError("duplicate component id '" + c.id + "'");
    }
  }
  std::vector<const Component*> owner(static_cast<std::size_t>(n), nullptr);
  for (const Component& c : components) {
    if (c.bit < 0 || c.bit >= n) {
      throw ModelError("component '" + c.id + "' uses bit " + std::to_string(c.bit) +
                       " but the bits must be exactly 0.." + std::to_string(n - 1) +
                       " (one per component)");
    }
    if (owner[static_cast<std::size_t>(c.bit)] != nullptr) {
      throw ModelError("components '" + owner[static_cast<std::size_t>(c.bit)]->id + "' and '" + c.id +
                       "' both use bit " + std::to_string(c.bit));
    }
    owner[static_cast<std::size_t>(c.bit)] = &c;
  }
  std::sort(components.begin(), components.end(),
            [](const Component& a, const Component& b) { return a.bit < b.bit; });
}

// ---- JSON helpers: every problem becomes a ModelError that names the field -----------------

const json& require_field(const json& object, const char* key, std::string_view where) {
  if (!object.is_object() || !object.contains(key)) {
    throw ModelError(std::string("missing required field '") + key + "' in " + std::string(where));
  }
  return object.at(key);
}

std::string string_field(const json& object, const char* key, std::string_view where,
                         bool required) {
  if (!object.contains(key)) {
    if (required) {
      throw ModelError(std::string("missing required field '") + key + "' in " + std::string(where));
    }
    return "";
  }
  if (!object.at(key).is_string()) {
    throw ModelError(std::string("field '") + key + "' in " + std::string(where) +
                     " must be a string");
  }
  return object.at(key).get<std::string>();
}

// ALGORITHM   (turns a parsed JSON document into a validated model)
//   components = require "components": a non-empty array of {id, bit, [name], [role]}
//   structure  = require "structure" with a "type":
//       "and_of_or_groups": groups = array of arrays of component ids -> convert ids to bits
//       "path_sets"       : paths = array of arrays of component ids (Phi = some path set fully works)
//       "series"          : every component is its own group
//       "parallel"        : one group holding every component
//       anything else     : ModelError("unknown structure type ...")
//   model = SystemModel::and_of_or_groups(components, groups); model.validate(); return model
SystemModel build_from_json(const json& doc) {
  if (!doc.is_object()) {
    throw ModelError("the configuration must be a JSON object");
  }
  const json& component_list = require_field(doc, "components", "the configuration");
  if (!component_list.is_array()) {
    throw ModelError("field 'components' must be an array");
  }

  std::vector<Component> components;
  for (std::size_t i = 0; i < component_list.size(); ++i) {
    const json& entry = component_list[i];
    const std::string where = "components[" + std::to_string(i) + "]";
    if (!entry.is_object()) {
      throw ModelError(where + " must be an object");
    }
    Component c;
    c.id = string_field(entry, "id", where, true);
    c.name = string_field(entry, "name", where, false);
    c.role = string_field(entry, "role", where, false);
    const json& bit = require_field(entry, "bit", where);
    if (!bit.is_number_integer()) {
      throw ModelError("field 'bit' in " + where + " must be an integer");
    }
    c.bit = bit.get<int>();
    components.push_back(std::move(c));
  }
  normalise_components(components);  // duplicate ids/bits, bit range, sorted by bit

  const json& structure = require_field(doc, "structure", "the configuration");
  const std::string type = string_field(structure, "type", "'structure'", true);
  const int n = static_cast<int>(components.size());

  std::vector<std::vector<int>> groups;
  if (type == "and_of_or_groups") {
    std::unordered_map<std::string, int> bit_of;
    for (const Component& c : components) {
      bit_of.emplace(c.id, c.bit);
    }
    const json& group_list = require_field(structure, "groups", "'structure'");
    if (!group_list.is_array()) {
      throw ModelError("field 'groups' in 'structure' must be an array of arrays of component ids");
    }
    for (const json& group : group_list) {
      if (!group.is_array()) {
        throw ModelError("every entry of 'groups' must be an array of component ids");
      }
      std::vector<int> bits;
      for (const json& id : group) {
        if (!id.is_string()) {
          throw ModelError("component ids in 'groups' must be strings");
        }
        const auto it = bit_of.find(id.get<std::string>());
        if (it == bit_of.end()) {
          throw ModelError("structure refers to unknown component id '" + id.get<std::string>() + "'");
        }
        bits.push_back(it->second);
      }
      groups.push_back(std::move(bits));
    }
  } else if (type == "path_sets") {
    std::unordered_map<std::string, int> bit_of;
    for (const Component& c : components) {
      bit_of.emplace(c.id, c.bit);
    }
    const json& path_list = require_field(structure, "paths", "'structure'");
    if (!path_list.is_array()) {
      throw ModelError("field 'paths' in 'structure' must be an array of arrays of component ids");
    }
    std::vector<std::vector<int>> paths;
    for (const json& path : path_list) {
      if (!path.is_array()) {
        throw ModelError("every entry of 'paths' must be an array of component ids");
      }
      std::vector<int> bits;
      for (const json& id : path) {
        if (!id.is_string()) {
          throw ModelError("component ids in 'paths' must be strings");
        }
        const auto it = bit_of.find(id.get<std::string>());
        if (it == bit_of.end()) {
          throw ModelError("structure refers to unknown component id '" + id.get<std::string>() + "'");
        }
        bits.push_back(it->second);
      }
      paths.push_back(std::move(bits));
    }
    SystemModel model = SystemModel::or_of_and_groups(std::move(components), paths);
    model.validate();
    return model;
  } else if (type == "series") {
    for (int i = 0; i < n; ++i) {
      groups.push_back({i});
    }
  } else if (type == "parallel") {
    std::vector<int> all;
    for (int i = 0; i < n; ++i) {
      all.push_back(i);
    }
    groups.push_back(std::move(all));
  } else {
    throw ModelError("unknown structure type '" + type +
                     "' (expected and_of_or_groups, path_sets, series or parallel)");
  }

  SystemModel model = SystemModel::and_of_or_groups(std::move(components), groups);
  model.validate();
  return model;
}

}  // namespace

// ALGORITHM
//   open the file                    (error: "cannot open configuration file <path>")
//   parse it as JSON                 (error: "malformed JSON in <path>: <where and why>")
//   build_from_json(...)             (any wrongly typed field is reported as ModelError too)
SystemModel SystemModel::from_json_file(const std::string& path) {
  std::ifstream in(path);
  if (!in) {
    throw ModelError("cannot open configuration file '" + path + "'");
  }
  json doc;
  try {
    doc = json::parse(in);
  } catch (const json::parse_error& e) {
    throw ModelError("malformed JSON in '" + path + "': " + e.what());
  }
  try {
    return build_from_json(doc);
  } catch (const json::exception& e) {
    throw ModelError("invalid configuration in '" + path + "': " + e.what());
  }
}

// ALGORITHM
//   components = n generic components; structure = x -> (x == all_working(n))
SystemModel SystemModel::series(int n) {
  SystemModel model;
  model.components_ = generic_components(n);
  model.structure_ = [n](StateMask x) { return x == all_working(n); };
  return model;
}

// ALGORITHM
//   components = n generic components; structure = x -> (x != 0)
SystemModel SystemModel::parallel(int n) {
  SystemModel model;
  model.components_ = generic_components(n);
  model.structure_ = [](StateMask x) { return x != 0U; };
  return model;
}

// ALGORITHM
//   check and sort the components (unique ids, bits exactly 0..n-1)
//   groups are lists of bit positions; every position must be 0..n-1 and no group may be empty
//   group_mask[g] = OR of (1 << bit) over the members of group g
//   structure = x -> for every group g: (x & group_mask[g]) != 0
//   (a component in no group is irrelevant: allowed)
SystemModel SystemModel::and_of_or_groups(std::vector<Component> components,
                                          const std::vector<std::vector<int>>& groups) {
  normalise_components(components);
  const int n = static_cast<int>(components.size());

  std::vector<StateMask> masks;
  masks.reserve(groups.size());
  for (std::size_t g = 0; g < groups.size(); ++g) {
    if (groups[g].empty()) {
      throw ModelError("group " + std::to_string(g) + " is empty (it could never work)");
    }
    StateMask mask = 0;
    for (int bit : groups[g]) {
      if (bit < 0 || bit >= n) {
        throw ModelError("group " + std::to_string(g) + " refers to bit " + std::to_string(bit) +
                         " but the components use bits 0.." + std::to_string(n - 1));
      }
      mask |= StateMask{1} << bit;
    }
    masks.push_back(mask);
  }

  SystemModel model;
  model.components_ = std::move(components);
  model.structure_ = [masks = std::move(masks)](StateMask x) {
    return std::all_of(masks.begin(), masks.end(), [x](StateMask m) { return (x & m) != 0U; });
  };
  return model;
}

// ALGORITHM
//   check and sort the components; every path set is a non-empty list of valid bit positions
//   path_mask[p] = OR of (1 << bit) over the members of path set p
//   structure = x -> some p has (x & path_mask[p]) == path_mask[p]
SystemModel SystemModel::or_of_and_groups(std::vector<Component> components,
                                          const std::vector<std::vector<int>>& path_sets) {
  normalise_components(components);
  const int n = static_cast<int>(components.size());
  std::vector<StateMask> masks;
  masks.reserve(path_sets.size());
  for (std::size_t p = 0; p < path_sets.size(); ++p) {
    if (path_sets[p].empty()) {
      throw ModelError("path set " + std::to_string(p) + " is empty (it would make the system always work)");
    }
    StateMask mask = 0;
    for (int bit : path_sets[p]) {
      if (bit < 0 || bit >= n) {
        throw ModelError("path set " + std::to_string(p) + " refers to bit " + std::to_string(bit) +
                         " but the components use bits 0.." + std::to_string(n - 1));
      }
      mask |= StateMask{1} << bit;
    }
    masks.push_back(mask);
  }
  SystemModel model;
  model.components_ = std::move(components);
  model.structure_ = [masks = std::move(masks)](StateMask x) {
    return std::any_of(masks.begin(), masks.end(), [x](StateMask m) { return (x & m) == m; });
  };
  return model;
}

// ALGORITHM
//   components = n generic components; structure = fn
//   (no validation here: tests call validate() themselves, including on bad functions)
SystemModel SystemModel::from_function(int n, StructureFn fn) {
  if (!fn) {
    throw ModelError("the structure function must not be empty");
  }
  SystemModel model;
  model.components_ = generic_components(n);
  model.structure_ = std::move(fn);
  return model;
}

// ALGORITHM
//   if x >= 2^n: throw ModelError("state mask <x> out of range for n = <n>")
//   phi_calls_ += 1
//   return structure_(x)
bool SystemModel::phi(StateMask x) const {
  if (static_cast<std::uint64_t>(x) >= state_count(n())) {
    throw ModelError("state mask " + std::to_string(x) + " is out of range for n = " +
                     std::to_string(n()) + " (valid masks are 0.." +
                     std::to_string(state_count(n()) - 1) + ")");
  }
  ++phi_calls_;
  return structure_(x);
}

// ALGORITHM
//   if n < 2 or n > kMaxComponents:          ModelError (the paper needs n > 1)
//   works[x] = phi(x) for every state x      // 2^n calls, done once and reused below
//   if works[0]:                             ModelError("system works with every component failed")
//   if not works[all working]:               ModelError("system fails with every component working")
//   for every state x that does NOT work, for every working bit i:
//       if the son without(x, i) DOES work:
//           ModelError("not monotone: ...")  // failing one more component cannot repair the system
//   reset_phi_calls()          // validation must not count towards the sweep's 2^n
// Checking single steps (sons) is enough: any chain y <= x is a sequence of single steps.
void SystemModel::validate() const {
  struct ResetOnExit {
    const SystemModel& model;
    ~ResetOnExit() { model.reset_phi_calls(); }
  } guard{*this};

  const int count = n();
  if (count < 2 || count > kMaxComponents) {
    throw ModelError("the system must have between 2 and " + std::to_string(kMaxComponents) +
                     " components (the paper needs n > 1), got " + std::to_string(count));
  }
  const std::uint64_t states = state_count(count);
  std::vector<std::uint8_t> works(static_cast<std::size_t>(states));
  for (std::uint64_t x = 0; x < states; ++x) {
    works[static_cast<std::size_t>(x)] = phi(static_cast<StateMask>(x)) ? 1 : 0;
  }

  if (works[0] != 0) {
    throw ModelError("the system works with every component failed (Phi(0) must be 0)");
  }
  if (works[static_cast<std::size_t>(states - 1)] == 0) {
    throw ModelError("the system fails with every component working (Phi(1) must be 1)");
  }
  for (std::uint64_t x = 1; x < states; ++x) {
    if (works[static_cast<std::size_t>(x)] != 0) {
      continue;
    }
    const StateMask state = static_cast<StateMask>(x);
    for (int i = 0; i < count; ++i) {
      if (is_working(state, i) && works[without(state, i)] != 0) {
        throw ModelError("not monotone: state " + std::to_string(without(state, i)) +
                         " works, but state " + std::to_string(state) + ", which also has component '" +
                         components_[static_cast<std::size_t>(i)].id + "' working, does not");
      }
    }
  }
}

}  // namespace reliability
