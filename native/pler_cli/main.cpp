#include "pler/pler.hpp"
#include "pler/calibrate.hpp"
#include "pler/config.hpp"
#include "pler/frame.hpp"
#include "pler/log.hpp"
#include "pler/mesh_io.hpp"
#include "pler/raycast.hpp"
#include "pler/report.hpp"
#include "pler/score.hpp"
#include "pler/sysinfo.hpp"
#include "pler/util.hpp"
#include "pler/version.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

static void print_result(const pler::PlerResult& r, bool display_mm,
                         double units_to_mm) {
  if (!r.ok) {
    std::cerr << "ERROR: " << r.error << "\n";
    return;
  }
  double scale = 1.0;
  const char* unit = "";
  if (display_mm && r.sphere_radius_world > 0) {
    scale = r.sphere_radius_world * units_to_mm;
    unit = " mm";
  }
  std::cout << "PLER:       " << r.pler_db << " dB\n";
  std::cout << "MSE:        " << r.mse << "\n";
  std::cout << "Peak:       " << (r.peak * scale) << unit << "\n";
  std::cout << "Mean err:   " << (r.mean_error * scale) << unit << "\n";
  std::cout << "Miss ref:   " << (r.miss_rate_ref * 100.0) << " %\n";
  std::cout << "Miss test:  " << (r.miss_rate_test * 100.0) << " %\n";
  std::cout << "Rays:       " << r.num_rays << "\n";
  std::cout << "Backend:    " << r.backend << "\n";
  std::cout << "Align:      " << r.align_mode_used << "\n";
  std::cout << "Verts:      " << r.vertex_count_ref << " / " << r.vertex_count_test
            << "\n";
  std::cout << "Time:       " << r.computation_time_s << " s\n";
  if (r.align_volume_used)
    std::cout << "Align IoU:  " << r.volume_iou << "\n";
  if (r.tsi >= 0.0) {
    std::cout << "TSI:        " << r.tsi << "\n";
  }
  if (r.combined >= 0.0) std::cout << "Combined:   " << r.combined << "\n";
}

static void usage() {
  std::cout
      << "PLER " << PLER_VERSION_STRING << " - geometric fidelity metric\n"
      << "Usage:\n"
      << "  pler <ref.obj> <test.obj> [options]\n"
      << "  pler batch <ref.obj> <pattern*> [options]\n"
      << "  pler calibrate <ref> <test> [options]\n"
      << "  pler report [cache_dir]\n"
      << "  pler predict <features...>   (requires --model PATH)\n"
      << "  pler selftest\n"
      << "  pler gui\n"
      << "  pler --version | -V\n"
      << "Options:\n"
      << "  --rays N          ray count\n"
      << "  --align-volume    volume IoU align (same as --align iou)\n"
      << "  --align MODE      off|iou|icp|iou+icp\n"
      << "  --tsi             topological similarity\n"
      << "  --converge        double rays until PLER stable (<5%)\n"
      << "  --cpu             force CPU backend\n"
      << "  --float32         quantize mesh to float32 before cast\n"
      << "  --mm              display peak/mean in mm\n"
      << "  --units-to-mm X   scale world units to mm (default 1)\n"
      << "  --dump-rays       write ray_depths_last.csv\n"
      << "  --no-cache        disable Fibonacci cache\n"
      << "  --config PATH     YAML config\n"
      << "  --w1 A --w2 B     optional weighted mix\n"
      << "  --model PATH      ML model for predict (required)\n"
      << "Headless: use CLI (batch/calibrate/report/selftest); no GUI needed.\n";
}

static void print_version() {
  std::cout << "PLER " << PLER_VERSION_STRING;
#if defined(PLER_HAS_CUDA) && PLER_HAS_CUDA
  std::cout << "  cuda=yes";
#else
  std::cout << "  cuda=no";
#endif
  std::cout << "\n";
}

static bool parse_align_arg(const std::string& v, pler::AlignMode& out) {
  if (v == "off") {
    out = pler::AlignMode::Off;
    return true;
  }
  if (v == "iou" || v == "volume") {
    out = pler::AlignMode::VolumeIoU;
    return true;
  }
  if (v == "icp") {
    out = pler::AlignMode::ICP;
    return true;
  }
  if (v == "iou+icp" || v == "both") {
    out = pler::AlignMode::IoUThenICP;
    return true;
  }
  return false;
}

static bool parse_options(int argc, char** argv, int start, pler::PlerOptions& opt,
                          std::string* model_path = nullptr) {
  for (int i = start; i < argc; i++) {
    std::string a = argv[i];
    if (a == "--rays" && i + 1 < argc) {
      opt.num_rays = std::stoi(argv[++i]);
    } else if (a == "--align-volume") {
      opt.align_mode = pler::AlignMode::VolumeIoU;
      opt.align_volume = true;
    } else if (a == "--align" && i + 1 < argc) {
      pler::AlignMode m;
      if (!parse_align_arg(argv[++i], m)) {
        std::cerr << "Bad --align value\n";
        return false;
      }
      opt.align_mode = m;
      opt.align_volume = (m == pler::AlignMode::VolumeIoU ||
                          m == pler::AlignMode::IoUThenICP);
    } else if (a == "--tsi") {
      opt.compute_tsi = true;
    } else if (a == "--converge") {
      opt.converge = true;
    } else if (a == "--cpu") {
      opt.prefer_cuda = false;
    } else if (a == "--float32") {
      opt.use_float32 = true;
    } else if (a == "--mm") {
      opt.display_mm = true;
    } else if (a == "--units-to-mm" && i + 1 < argc) {
      opt.units_to_mm = std::stod(argv[++i]);
    } else if (a == "--dump-rays") {
      opt.dump_rays = true;
      opt.keep_lengths = true;
    } else if (a == "--no-cache") {
      opt.cache_enabled = false;
    } else if (a == "--config" && i + 1 < argc) {
      if (!pler::load_options_yaml(argv[++i], opt)) {
        std::cerr << "Could not load config: " << argv[i] << "\n";
        return false;
      }
    } else if (a == "--w1" && i + 1 < argc) {
      opt.w_pler = std::stod(argv[++i]);
    } else if (a == "--w2" && i + 1 < argc) {
      opt.w_tsi = std::stod(argv[++i]);
    } else if (a == "--model" && i + 1 < argc && model_path) {
      *model_path = argv[++i];
    } else if (a == "-h" || a == "--help") {
      usage();
      return false;
    } else {
      std::cerr << "Unknown option: " << a << "\n";
      return false;
    }
  }
  if (opt.w_pler >= 0 || opt.w_tsi >= 0) opt.compute_tsi = true;

  auto mem = pler::query_memory();
  int rec = pler::recommended_max_rays(mem);
  if (opt.num_rays > rec) {
    std::cerr << "warning: num_rays=" << opt.num_rays
              << " exceeds recommended_max=" << rec << "\n";
  }
  return true;
}

static int run_selftest() {
  std::cout << "PLER selftest\n";
  pler::Mesh sphere;
  const int stacks = 16, sectors = 32;
  for (int i = 0; i <= stacks; i++) {
    double phi = 3.14159265358979323846 * i / stacks;
    for (int j = 0; j < sectors; j++) {
      double th = 2.0 * 3.14159265358979323846 * j / sectors;
      sphere.vertices.push_back({std::sin(phi) * std::cos(th),
                                 std::sin(phi) * std::sin(th), std::cos(phi)});
    }
  }
  for (int i = 0; i < stacks; i++) {
    for (int j = 0; j < sectors; j++) {
      int first = i * sectors + j;
      int second = first + sectors;
      int nj = (j + 1) % sectors;
      int first_n = i * sectors + nj;
      int second_n = second - j + nj;
      if (i != 0) sphere.triangles.push_back({first, second, first_n});
      if (i != stacks - 1) sphere.triangles.push_back({first_n, second, second_n});
    }
  }

  pler::Mesh sphere2 = sphere;
  auto frame = pler::normalize_pair(sphere, sphere2, 1.02);
  auto dirs = pler::fibonacci_directions(1000);
  auto rays = pler::make_inward_rays(dirs, frame.radius);
  auto be = pler::select_backend(true);
  auto th = pler::cast_rays(sphere, rays, be, nullptr);
  auto L = pler::hits_to_lengths(th, frame.radius);
  auto r = pler::score_pler(L, L, frame.radius, 1000);
  std::cout << "Identical sphere PLER: " << r.pler_db << " dB (expect ~100)\n";
  std::cout << "Backend: " << (be == pler::Backend::CUDA ? "cuda-bvh" : "cpu-bvh")
            << "\n";
  bool ok = r.pler_db >= 99.0;

  if (pler::cuda_available()) {
    double diff = pler::compare_backends(sphere, rays);
    std::cout << "CPU vs CUDA BVH max |dL|: " << diff << "\n";
    if (diff > 1e-2) {
      std::cout << "FAIL: backend disagreement\n";
      ok = false;
    } else {
      std::cout << "PASS: backends agree\n";
    }
  } else {
    std::cout << "CUDA not available; skipped GPU compare\n";
  }

  std::cout << (ok ? "SELFTEST PASS\n" : "SELFTEST FAIL\n");
  return ok ? 0 : 1;
}

static int run_gui() {
#ifdef _WIN32
  wchar_t path[MAX_PATH];
  GetModuleFileNameW(nullptr, path, MAX_PATH);
  fs::path p(path);
  fs::path gui = p.parent_path() / "pler_gui.exe";
  if (!fs::exists(gui)) {
    std::cerr << "pler_gui not found at " << gui.string() << "\n";
    return 1;
  }
  std::string cmd = "\"" + gui.string() + "\"";
  return std::system(cmd.c_str());
#else
  return std::system("./pler_gui");
#endif
}

int main(int argc, char** argv) {
  if (argc < 2) {
    usage();
    return 1;
  }
  std::string cmd = argv[1];
  if (cmd == "-h" || cmd == "--help") {
    usage();
    return 0;
  }
  if (cmd == "--version" || cmd == "-V") {
    print_version();
    return 0;
  }
  if (cmd == "selftest") return run_selftest();
  if (cmd == "gui") return run_gui();

  if (cmd == "report") {
    std::string cache = (argc >= 3) ? argv[2] : ".pler_cache";
    std::string csv = pler::default_research_csv_path(cache);
    if (!fs::exists(csv)) {
      std::cerr << "No research CSV at " << csv << "\n";
      return 1;
    }
    // Re-read is not implemented; point user at last report or regenerate empty
    std::cout << "Last research CSV: " << csv << "\n";
    std::cout << "Log:\n" << pler::read_log_tail(cache, 50);
    std::cout << "Reports under: " << cache << "/reports/\n";
    return 0;
  }

  if (cmd == "predict") {
    std::string model;
    pler::PlerOptions opt;
    pler::apply_default_config(opt);
    if (!parse_options(argc, argv, 2, opt, &model)) return 1;
    if (model.empty() || !fs::exists(model)) {
      std::cerr << "predict requires an existing --model PATH (ML stub; no "
                   "default model ships with PLER).\n";
      return 1;
    }
    std::cerr << "ML predictor stub: model found at " << model
              << " but inference is not wired in this build.\n";
    return 2;
  }

  if (cmd == "calibrate") {
    if (argc < 4) {
      usage();
      return 1;
    }
    pler::PlerOptions opt;
    pler::apply_default_config(opt);
    if (!parse_options(argc, argv, 4, opt)) return 1;
    auto cr = pler::calibrate_rays(
        argv[2], argv[3], opt,
        [](float p, const std::string& m) {
          std::cerr << "[" << int(p * 100) << "%] " << m << "\n";
        });
    if (!cr.ok) {
      std::cerr << "ERROR: " << cr.error << "\n";
      return 2;
    }
    std::cout << "Suggested rays: " << cr.suggested_rays << "\n";
    std::cout << "CSV: " << cr.csv_path << "\n";
    for (const auto& pt : cr.points) {
      std::cout << "  N=" << pt.num_rays << " PLER=" << pt.pler_db
                << " rel=" << pt.rel_change << " t=" << pt.time_s << "s\n";
    }
    return 0;
  }

  if (cmd == "batch") {
    if (argc < 4) {
      usage();
      return 1;
    }
    pler::PlerOptions opt;
    pler::apply_default_config(opt);
    if (!parse_options(argc, argv, 4, opt)) return 1;
    std::string ref = argv[2];
    std::string pattern = argv[3];
    if (pattern.find('*') != std::string::npos) {
      fs::path parent = fs::path(pattern).parent_path();
      if (parent.empty()) parent = ".";
      std::string filepat = fs::path(pattern).filename().string();
      auto star = filepat.find('*');
      std::string pre = filepat.substr(0, star);
      std::string suf = filepat.substr(star + 1);
      std::vector<std::string> files;
      for (auto& ent : fs::directory_iterator(parent)) {
        auto name = ent.path().filename().string();
        if (name.size() >= pre.size() + suf.size() &&
            name.compare(0, pre.size(), pre) == 0 &&
            name.compare(name.size() - suf.size(), suf.size(), suf) == 0) {
          files.push_back(ent.path().string());
        }
      }
      pler::natural_sort_paths(files);
      std::vector<std::pair<std::string, pler::PlerResult>> rows;
      int n_ok = 0;
      for (const auto& path : files) {
        auto name = fs::path(path).filename().string();
        std::cout << "=== " << name << " ===\n";
        auto r = pler::compute_pler(ref, path, opt,
                                    [](float p, const std::string& m) {
                                      std::cerr << "[" << int(p * 100) << "%] "
                                                << m << "\n";
                                    });
        print_result(r, opt.display_mm, opt.units_to_mm);
        rows.emplace_back(name, r);
        if (r.ok) n_ok++;
      }
      pler::write_research_csv(pler::default_research_csv_path(opt.cache_dir),
                               rows);
      auto rp = pler::write_research_report(opt.cache_dir, rows);
      std::cout << "Report: " << rp.dir << "\n";
      return n_ok > 0 ? 0 : 1;
    }
    std::cerr << "batch pattern must contain *\n";
    return 1;
  }

  if (argc < 3) {
    usage();
    return 1;
  }
  pler::PlerOptions opt;
  pler::apply_default_config(opt);
  if (!parse_options(argc, argv, 3, opt)) return 1;
  auto r = pler::compute_pler(argv[1], argv[2], opt,
                              [](float p, const std::string& m) {
                                std::cerr << "[" << int(p * 100) << "%] " << m
                                          << "\n";
                              });
  print_result(r, opt.display_mm, opt.units_to_mm);
  return r.ok ? 0 : 2;
}
