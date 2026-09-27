#include "pler/config.hpp"
#include "pler/util.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pler {
namespace {

std::string trim(std::string s) {
  auto a = s.find_first_not_of(" \t\r\n");
  if (a == std::string::npos) return {};
  auto b = s.find_last_not_of(" \t\r\n");
  return s.substr(a, b - a + 1);
}

bool parse_bool(const std::string& v, bool& out) {
  if (v == "true" || v == "True" || v == "1" || v == "yes") {
    out = true;
    return true;
  }
  if (v == "false" || v == "False" || v == "0" || v == "no") {
    out = false;
    return true;
  }
  return false;
}

AlignMode parse_align(const std::string& v) {
  if (v == "iou" || v == "volume" || v == "volume_iou") return AlignMode::VolumeIoU;
  if (v == "icp") return AlignMode::ICP;
  if (v == "iou+icp" || v == "iou_icp" || v == "both") return AlignMode::IoUThenICP;
  return AlignMode::Off;
}

}  // namespace

bool load_options_yaml(const std::string& path, PlerOptions& opt) {
  std::ifstream in(path);
  if (!in) return false;
  std::string line;
  while (std::getline(in, line)) {
    auto hash = line.find('#');
    if (hash != std::string::npos) line = line.substr(0, hash);
    line = trim(line);
    if (line.empty()) continue;
    auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    std::string key = trim(line.substr(0, colon));
    std::string val = trim(line.substr(colon + 1));
    if (!val.empty() && val.front() == '"' && val.back() == '"')
      val = val.substr(1, val.size() - 2);
    try {
      if (key == "num_rays")
        opt.num_rays = std::stoi(val);
      else if (key == "min_rays")
        opt.min_rays = std::stoi(val);
      else if (key == "max_rays")
        opt.max_rays = std::stoi(val);
      else if (key == "align_volume") {
        parse_bool(val, opt.align_volume);
        if (opt.align_volume) opt.align_mode = AlignMode::VolumeIoU;
      } else if (key == "align_mode")
        opt.align_mode = parse_align(val);
      else if (key == "compute_tsi")
        parse_bool(val, opt.compute_tsi);
      else if (key == "converge")
        parse_bool(val, opt.converge);
      else if (key == "converge_rel")
        opt.converge_rel = std::stod(val);
      else if (key == "voxel_resolution")
        opt.voxel_resolution = std::stoi(val);
      else if (key == "sphere_margin")
        opt.sphere_margin = std::stod(val);
      else if (key == "prefer_cuda")
        parse_bool(val, opt.prefer_cuda);
      else if (key == "w_pler")
        opt.w_pler = std::stod(val);
      else if (key == "w_tsi")
        opt.w_tsi = std::stod(val);
      else if (key == "cache_dir")
        opt.cache_dir = val;
      else if (key == "cache_enabled")
        parse_bool(val, opt.cache_enabled);
      else if (key == "use_float32")
        parse_bool(val, opt.use_float32);
      else if (key == "display_mm")
        parse_bool(val, opt.display_mm);
      else if (key == "units_to_mm")
        opt.units_to_mm = std::stod(val);
      else if (key == "dump_rays")
        parse_bool(val, opt.dump_rays);
      else if (key == "theme")
        opt.theme = std::stoi(val);
      else if (key == "icp_iterations")
        opt.icp_iterations = std::stoi(val);
    } catch (...) {
    }
  }
  return true;
}

std::string find_default_config_path() {
  if (const char* env = std::getenv("PLER_CONFIG")) {
    if (env[0]) return env;
  }
  const char* names[] = {"config/pler.yaml", "pler.yaml"};
  for (const char* n : names) {
    std::ifstream t(n);
    if (t) return n;
  }
  std::string exe = executable_dir();
  if (!exe.empty()) {
    std::string cands[] = {
        exe + "/config/pler.yaml",
        exe + "/../config/pler.yaml",
        exe + "/../../config/pler.yaml",
        exe + "/../../../config/pler.yaml",
        exe + "/../../../../config/pler.yaml",
    };
    for (const auto& c : cands) {
      std::ifstream t(c);
      if (t) return c;
    }
  }
  return {};
}

void apply_default_config(PlerOptions& opt) {
  std::string p = find_default_config_path();
  if (!p.empty()) load_options_yaml(p, opt);
}

}  // namespace pler
