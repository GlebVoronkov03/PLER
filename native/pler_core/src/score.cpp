#include "pler/score.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pler {

LengthStats hits_to_lengths(const std::vector<double>& t_hit, double R) {
  LengthStats s;
  s.L.resize(t_hit.size());
  for (size_t i = 0; i < t_hit.size(); i++) {
    if (!std::isfinite(t_hit[i]) || t_hit[i] < 0 || t_hit[i] > R * 1.0001) {
      s.L[i] = R;  // miss: sphere to center
      s.miss_count++;
    } else {
      s.L[i] = t_hit[i];
    }
  }
  return s;
}

PlerResult score_pler(const LengthStats& ref, const LengthStats& test, double R,
                      int num_rays) {
  if (ref.L.size() != test.L.size())
    throw PlerError("Length array size mismatch");
  if (ref.L.empty()) throw PlerError("No samples to score");

  const size_t n = ref.L.size();
  double sum_sq = 0, sum_abs = 0, max_abs = 0;
  double Lmin = ref.L[0];
  for (size_t i = 0; i < n; i++) {
    double e = ref.L[i] - test.L[i];
    sum_sq += e * e;
    sum_abs += std::fabs(e);
    max_abs = std::max(max_abs, std::fabs(e));
    Lmin = std::min(Lmin, ref.L[i]);
  }

  const double mse = sum_sq / static_cast<double>(n);
  const double peak = R - Lmin;  // max center-to-surface on reference
  PlerResult r;
  r.mse = mse;
  r.mean_error = sum_abs / static_cast<double>(n);
  r.max_error = max_abs;
  r.peak = peak;
  r.num_rays = num_rays;
  r.miss_rate_ref = static_cast<double>(ref.miss_count) / static_cast<double>(n);
  r.miss_rate_test = static_cast<double>(test.miss_count) / static_cast<double>(n);

  if (mse <= 1e-10) {
    r.pler_db = 100.0;
  } else {
    double peak2 = peak * peak;
    if (peak2 <= 1e-30) peak2 = 1e-30;
    r.pler_db = 10.0 * std::log10(peak2 / mse);
  }
  return r;
}

}  // namespace pler
