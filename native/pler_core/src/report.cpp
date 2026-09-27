#include "pler/report.hpp"
#include "pler/log.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <ctime>
#include <deque>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace pler {
namespace {

std::string stamp_folder() {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%04d%02d%02d_%02d%02d%02d", tm.tm_year + 1900,
                tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  return buf;
}

void write_svg_scatter(const std::string& path, const std::string& title,
                       const std::vector<double>& xs,
                       const std::vector<double>& ys, bool log_x) {
  const int W = 640, H = 360, pad = 40;
  if (xs.empty() || xs.size() != ys.size()) {
    std::ofstream empty(path);
    empty << "<svg xmlns='http://www.w3.org/2000/svg' width='" << W << "' height='"
          << H << "'><text x='20' y='40'>no data</text></svg>\n";
    return;
  }
  double xmin = *std::min_element(xs.begin(), xs.end());
  double xmax = *std::max_element(xs.begin(), xs.end());
  double ymin = *std::min_element(ys.begin(), ys.end());
  double ymax = *std::max_element(ys.begin(), ys.end());
  if (xmax <= xmin) xmax = xmin + 1;
  if (ymax <= ymin) ymax = ymin + 1;

  auto mapx = [&](double x) {
    double t;
    if (log_x) {
      double a = std::log10(std::max(1e-12, xmin));
      double b = std::log10(std::max(1e-12, xmax));
      t = (std::log10(std::max(1e-12, x)) - a) / (b - a + 1e-15);
    } else {
      t = (x - xmin) / (xmax - xmin);
    }
    return pad + t * (W - 2 * pad);
  };
  auto mapy = [&](double y) {
    double t = (y - ymin) / (ymax - ymin);
    return H - pad - t * (H - 2 * pad);
  };

  std::ofstream out(path);
  out << "<svg xmlns='http://www.w3.org/2000/svg' width='" << W << "' height='"
      << H << "'>\n";
  out << "<rect width='100%' height='100%' fill='#fafafa'/>\n";
  out << "<text x='" << pad << "' y='24' font-size='14'>" << title << "</text>\n";
  out << "<line x1='" << pad << "' y1='" << (H - pad) << "' x2='" << (W - pad)
      << "' y2='" << (H - pad) << "' stroke='#333'/>\n";
  out << "<line x1='" << pad << "' y1='" << pad << "' x2='" << pad << "' y2='"
      << (H - pad) << "' stroke='#333'/>\n";
  for (size_t i = 0; i < xs.size(); i++) {
    out << "<circle cx='" << mapx(xs[i]) << "' cy='" << mapy(ys[i])
        << "' r='4' fill='#c45c26'/>\n";
  }
  out << "</svg>\n";
}

}  // namespace

ReportPaths write_research_report(
    const std::string& cache_dir,
    const std::vector<std::pair<std::string, PlerResult>>& rows) {
  ReportPaths rp;
  rp.dir = cache_dir + "/reports/" + stamp_folder();
  std::error_code ec;
  std::filesystem::create_directories(rp.dir, ec);
  rp.summary_csv = rp.dir + "/summary.csv";
  rp.pler_vs_rays_svg = rp.dir + "/pler_vs_rays.svg";
  rp.mse_vs_rays_svg = rp.dir + "/mse_vs_rays.svg";
  rp.pler_vs_verts_svg = rp.dir + "/pler_vs_verts.svg";

  write_research_csv(rp.summary_csv, rows);

  std::vector<double> rays, pler, mse, verts, pler2;
  for (const auto& row : rows) {
    if (!row.second.ok) continue;
    rays.push_back(static_cast<double>(row.second.num_rays));
    pler.push_back(row.second.pler_db);
    mse.push_back(row.second.mse);
    if (row.second.vertex_count_test > 0) {
      verts.push_back(static_cast<double>(row.second.vertex_count_test));
      pler2.push_back(row.second.pler_db);
    }
  }
  write_svg_scatter(rp.pler_vs_rays_svg, "PLER vs rays", rays, pler, false);
  write_svg_scatter(rp.mse_vs_rays_svg, "MSE vs rays", rays, mse, false);
  write_svg_scatter(rp.pler_vs_verts_svg, "PLER vs vertices (log x)", verts, pler2,
                    true);
  append_log(cache_dir, "report written " + rp.dir);
  return rp;
}

bool dump_ray_depths(const std::string& path, const PlerResult& r) {
  if (r.L_ref.empty() || r.L_test.empty()) return false;
  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
  }
  std::ofstream out(path);
  if (!out) return false;
  out << "i,dir_x,dir_y,dir_z,L_ref,L_test,abs_err\n";
  for (size_t i = 0; i < r.L_ref.size(); i++) {
    double dx = 0, dy = 0, dz = 0;
    if (i < r.ray_dirs.size()) {
      dx = r.ray_dirs[i].x;
      dy = r.ray_dirs[i].y;
      dz = r.ray_dirs[i].z;
    }
    out << i << "," << dx << "," << dy << "," << dz << "," << r.L_ref[i] << ","
        << r.L_test[i] << "," << std::fabs(r.L_ref[i] - r.L_test[i]) << "\n";
  }
  return true;
}

std::string read_log_tail(const std::string& cache_dir, int max_lines) {
  std::ifstream in(default_log_path(cache_dir));
  if (!in) return "(no log yet)";
  std::deque<std::string> lines;
  std::string line;
  while (std::getline(in, line)) {
    lines.push_back(line);
    if (static_cast<int>(lines.size()) > max_lines) lines.pop_front();
  }
  std::ostringstream os;
  for (const auto& l : lines) os << l << "\n";
  return os.str();
}

}  // namespace pler
