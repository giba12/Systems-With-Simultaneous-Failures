// errors.hpp - the two kinds of error the library can raise.
//
// PSEUDO-CODE
//   ModelError          : the user gave us something invalid (bad config, bad mask,
//                         bad parameter). The message must name the field/value and
//                         the rule that was broken (requirement INT-004).
//   NotImplementedError : this function is still a skeleton stub. Every stub calls
//                         not_implemented("name") so nothing can silently return a
//                         wrong number while the project is being built.
//
// Delete NotImplementedError once every stub has been replaced (Day 9 cleanup).
#pragma once

#include <stdexcept>
#include <string>

namespace reliability {

class ModelError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

class NotImplementedError : public std::logic_error {
 public:
  using std::logic_error::logic_error;
};

[[noreturn]] inline void not_implemented(const std::string& what) {
  throw NotImplementedError("not implemented yet: " + what);
}

}  // namespace reliability
