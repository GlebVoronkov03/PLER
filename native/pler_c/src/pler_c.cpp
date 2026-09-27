#include "pler.h"

#include "pler/pler.hpp"
#include "pler/raycast.hpp"
#include "pler/version.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

thread_local std::string g_last_error;

void set_error(const std::string& msg) { g_last_error = msg; }

void copy_str(char* dst, size_t n, const std::string& s) {
  if (!dst || n == 0) return;
  std::snprintf(dst, n, "%s", s.c_str());
}

}  // namespace

extern "C" {

const char* pler_version_string(void) { return PLER_VERSION_STRING; }
int pler_version_major(void) { return PLER_VERSION_MAJOR; }
int pler_version_minor(void) { return PLER_VERSION_MINOR; }
int pler_version_patch(void) { return PLER_VERSION_PATCH; }

int pler_build_has_cuda(void) {
#if defined(PLER_HAS_CUDA) && PLER_HAS_CUDA
  return 1;
#else
  return 0;
#endif
}

void pler_options_init(pler_options* opt) {
  if (!opt) return;
  std::memset(opt, 0, sizeof(*opt));
  opt->num_rays = 5000;
  opt->min_rays = 1000;
  opt->max_rays = 20000;
  opt->align_mode = PLER_ALIGN_OFF;
  opt->compute_tsi = 0;
  opt->converge = 0;
  opt->prefer_cuda = 1;
  opt->voxel_resolution = 48;
  opt->sphere_margin = 1.02;
  std::snprintf(opt->cache_dir, sizeof(opt->cache_dir), "%s", ".pler_cache");
}

int pler_compute_files(const char* ref_path, const char* test_path,
                       const pler_options* opt, pler_result* out) {
  if (!out) {
    set_error("pler_result pointer is null");
    return -1;
  }
  std::memset(out, 0, sizeof(*out));
  out->tsi = -1.0;
  out->volume_iou = -1.0;
  if (!ref_path || !test_path) {
    set_error("ref_path or test_path is null");
    copy_str(out->error, sizeof(out->error), g_last_error);
    return -2;
  }

  pler::PlerOptions o;
  if (opt) {
    o.num_rays = opt->num_rays;
    o.min_rays = opt->min_rays;
    o.max_rays = opt->max_rays;
    o.align_mode = static_cast<pler::AlignMode>(opt->align_mode);
    o.align_volume = (o.align_mode == pler::AlignMode::VolumeIoU ||
                      o.align_mode == pler::AlignMode::IoUThenICP);
    o.compute_tsi = opt->compute_tsi != 0;
    o.converge = opt->converge != 0;
    o.prefer_cuda = opt->prefer_cuda != 0;
    o.voxel_resolution = opt->voxel_resolution;
    o.sphere_margin = opt->sphere_margin;
    if (opt->cache_dir[0]) o.cache_dir = opt->cache_dir;
  }

  try {
    pler::PlerResult r = pler::compute_pler(ref_path, test_path, o);
    out->pler_db = r.pler_db;
    out->mse = r.mse;
    out->mean_error = r.mean_error;
    out->max_error = r.max_error;
    out->peak = r.peak;
    out->miss_rate_ref = r.miss_rate_ref;
    out->miss_rate_test = r.miss_rate_test;
    out->num_rays = r.num_rays;
    out->computation_time_s = r.computation_time_s;
    out->tsi = r.tsi;
    out->volume_iou = r.volume_iou;
    out->ok = r.ok ? 1 : 0;
    copy_str(out->backend, sizeof(out->backend), r.backend);
    copy_str(out->error, sizeof(out->error), r.error);
    if (!r.ok) {
      set_error(r.error.empty() ? "compute failed" : r.error);
      return -3;
    }
    set_error("");
    return 0;
  } catch (const std::exception& ex) {
    set_error(ex.what());
    copy_str(out->error, sizeof(out->error), g_last_error);
    out->ok = 0;
    return -4;
  }
}

const char* pler_last_error(void) {
  return g_last_error.c_str();
}

}  // extern "C"
