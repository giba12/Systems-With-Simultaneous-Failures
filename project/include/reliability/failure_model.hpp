// failure_model.hpp - how long components live.    (Plan: Day 6 [Exp], Day 7 [LSh]; REL-002..004)
//
// IDEA
//   The signature only describes the SHAPE of the system. To get a time (MTTF) we also need
//   to say how fast components fail. The paper allows any model where
//     (A) all components are interchangeable (exchangeable lifetimes), and
//     (B) the number of working components is a continuous-time Markov chain
//         (it only ever goes down).
//   Such a model is fully described by three things, for w working components:
//       nu(w)        = rate at which the next failure EVENT happens
//       P(w, w2)     = probability that the event leaves w2 components working
//                      (w2 < w; if w - w2 >= 2 it is a simultaneous failure)
//       increment(w,k) = E[T_{k+1:w}] - E[T_{k:w}]
//                      = expected extra waiting time between the k-th and (k+1)-th
//                        component failure when w components start out working.
//   The MTTF engine only needs increment(). The other two are used to cross-check it.
//
// PSEUDO-CODE
//   IndependentExponential(lambda)               paper model [Exp], Section VII-A.1
//       reject lambda <= 0 or NaN
//       nu(w)             = w * lambda
//       P(w, w2)          = 1 if w2 == w-1 else 0          (one failure at a time)
//       increment(w, k)   = 1 / (lambda * (w - k))
//
//   LevyFrailty(Psi)   Levy-frailty Marshall-Olkin family, Section VII-A.2
//       Psi(x) is the "Laplace exponent": a non-decreasing concave function, Psi(0) = 0.
//       nu(w) = Psi(w)                                                         Eq. (19)
//       P(w1, w2) = C(w1, w1-w2) * sum_{w=w2..w1} (-1)^(w-w2+1) * C(w1-w2, w1-w)
//                                         * Psi(w) / Psi(w1)                   Eq. (20)
//             (an ALTERNATING sum: huge terms cancel, so compute in extended precision;
//              see docs/paper-validation-report.md section 4)
//       increment(w, k): two routes, use both and compare in tests
//           route 1 (preferred, all terms >= 0, paper after Prop. 3):
//               f[w] = 1;  for v = w-1 down to 0:  f[v] = sum_{v' > v} f[v'] * P(v', v)
//               increment(w, k) = f[w-k] / nu(w-k)
//           route 2 (Eq. (21), alternating sum):
//               sum_{i=w-k..w} C(w,k) * C(k, w-i) * (-1)^(i-w+k) / Psi(i)
//               NOTE: the paper PRINTS the lower limit as w-k+1. That is a typo (it gives 0 for
//               k = 0, even for plain exponential lifetimes). The correct limit is w-k;
//               see docs/paper-validation-report.md section 6.
//
//   make_lethal_shock(beta)     the simplest simultaneous-failure model, [LSh]
//       require 0 <= beta <= 1
//       Psi(x) = (1 - beta) * x + beta    for x > 0     (components fail alone, plus a
//                                                        "lethal shock" that kills all)
//       beta = 0  ->  identical to IndependentExponential(1)
//       calibration: Psi(1) = 1, i.e. every component lives 1 time unit on average
//       beta is the paper's "pairwise beta factor": beta = 2 - Psi(2)    Eq. (22)
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace reliability {

class FailureModel {
 public:
  virtual ~FailureModel() = default;
  virtual std::string name() const = 0;
  virtual double exit_rate(int w) const = 0;                // nu_w
  virtual double transition_prob(int w1, int w2) const = 0;  // P_{w1,w2}
  virtual double increment(int w, int k) const = 0;          // E T_{k+1:w} - E T_{k:w}
};

class IndependentExponential final : public FailureModel {
 public:
  explicit IndependentExponential(double lambda);
  std::string name() const override;
  double exit_rate(int w) const override;
  double transition_prob(int w1, int w2) const override;
  double increment(int w, int k) const override;

 private:
  double lambda_;
};

// Largest number of components the Levy-frailty models accept. Eq. (20) is an alternating sum whose
// cancellation grows like 2^n; in long double the transition kernel is trustworthy up to n = 16
// (parallel-system MTTF accurate to about 2e-10 relative there, 3e-12 at n = 12), while for n = 17
// some parameter values are already refused by the numerical guard. Measured on 2026-10-21; see
// docs/paper-validation-report.md section 4. (The paper uses 512-bit arithmetic for n up to 30.)
inline constexpr int kMaxLevyFrailtyComponents = 16;

// A Levy-frailty Marshall-Olkin model given by its Laplace exponent Psi (paper Section VII-A.2).
// The constructor computes the transition kernel P (Eq. (20)) and the increments once, in long
// double, and REJECTS the model if the numbers cannot be trusted: Psi must be positive, non-decreasing
// and calibrated (Psi(1) = 1), every row of P must sum to 1 and have no negative entry beyond
// rounding noise. That guard is what limits max_n when the alternating sums lose too much precision.
class LevyFrailty final : public FailureModel {
 public:
  // max_n = the largest number of working components the model will be asked about (<= kMaxLevyFrailtyComponents)
  LevyFrailty(std::string label, std::function<double(int)> psi, int max_n);
  std::string name() const override;
  double exit_rate(int w) const override;
  double transition_prob(int w1, int w2) const override;
  double increment(int w, int k) const override;
  // Paper Eq. (21) (with the corrected lower limit w-k), kept separately so tests can compare it
  // with increment(). Alternating sum: less accurate than increment() for large w.
  double increment_eq21(int w, int k) const;
  int max_n() const { return max_n_; }

 private:
  std::size_t index(int w, int k) const {
    return static_cast<std::size_t>(w) * static_cast<std::size_t>(max_n_ + 1) + static_cast<std::size_t>(k);
  }

  std::string label_;
  std::function<double(int)> psi_;
  int max_n_;
  std::vector<long double> psi_values_;   // Psi(0..max_n)
  std::vector<long double> kernel_;       // P[w1][w2], flat, (max_n+1)^2 entries
  std::vector<double> increments_;        // delta[w][k], flat
};

std::unique_ptr<FailureModel> make_exponential(double lambda);
// Lethal-shock model [LSh]: Psi(x) = (1 - beta) x + beta for x > 0. beta in [0, 1].
std::unique_ptr<FailureModel> make_lethal_shock(double beta, int max_n);
// Binomial failure rate model [BFR], driftless, calibrated: Psi(x) = (1 - (1 - beta)^x) / beta.
// Here beta = p is the probability that a common-cause shock fails a given working component. beta in (0, 1].
std::unique_ptr<FailureModel> make_binomial_failure_rate(double beta, int max_n);

}  // namespace reliability
