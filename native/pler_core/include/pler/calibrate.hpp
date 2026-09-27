#pragma once
#include "pler/types.hpp"
#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace pler {

struct CalibratePoint {
  int num_rays = 0;
  double pler_db = 0;
  double mse = 0;
  double time_s = 0;
  double rel_change = 0;  // vs previous point
};

struct CalibrateResult {
  std::vector<CalibratePoint> points;
  int suggested_rays = 0;  // smallest N with rel < converge_rel (or last)
  std::string csv_path;
  std::string error;
  bool ok = true;
};

/** Log-spaced ray sweep (converge off). Max 25 points. */
CalibrateResult calibrate_rays(const std::string& ref_path,
                               const std::string& test_path,
                               PlerOptions opt,
                               ProgressFn progress = nullptr,
                               std::atomic<bool>* cancel = nullptr);

std::vector<int> log_spaced_rays(int min_rays, int max_rays, int max_points = 25);

}  // namespace pler
