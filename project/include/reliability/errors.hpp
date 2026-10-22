// errors.hpp - the error type the library raises.
//
// ModelError : the caller gave us something invalid (bad configuration, bad mask, bad parameter), or a
//              model that cannot be trusted (for example a failure model whose numbers lose too much
//              precision). The message names the field or value and the rule that was broken
//              (requirement INT-004); the command line tool turns it into exit status 1.
#pragma once

#include <stdexcept>
#include <string>

namespace reliability {

class ModelError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

}  // namespace reliability
