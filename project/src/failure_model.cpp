// failure_model.cpp - [Exp] and the Levy-frailty (lethal-shock) failure models.
//                                                    (Plan: Day 6 for [Exp], Day 7 for the rest)
#include "reliability/failure_model.hpp"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>

#include "reliability/errors.hpp"

namespace reliability {

// ---------------- IndependentExponential ----------------

// ALGORITHM
//   if lambda is NaN, infinite or <= 0: throw ModelError("lambda must be a positive finite number")
//   lambda_ = lambda
IndependentExponential::IndependentExponential(double lambda) : lambda_(lambda) {
  if (!std::isfinite(lambda) || lambda <= 0.0) {
    std::ostringstream message;
    message << "lambda must be a positive finite number, got " << lambda;
    throw ModelError(message.str());
  }
}

// ALGORITHM   return "independent_exponential(lambda=...)"
std::string IndependentExponential::name() const {
  std::ostringstream out;
  out << std::setprecision(17) << "independent_exponential(lambda=" << lambda_ << ")";
  return out.str();
}

// ALGORITHM   w components fail independently, each at rate lambda: the next failure comes at rate w*lambda
//   (paper Section VII-A.1: nu_w = w * lambda)
double IndependentExponential::exit_rate(int w) const {
  if (w < 0) {
    throw ModelError("exit_rate: the number of working components must be >= 0, got " +
                     std::to_string(w));
  }
  return static_cast<double>(w) * lambda_;
}

// ALGORITHM   components fail one at a time: from w1 working the next state has w1 - 1 working
double IndependentExponential::transition_prob(int w1, int w2) const {
  if (w1 < 0 || w2 < 0) {
    throw ModelError("transition_prob: component counts must be >= 0");
  }
  return (w1 >= 1 && w2 == w1 - 1) ? 1.0 : 0.0;
}

// ALGORITHM
//   require 0 <= k < w  (else ModelError)
//   return 1.0 / (lambda_ * (w - k))      // paper: E T_{k+1:w} - E T_{k:w} = 1 / nu_{w-k}
double IndependentExponential::increment(int w, int k) const {
  if (k < 0 || k >= w) {
    throw ModelError("increment: need 0 <= k < w, got w = " + std::to_string(w) + ", k = " +
                     std::to_string(k));
  }
  return 1.0 / (lambda_ * static_cast<double>(w - k));
}

// ---------------- LevyFrailty ----------------

// PSEUDO-CODE
//   require psi(1) ~ 1 within 1e-12 (calibration: unit mean component lifetime)
//   require psi(0) == 0, psi non-decreasing for 1..max_n
//   precompute psi(1..max_n) and the table P[w1][w2] for all w1 <= max_n  (extended precision)
//   precompute increment[w][k] by the non-negative recursion (route 1)
LevyFrailty::LevyFrailty(std::string label, std::function<double(int)> psi, int max_n)
    : label_(std::move(label)), psi_(std::move(psi)), max_n_(max_n) {
  not_implemented("LevyFrailty::LevyFrailty");
}

// PSEUDO-CODE   return label_
std::string LevyFrailty::name() const { not_implemented("LevyFrailty::name"); }

// PSEUDO-CODE   require 1 <= w <= max_n;  return psi(w)                         Eq. (19)
double LevyFrailty::exit_rate(int /*w*/) const { not_implemented("LevyFrailty::exit_rate"); }

// PSEUDO-CODE   look up the precomputed table P[w1][w2]                          Eq. (20)
//   check rows: sum over w2 < w1 of P[w1][w2] == 1 within tolerance (test TC-D-07)
double LevyFrailty::transition_prob(int /*w1*/, int /*w2*/) const {
  not_implemented("LevyFrailty::transition_prob");
}

// PSEUDO-CODE   look up the precomputed increment table (route 1)
double LevyFrailty::increment(int /*w*/, int /*k*/) const {
  not_implemented("LevyFrailty::increment");
}

// PSEUDO-CODE   evaluate Eq. (21) directly (route 2); used ONLY to cross-check route 1
double LevyFrailty::increment_eq21(int /*w*/, int /*k*/) const {
  not_implemented("LevyFrailty::increment_eq21");
}

// ---------------- factories ----------------

// ALGORITHM   return a new IndependentExponential(lambda)
std::unique_ptr<FailureModel> make_exponential(double lambda) {
  return std::make_unique<IndependentExponential>(lambda);
}

// PSEUDO-CODE
//   if beta < 0 or beta > 1: throw ModelError("beta must lie in [0, 1]")
//   psi = x -> (x == 0) ? 0 : (1 - beta) * x + beta
//   return a new LevyFrailty("lethal_shock(beta=...)", psi, max_n)
std::unique_ptr<FailureModel> make_lethal_shock(double /*beta*/, int /*max_n*/) {
  not_implemented("make_lethal_shock");
}

}  // namespace reliability
