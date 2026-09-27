#pragma once
#include "pler/types.hpp"
#include <string>
#include <utility>
#include <vector>

namespace pler {

struct ReportPaths {
  std::string dir;
  std::string summary_csv;
  std::string pler_vs_rays_svg;
  std::string pler_vs_verts_svg;
  std::string mse_vs_rays_svg;
};

/** Write research report folder with CSV + simple SVG plots. */
ReportPaths write_research_report(
    const std::string& cache_dir,
    const std::vector<std::pair<std::string, PlerResult>>& rows);

bool dump_ray_depths(const std::string& path, const PlerResult& r);

std::string read_log_tail(const std::string& cache_dir, int max_lines = 200);

}  // namespace pler
