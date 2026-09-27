#include "pler/calibrate.hpp"
#include "pler/log.hpp"
#include "pler/pler.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace pler {

std::vector<int> log_spaced_rays(int min_rays, int max_rays, int max_points) {
  if (min_rays < 100) min_rays = 100;
  if (max_rays < min_rays) max_rays = min_rays;
  if (max_points < 2) max_points = 2;
  if (max_points > 25) max_points = 25;

  std::vector<int> out;
  if (min_rays == max_rays) {
    out.push_back(min_rays);
    return out;
  }
  for (int i = 0; i < max_points; i++) {
    double t = static_cast<double>(i) / static_cast<double>(max_points - 1);
    double v = min_rays * std::pow(static_cast<double>(max_rays) / min_rays, t);
    int n = static_cast<int>(std::lround(v));
    if (out.empty() || n != out.back()) out.push_back(n);
  }
  if (out.back() != max_rays) out.push_back(max_rays);
  return out;
}

CalibrateResult calibrate_rays(const std::string& ref_path,
                               const std::string& test_path, PlerOptions opt,
                               ProgressFn progress, std::atomic<bool>* cancel) {
  CalibrateResult cr;
  opt.converge = false;
  auto counts = log_spaced_rays(opt.min_rays, opt.max_rays, 25);
  double prev = 0;
  bool have_prev = false;
  cr.suggested_rays = counts.empty() ? opt.num_rays : counts.back();

  for (size_t i = 0; i < counts.size(); i++) {
    if (cancel && cancel->load()) {
      cr.ok = false;
      cr.error = "cancelled";
      return cr;
    }
    int n = counts[i];
    if (progress)
      progress(static_cast<float>(i) / static_cast<float>(counts.size()),
               "Calibrate @" + std::to_string(n));
    PlerOptions local = opt;
    local.num_rays = n;
    local.keep_lengths = false;
    local.dump_rays = false;
    auto start = std::chrono::steady_clock::now();
    PlerResult r = compute_pler(ref_path, test_path, local, nullptr, cancel);
    auto end = std::chrono::steady_clock::now();
    if (!r.ok) {
      cr.ok = false;
      cr.error = r.error;
      return cr;
    }
    CalibratePoint pt;
    pt.num_rays = r.num_rays;
    pt.pler_db = r.pler_db;
    pt.mse = r.mse;
    pt.time_s = std::chrono::duration<double>(end - start).count();
    if (have_prev) {
      pt.rel_change =
          std::fabs(pt.pler_db - prev) / std::max(1.0, std::fabs(prev));
      if (pt.rel_change < opt.converge_rel && cr.suggested_rays == counts.back())
        cr.suggested_rays = pt.num_rays;
    }
    prev = pt.pler_db;
    have_prev = true;
    cr.points.push_back(pt);
  }

  // Find smallest N with rel < thresh
  for (size_t i = 1; i < cr.points.size(); i++) {
    if (cr.points[i].rel_change < opt.converge_rel) {
      cr.suggested_rays = cr.points[i].num_rays;
      break;
    }
    cr.suggested_rays = cr.points[i].num_rays;
  }

  std::error_code ec;
  std::filesystem::create_directories(opt.cache_dir, ec);
  auto now = std::chrono::system_clock::now().time_since_epoch().count();
  cr.csv_path = opt.cache_dir + "/calibrate_" + std::to_string(now) + ".csv";
  std::ofstream out(cr.csv_path);
  if (out) {
    out << "num_rays,pler_db,mse,time_s,rel_change\n";
    for (const auto& p : cr.points)
      out << p.num_rays << "," << p.pler_db << "," << p.mse << "," << p.time_s
          << "," << p.rel_change << "\n";
  }
  append_log(opt.cache_dir,
             "calibrate suggested_rays=" + std::to_string(cr.suggested_rays) +
                 " points=" + std::to_string(cr.points.size()));
  if (progress) progress(1.f, "Calibrate done");
  return cr;
}

}  // namespace pler
