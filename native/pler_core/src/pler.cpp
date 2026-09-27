#include "pler/pler.hpp"
#include "pler/align_icp.hpp"
#include "pler/align_volume.hpp"
#include "pler/cache.hpp"
#include "pler/frame.hpp"
#include "pler/log.hpp"
#include "pler/mesh_io.hpp"
#include "pler/raycast.hpp"
#include "pler/report.hpp"
#include "pler/score.hpp"
#include "pler/sysinfo.hpp"
#include "pler/topology.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace pler {
namespace {

void quantize_mesh_f32(Mesh& m) {
  for (auto& v : m.vertices) {
    v.x = static_cast<double>(static_cast<float>(v.x));
    v.y = static_cast<double>(static_cast<float>(v.y));
    v.z = static_cast<double>(static_cast<float>(v.z));
  }
}

}  // namespace

int suggest_rays(std::size_t vertices, std::size_t triangles, int min_rays,
                 int max_rays) {
  if (min_rays < 100) min_rays = 100;
  if (max_rays < min_rays) max_rays = min_rays;
  double complexity = static_cast<double>(vertices) / 1000.0 +
                      static_cast<double>(triangles) / 2000.0;
  int n = static_cast<int>(min_rays * std::log1p(complexity));
  return std::max(min_rays, std::min(max_rays, n));
}

PlerResult compute_pler(const std::string& ref_path, const std::string& test_path,
                        const PlerOptions& opt, ProgressFn progress,
                        std::atomic<bool>* cancel) {
  auto report = [&](float p, const char* msg) {
    if (progress) progress(p, msg);
  };

  auto start = std::chrono::steady_clock::now();
  PlerResult result;

  try {
    report(0.02f, "Loading meshes");
    Mesh ref = load_mesh(ref_path);
    Mesh test = load_mesh(test_path);
    result.vertex_count_ref = ref.vertex_count();
    result.vertex_count_test = test.vertex_count();

    if (cancel && cancel->load()) {
      result.ok = false;
      result.error = "cancelled";
      return result;
    }

    PlerOptions local = opt;
    if (local.num_rays <= 0) {
      local.num_rays = suggest_rays(ref.vertex_count(), ref.triangle_count(),
                                    local.min_rays, local.max_rays);
      report(0.05f, "Adaptive rays");
    }

    // Warn if over recommended (CLI/GUI can also check)
    MemoryInfo mem = query_memory();
    int rec = recommended_max_rays(mem);
    if (local.num_rays > rec) {
      append_log(local.cache_dir,
                 "warning: num_rays=" + std::to_string(local.num_rays) +
                     " exceeds recommended_max=" + std::to_string(rec));
    }

    AlignMode am = resolve_align_mode(local);
    result.align_mode_used = align_mode_name(am);
    result.align_volume_used = (am == AlignMode::VolumeIoU || am == AlignMode::IoUThenICP);
    if (am == AlignMode::VolumeIoU || am == AlignMode::IoUThenICP) {
      report(0.08f, "Volume IoU alignment");
      result.volume_iou = align_volume_iou(ref, test, local.voxel_resolution);
    }
    if (am == AlignMode::ICP || am == AlignMode::IoUThenICP) {
      report(0.12f, "ICP alignment");
      align_icp(ref, test, local.icp_iterations);
    }

    // World radius before normalize (for mm display)
    SphereFrame world = make_shared_sphere(ref, test, local.sphere_margin);
    result.sphere_radius_world = world.radius;

    report(0.15f, "Shared sphere normalize");
    SphereFrame frame = normalize_pair(ref, test, local.sphere_margin);
    const double R = frame.radius;

    if (local.use_float32) {
      quantize_mesh_f32(ref);
      quantize_mesh_f32(test);
    }

    if (local.compute_tsi) {
      report(0.18f, "Topology");
      result.topo_ref = compute_topology(ref);
      result.topo_test = compute_topology(test);
      result.tsi = topological_similarity(result.topo_ref, result.topo_test);
    }

    auto run_once = [&](int n_rays, float p0, float p1) -> PlerResult {
      if (cancel && cancel->load()) throw PlerError("cancelled");
      auto dirs = cached_fibonacci(n_rays, local.cache_dir, local.cache_enabled);
      auto rays = make_inward_rays(dirs, R);
      Backend be = select_backend(local.prefer_cuda);
      auto map_prog = [&](float t, const char* label) {
        report(p0 + (p1 - p0) * t, label);
      };
      report(p0, "Ray casting reference");
      auto th_ref = cast_rays(ref, rays, be, cancel, [&](float f) {
        map_prog(0.45f * f, "Ray casting reference");
      });
      if (cancel && cancel->load()) throw PlerError("cancelled");
      report(p0 + (p1 - p0) * 0.5f, "Ray casting test");
      auto th_test = cast_rays(test, rays, be, cancel, [&](float f) {
        map_prog(0.5f + 0.45f * f, "Ray casting test");
      });
      if (cancel && cancel->load()) throw PlerError("cancelled");
      auto Lref = hits_to_lengths(th_ref, R);
      auto Ltest = hits_to_lengths(th_test, R);
      PlerResult r = score_pler(Lref, Ltest, R, n_rays);
      r.backend = (be == Backend::CUDA) ? "cuda" : "cpu";
      r.align_volume_used = result.align_volume_used;
      r.volume_iou = result.volume_iou;
      r.tsi = result.tsi;
      r.topo_ref = result.topo_ref;
      r.topo_test = result.topo_test;
      r.vertex_count_ref = result.vertex_count_ref;
      r.vertex_count_test = result.vertex_count_test;
      r.sphere_radius_world = result.sphere_radius_world;
      r.align_mode_used = result.align_mode_used;
      if (local.keep_lengths || local.dump_rays) {
        r.L_ref = Lref.L;
        r.L_test = Ltest.L;
        r.ray_dirs = std::move(dirs);
      }
      return r;
    };

    bool aligned = result.align_volume_used;
    double vol_iou = result.volume_iou;
    TopologyFeatures topo_r = result.topo_ref;
    TopologyFeatures topo_t = result.topo_test;
    double tsi_v = result.tsi;
    auto vref = result.vertex_count_ref;
    auto vtest = result.vertex_count_test;
    double Rworld = result.sphere_radius_world;
    std::string amode = result.align_mode_used;

    int n = local.num_rays;
    result = run_once(n, 0.25f, local.converge ? 0.55f : 0.95f);
    result.align_volume_used = aligned;
    result.volume_iou = vol_iou;
    result.topo_ref = topo_r;
    result.topo_test = topo_t;
    result.tsi = tsi_v;
    result.vertex_count_ref = vref;
    result.vertex_count_test = vtest;
    result.sphere_radius_world = Rworld;
    result.align_mode_used = amode;

    if (local.converge) {
      double prev = result.pler_db;
      for (int iter = 0; iter < 4; iter++) {
        n *= 2;
        if (n > local.max_rays * 2) break;
        char msg[64];
        std::snprintf(msg, sizeof(msg), "Convergence at %d rays", n);
        report(0.55f + 0.08f * static_cast<float>(iter), msg);
        auto next = run_once(n, 0.55f + 0.08f * static_cast<float>(iter),
                             0.63f + 0.08f * static_cast<float>(iter));
        next.align_volume_used = aligned;
        next.volume_iou = vol_iou;
        next.topo_ref = topo_r;
        next.topo_test = topo_t;
        next.tsi = tsi_v;
        next.vertex_count_ref = vref;
        next.vertex_count_test = vtest;
        next.sphere_radius_world = Rworld;
        next.align_mode_used = amode;
        double rel =
            std::fabs(next.pler_db - prev) / std::max(1.0, std::fabs(prev));
        result = next;
        prev = next.pler_db;
        if (rel < local.converge_rel) break;
      }
    }

    if (local.w_pler >= 0.0 && local.w_tsi >= 0.0 && result.tsi >= 0.0) {
      double w1 = local.w_pler, w2 = local.w_tsi;
      double s = w1 + w2;
      if (s <= 1e-12) throw PlerError("weights must sum > 0");
      w1 /= s;
      w2 /= s;
      double pler_n = std::min(1.0, std::max(0.0, result.pler_db / 100.0));
      result.combined = w1 * pler_n + w2 * result.tsi;
    }

    if (local.dump_rays && !result.L_ref.empty()) {
      dump_ray_depths(local.cache_dir + "/ray_depths_last.csv", result);
    }

    auto end = std::chrono::steady_clock::now();
    result.computation_time_s =
        std::chrono::duration<double>(end - start).count();
    result.ok = true;
    log_result(local.cache_dir, ref_path, test_path, result);
    report(1.0f, "Done");
    return result;
  } catch (const std::exception& ex) {
    result.ok = false;
    result.error = ex.what();
    auto end = std::chrono::steady_clock::now();
    result.computation_time_s =
        std::chrono::duration<double>(end - start).count();
    log_result(opt.cache_dir, ref_path, test_path, result);
    return result;
  }
}

}  // namespace pler
