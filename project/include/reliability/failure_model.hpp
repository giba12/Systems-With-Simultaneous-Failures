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

#include <functional>
#include <memory>
#include <string>

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

class LevyFrailty final : public FailureModel {
 public:
  LevyFrailty(std::string label, std::function<double(int)> psi, int max_n);
  std::string name() const override;
  double exit_rate(int w) const override;
  double transition_prob(int w1, int w2) const override;
  double increment(int w, int k) const override;
  // Paper Eq. (21), kept separately so tests can compare it with increment().
  double increment_eq21(int w, int k) const;

 private:
  std::string label_;
  std::function<double(int)> psi_;
  int max_n_;
};

std::unique_ptr<FailureModel> make_exponential(double lambda);
std::unique_ptr<FailureModel> make_lethal_shock(double beta, int max_n);

}  // namespace reliability
