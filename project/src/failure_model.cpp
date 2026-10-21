// failure_model.cpp - [Exp] and the Levy-frailty (lethal-shock) failure models.
//                                                    (Plan: Day 6 for [Exp], Day 7 for the rest)
#include "reliability/failure_model.hpp"

#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string>

#include "reliability/errors.hpp"
#include "reliability/state.hpp"

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

namespace {

// Rounding noise allowed in the transition kernel before a model is refused.
constexpr long double kKernelTolerance = 1e-9L;

long double binom_ld(int n, int k) { return static_cast<long double>(binomial(n, k)); }

}  // namespace

// ALGORITHM   (everything is computed once, in long double)
//   check the arguments: 1 <= max_n <= kMaxLevyFrailtyComponents (16), psi not empty
//   Psi(0) must be 0; Psi(w) must be finite, positive and non-decreasing for w = 1..max_n
//   calibration: |Psi(1) - 1| <= 1e-12 (every component has unit mean lifetime)
//   kernel, Eq. (20), for 1 <= w2 < w1 <= max_n ... and w2 = 0:
//       P[w1][w2] = C(w1, w1-w2) * sum_{w=w2..w1} (-1)^(w-w2+1) * C(w1-w2, w1-w) * Psi(w) / Psi(w1)
//   guard: each row sums to 1 within 1e-9 and no entry is below -1e-9; tiny negatives become 0.
//       If this fails, Psi is not a valid Laplace exponent, OR max_n is too large for the precision
//       of the alternating sum (docs/paper-validation-report.md section 4): the model is refused.
//   increments, for each w <= max_n (all terms >= 0):
//       f[w] = 1; for v = w-1 down to 0:  f[v] = sum_{v' > v} f[v'] * P[v'][v]
//       delta[w][k] = f[w-k] / Psi(w-k)                   (paper, after Proposition 3)
LevyFrailty::LevyFrailty(std::string label, std::function<double(int)> psi, int max_n)
    : label_(std::move(label)), psi_(std::move(psi)), max_n_(max_n) {
  if (!psi_) {
    throw ModelError("LevyFrailty: the Laplace exponent Psi must not be empty");
  }
  if (max_n_ < 1 || max_n_ > kMaxLevyFrailtyComponents) {
    throw ModelError("LevyFrailty: max_n must be between 1 and " + std::to_string(kMaxLevyFrailtyComponents) +
                     ", got " + std::to_string(max_n_) +
                     " (the alternating sum of Eq. (20) loses too much precision in long double beyond that)");
  }

  const std::size_t size = static_cast<std::size_t>(max_n_ + 1);
  psi_values_.assign(size, 0.0L);
  if (std::fabs(psi_(0)) > 1e-15) {
    throw ModelError("LevyFrailty: Psi(0) must be 0");
  }
  for (int w = 1; w <= max_n_; ++w) {
    const double value = psi_(w);
    if (!std::isfinite(value) || value <= 0.0) {
      throw ModelError("LevyFrailty: Psi(" + std::to_string(w) + ") must be positive and finite, got " +
                       std::to_string(value));
    }
    psi_values_[static_cast<std::size_t>(w)] = value;
    if (w > 1 && psi_values_[static_cast<std::size_t>(w)] < psi_values_[static_cast<std::size_t>(w - 1)] * (1.0L - 1e-12L)) {
      throw ModelError("LevyFrailty: Psi must be non-decreasing, but Psi(" + std::to_string(w) +
                       ") < Psi(" + std::to_string(w - 1) + ")");
    }
  }
  if (std::fabs(psi_values_[1] - 1.0L) > 1e-12L) {
    throw ModelError("LevyFrailty: the model must be calibrated so that Psi(1) = 1 (unit mean component "
                     "lifetime), got " + std::to_string(static_cast<double>(psi_values_[1])));
  }

  // transition kernel, Eq. (20)
  kernel_.assign(size * size, 0.0L);
  for (int w1 = 1; w1 <= max_n_; ++w1) {
    long double row_sum = 0.0L;
    for (int w2 = 0; w2 < w1; ++w2) {
      long double sum = 0.0L;
      for (int w = w2; w <= w1; ++w) {
        const long double sign = ((w - w2 + 1) % 2 == 0) ? 1.0L : -1.0L;
        sum += sign * binom_ld(w1 - w2, w1 - w) * psi_values_[static_cast<std::size_t>(w)] /
               psi_values_[static_cast<std::size_t>(w1)];
      }
      long double p = binom_ld(w1, w1 - w2) * sum;
      if (p < -kKernelTolerance) {
        throw ModelError("LevyFrailty '" + label_ + "': the transition probability P[" + std::to_string(w1) +
                         "][" + std::to_string(w2) + "] is negative (" +
                         std::to_string(static_cast<double>(p)) +
                         "). Psi is not a valid Laplace exponent, or max_n = " + std::to_string(max_n_) +
                         " is too large for the numerical precision");
      }
      if (p < 0.0L) {
        p = 0.0L;
      }
      kernel_[static_cast<std::size_t>(w1) * size + static_cast<std::size_t>(w2)] = p;
      row_sum += p;
    }
    if (std::fabs(row_sum - 1.0L) > kKernelTolerance) {
      throw ModelError("LevyFrailty '" + label_ + "': the transition probabilities from " +
                       std::to_string(w1) + " working components sum to " +
                       std::to_string(static_cast<double>(row_sum)) +
                       ", not 1. Psi is not a valid Laplace exponent, or max_n = " +
                       std::to_string(max_n_) + " is too large for the numerical precision");
    }
  }

  // increments, by the non-negative recursion
  increments_.assign(size * size, 0.0);
  for (int w = 1; w <= max_n_; ++w) {
    std::vector<long double> reach(static_cast<std::size_t>(w) + 1, 0.0L);  // reach[v] = f^{(w)}_v
    reach[static_cast<std::size_t>(w)] = 1.0L;
    for (int v = w - 1; v >= 0; --v) {
      long double total = 0.0L;
      for (int above = v + 1; above <= w; ++above) {
        total += reach[static_cast<std::size_t>(above)] *
                 kernel_[static_cast<std::size_t>(above) * size + static_cast<std::size_t>(v)];
      }
      reach[static_cast<std::size_t>(v)] = total;
    }
    for (int k = 0; k < w; ++k) {
      increments_[index(w, k)] = static_cast<double>(reach[static_cast<std::size_t>(w - k)] /
                                                     psi_values_[static_cast<std::size_t>(w - k)]);
    }
  }
}

// ALGORITHM   return label_
std::string LevyFrailty::name() const { return label_; }

// ALGORITHM   require 0 <= w <= max_n;  return Psi(w)  (0 when no component works)      Eq. (19)
double LevyFrailty::exit_rate(int w) const {
  if (w < 0 || w > max_n_) {
    throw ModelError("exit_rate: w must be between 0 and " + std::to_string(max_n_) + ", got " +
                     std::to_string(w));
  }
  return static_cast<double>(psi_values_[static_cast<std::size_t>(w)]);
}

// ALGORITHM   look up the precomputed kernel                                            Eq. (20)
//   zero unless 0 <= w2 < w1 <= max_n ("and zero otherwise" in the paper)
double LevyFrailty::transition_prob(int w1, int w2) const {
  if (w1 < 0 || w2 < 0 || w1 > max_n_) {
    throw ModelError("transition_prob: need 0 <= w2 and 0 <= w1 <= " + std::to_string(max_n_) +
                     ", got w1 = " + std::to_string(w1) + ", w2 = " + std::to_string(w2));
  }
  if (w1 == 0 || w2 >= w1) {
    return 0.0;
  }
  return static_cast<double>(kernel_[static_cast<std::size_t>(w1) * static_cast<std::size_t>(max_n_ + 1) +
                                     static_cast<std::size_t>(w2)]);
}

// ALGORITHM   look up the precomputed increment table (route 1)
double LevyFrailty::increment(int w, int k) const {
  if (w < 1 || w > max_n_ || k < 0 || k >= w) {
    throw ModelError("increment: need 1 <= w <= " + std::to_string(max_n_) + " and 0 <= k < w, got w = " +
                     std::to_string(w) + ", k = " + std::to_string(k));
  }
  return increments_[index(w, k)];
}

// ALGORITHM   Eq. (21) directly (route 2), with the corrected lower limit i = w-k:
//   sum_{i=w-k..w} C(w,k) * C(k, w-i) * (-1)^(i-w+k) / Psi(i)
double LevyFrailty::increment_eq21(int w, int k) const {
  if (w < 1 || w > max_n_ || k < 0 || k >= w) {
    throw ModelError("increment_eq21: need 1 <= w <= " + std::to_string(max_n_) + " and 0 <= k < w, got w = " +
                     std::to_string(w) + ", k = " + std::to_string(k));
  }
  long double total = 0.0L;
  for (int i = w - k; i <= w; ++i) {
    const long double sign = ((i - w + k) % 2 == 0) ? 1.0L : -1.0L;
    total += sign * binom_ld(w, k) * binom_ld(k, w - i) / psi_values_[static_cast<std::size_t>(i)];
  }
  return static_cast<double>(total);
}

// ---------------- factories ----------------

// ALGORITHM   return a new IndependentExponential(lambda)
std::unique_ptr<FailureModel> make_exponential(double lambda) {
  return std::make_unique<IndependentExponential>(lambda);
}

// ALGORITHM
//   if beta is NaN or outside [0, 1]: throw ModelError("beta must lie in [0, 1]")
//   Psi(x) = 0 for x = 0, else (1 - beta) * x + beta       // components fail alone, plus a lethal shock
std::unique_ptr<FailureModel> make_lethal_shock(double beta, int max_n) {
  if (!(beta >= 0.0 && beta <= 1.0)) {  // also rejects NaN
    std::ostringstream message;
    message << "beta must lie in [0, 1], got " << beta;
    throw ModelError(message.str());
  }
  std::ostringstream label;
  label << std::setprecision(17) << "lethal_shock(beta=" << beta << ")";
  return std::make_unique<LevyFrailty>(
      label.str(), [beta](int x) { return x == 0 ? 0.0 : (1.0 - beta) * x + beta; }, max_n);
}

// ALGORITHM
//   if beta is NaN or outside (0, 1]: throw ModelError
//   Psi(x) = (1 - (1-beta)^x) / beta  = -expm1(x * log1p(-beta)) / beta   (accurate for small beta)
std::unique_ptr<FailureModel> make_binomial_failure_rate(double beta, int max_n) {
  if (!(beta > 0.0 && beta <= 1.0)) {
    std::ostringstream message;
    message << "beta must lie in (0, 1] for the binomial failure rate model, got " << beta;
    throw ModelError(message.str());
  }
  std::ostringstream label;
  label << std::setprecision(17) << "binomial_failure_rate(beta=" << beta << ")";
  return std::make_unique<LevyFrailty>(
      label.str(),
      [beta](int x) {
        if (x == 0) {
          return 0.0;
        }
        const double log_survive = beta >= 1.0 ? -std::numeric_limits<double>::infinity() : std::log1p(-beta);
        return -std::expm1(static_cast<double>(x) * log_survive) / beta;
      },
      max_n);
}

}  // namespace reliability
