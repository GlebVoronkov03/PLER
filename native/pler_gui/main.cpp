#include "pler/pler.hpp"
#include "pler/calibrate.hpp"
#include "pler/config.hpp"
#include "pler/log.hpp"
#include "pler/report.hpp"
#include "pler/sysinfo.hpp"
#include "pler/util.hpp"
#include "pler/version.hpp"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#include <shlobj.h>
#include <shellapi.h>
#endif

#include <GLFW/glfw3.h>
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

namespace fs = std::filesystem;

static void glfw_error(int code, const char* desc) {
  std::fprintf(stderr, "GLFW %d: %s\n", code, desc);
}

#ifdef _WIN32
static bool browse_open_file(char* out, size_t out_sz, const char* filter) {
  char buf[MAX_PATH] = {};
  OPENFILENAMEA ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = buf;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = filter;
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;
  if (!GetOpenFileNameA(&ofn)) return false;
  std::snprintf(out, out_sz, "%s", buf);
  return true;
}

static bool browse_folder(char* out, size_t out_sz) {
  BROWSEINFOA bi{};
  bi.lpszTitle = "Select distorted models folder";
  bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
  LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
  if (!pidl) return false;
  char path[MAX_PATH] = {};
  BOOL ok = SHGetPathFromIDListA(pidl, path);
  CoTaskMemFree(pidl);
  if (!ok) return false;
  std::snprintf(out, out_sz, "%s", path);
  return true;
}

static bool browse_save_csv(char* out, size_t out_sz) {
  char buf[MAX_PATH] = "research.csv";
  OPENFILENAMEA ofn{};
  ofn.lStructSize = sizeof(ofn);
  ofn.lpstrFile = buf;
  ofn.nMaxFile = MAX_PATH;
  ofn.lpstrFilter = "CSV (*.csv)\0*.csv\0All\0*.*\0";
  ofn.nFilterIndex = 1;
  ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
  ofn.lpstrDefExt = "csv";
  if (!GetSaveFileNameA(&ofn)) return false;
  std::snprintf(out, out_sz, "%s", buf);
  return true;
}

static void open_folder(const std::string& path) {
  ShellExecuteA(nullptr, "open", path.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}
#else
static bool browse_open_file(char*, size_t, const char*) { return false; }
static bool browse_folder(char*, size_t) { return false; }
static bool browse_save_csv(char*, size_t) { return false; }
static void open_folder(const std::string&) {}
#endif

static const char* quality_label(double db) {
  if (db >= 60.0) return "Excellent";
  if (db >= 40.0) return "Good";
  if (db >= 20.0) return "Fair";
  return "Poor";
}

static ImVec4 quality_color(double db) {
  if (db >= 60.0) return ImVec4(0.35f, 0.85f, 0.45f, 1.f);
  if (db >= 40.0) return ImVec4(0.75f, 0.85f, 0.35f, 1.f);
  if (db >= 20.0) return ImVec4(0.95f, 0.7f, 0.3f, 1.f);
  return ImVec4(0.95f, 0.35f, 0.35f, 1.f);
}

static pler::PlerOptions make_options(int rays, bool auto_rays, int align_mode,
                                      bool tsi, bool converge, bool prefer_cuda,
                                      int min_rays, int max_rays, int voxel_res,
                                      bool display_mm, bool dump_rays,
                                      bool use_float32, bool cache_enabled,
                                      double units_to_mm,
                                      const pler::PlerOptions& base) {
  pler::PlerOptions opt = base;
  opt.num_rays = auto_rays ? 0 : rays;
  opt.min_rays = min_rays;
  opt.max_rays = max_rays;
  opt.align_mode = static_cast<pler::AlignMode>(align_mode);
  opt.align_volume = (opt.align_mode == pler::AlignMode::VolumeIoU ||
                      opt.align_mode == pler::AlignMode::IoUThenICP);
  opt.compute_tsi = tsi;
  opt.converge = converge;
  opt.prefer_cuda = prefer_cuda;
  opt.voxel_resolution = voxel_res;
  opt.display_mm = display_mm;
  opt.dump_rays = dump_rays;
  opt.use_float32 = use_float32;
  opt.cache_enabled = cache_enabled;
  opt.units_to_mm = units_to_mm;
  return opt;
}

static std::vector<std::string> list_meshes_in_folder(const char* folder) {
  std::vector<std::string> out;
  std::error_code ec;
  if (!fs::is_directory(folder, ec)) return out;
  for (auto& ent : fs::directory_iterator(folder, ec)) {
    if (!ent.is_regular_file()) continue;
    auto ext = ent.path().extension().string();
    for (char& c : ext)
      c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".obj" || ext == ".stl" || ext == ".ply" || ext == ".gltf" ||
        ext == ".glb" || ext == ".fbx" || ext == ".dae")
      out.push_back(ent.path().string());
  }
  pler::natural_sort_paths(out);
  return out;
}

static std::vector<int> parse_ray_list(const char* text) {
  std::vector<int> rays;
  std::stringstream ss(text);
  std::string tok;
  while (std::getline(ss, tok, ',')) {
    try {
      int v = std::stoi(tok);
      if (v > 0) rays.push_back(v);
    } catch (...) {
    }
  }
  if (rays.empty()) rays = {1000, 5000, 10000};
  return rays;
}

static void draw_run_button(const char* label, bool busy, bool* pressed) {
  *pressed = false;
  ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.45f, 0.85f, 1.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.55f, 0.95f, 1.f));
  ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.1f, 0.35f, 0.7f, 1.f));
  ImGui::BeginDisabled(busy);
  if (ImGui::Button(label, ImVec2(-1, 48))) *pressed = true;
  ImGui::EndDisabled();
  ImGui::PopStyleColor(3);
}

static void draw_length_histogram(const pler::PlerResult& r) {
  if (r.L_ref.empty() || r.L_test.empty()) return;
  const int bins = 48;
  std::vector<float> hist(static_cast<size_t>(bins), 0.f);
  double max_e = 0;
  for (size_t i = 0; i < r.L_ref.size(); i++)
    max_e = std::max(max_e, std::fabs(r.L_ref[i] - r.L_test[i]));
  if (max_e < 1e-12) max_e = 1e-12;
  for (size_t i = 0; i < r.L_ref.size(); i++) {
    double e = std::fabs(r.L_ref[i] - r.L_test[i]);
    int b = static_cast<int>((e / max_e) * (bins - 1));
    if (b < 0) b = 0;
    if (b >= bins) b = bins - 1;
    hist[static_cast<size_t>(b)] += 1.f;
  }
  if (ImGui::CollapsingHeader("Length error histogram", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PlotHistogram("##herr", hist.data(), bins, 0, nullptr, 0.f, FLT_MAX,
                         ImVec2(-1, 80));
    ImGui::TextDisabled("bin axis: |L_ref - L_test| 0 .. %.4g (normalized R)", max_e);
  }
}

static void draw_error_viewer(const pler::PlerResult& r, bool* open) {
  if (!*open || r.ray_dirs.empty() || r.L_ref.empty()) return;
  ImGui::SetNextWindowSize(ImVec2(420, 420), ImGuiCond_FirstUseEver);
  if (!ImGui::Begin("Error sphere view", open)) {
    ImGui::End();
    return;
  }
  ImDrawList* dl = ImGui::GetWindowDrawList();
  ImVec2 p0 = ImGui::GetCursorScreenPos();
  ImVec2 sz = ImGui::GetContentRegionAvail();
  float side = std::min(sz.x, sz.y);
  ImVec2 c{p0.x + side * 0.5f, p0.y + side * 0.5f};
  float rad = side * 0.42f;
  dl->AddCircle(c, rad, IM_COL32(80, 80, 90, 255), 64);

  size_t step = std::max<size_t>(1, r.ray_dirs.size() / 4000);
  double max_e = 1e-12;
  for (size_t i = 0; i < r.L_ref.size(); i += step)
    max_e = std::max(max_e, std::fabs(r.L_ref[i] - r.L_test[i]));

  for (size_t i = 0; i < r.ray_dirs.size() && i < r.L_ref.size(); i += step) {
    const auto& d = r.ray_dirs[i];
    float x = static_cast<float>(d.x);
    float y = static_cast<float>(d.y);
    float e = static_cast<float>(std::fabs(r.L_ref[i] - r.L_test[i]) / max_e);
    ImU32 col = IM_COL32(40 + int(e * 200), 40, 40 + int((1.f - e) * 180), 220);
    dl->AddCircleFilled(ImVec2(c.x + x * rad, c.y - y * rad), 2.f, col);
  }
  ImGui::Dummy(ImVec2(side, side));
  ImGui::TextDisabled("Orthographic XY of Fibonacci dirs; red = larger |dL|");
  ImGui::End();
}

static void draw_single_result(const pler::PlerResult& r, bool display_mm,
                               double units_to_mm, bool* open_view) {
  ImGui::Separator();
  ImGui::Text("PLER: %.4f dB", r.pler_db);
  ImGui::SameLine();
  ImGui::TextColored(quality_color(r.pler_db), "[%s]", quality_label(r.pler_db));

  double scale = 1.0;
  const char* unit = "(norm)";
  if (display_mm && r.sphere_radius_world > 0) {
    scale = r.sphere_radius_world * units_to_mm;
    unit = "mm";
  }
  ImGui::Text("MSE: %.6g   Peak: %.4f %s   Rays: %d", r.mse, r.peak * scale, unit,
              r.num_rays);
  ImGui::Text("Mean/max err: %.4g / %.4g %s", r.mean_error * scale,
              r.max_error * scale, unit);
  ImGui::Text("Miss ref/test: %.2f%% / %.2f%%", r.miss_rate_ref * 100.0,
              r.miss_rate_test * 100.0);
  ImGui::Text("Backend: %s   Time: %.3f s   Align: %s", r.backend.c_str(),
              r.computation_time_s, r.align_mode_used.c_str());
  ImGui::Text("Verts ref/test: %zu / %zu", r.vertex_count_ref, r.vertex_count_test);
  if (r.align_volume_used) ImGui::Text("Volume IoU: %.4f", r.volume_iou);
  if (r.tsi >= 0.0) ImGui::Text("TSI: %.4f", r.tsi);
  if (r.mse <= 1e-10)
    ImGui::TextDisabled("MSE is ~0: geometry matches within the ray sample.");
  draw_length_histogram(r);
  if (!r.ray_dirs.empty() && ImGui::Button("Open 3D error view")) *open_view = true;
}

/** Map value in [vmin,vmax] to pixel; log_x uses log10 of positive values. */
static float map_axis(double v, double vmin, double vmax, float p0, float p1,
                      bool log_scale) {
  double t;
  if (log_scale) {
    double a = std::log10(std::max(1e-12, vmin));
    double b = std::log10(std::max(1e-12, vmax));
    t = (std::log10(std::max(1e-12, v)) - a) / (b - a + 1e-15);
  } else {
    t = (v - vmin) / (vmax - vmin + 1e-15);
  }
  if (t < 0) t = 0;
  if (t > 1) t = 1;
  return p0 + static_cast<float>(t) * (p1 - p0);
}

static void draw_axes_frame(ImDrawList* dl, ImVec2 origin, float w, float h,
                            const char* title) {
  ImU32 axis = IM_COL32(120, 120, 130, 255);
  dl->AddRect(origin, ImVec2(origin.x + w, origin.y + h), axis);
  dl->AddText(ImVec2(origin.x + 4, origin.y - 18), IM_COL32(200, 200, 210, 255),
              title);
}

static void draw_xy_scatter(const char* title, const std::vector<double>& xs,
                            const std::vector<double>& ys, bool log_x,
                            const char* xlab, const char* ylab,
                            float height = 160.f,
                            double mark_x = -1.0) {
  if (xs.empty() || xs.size() != ys.size()) {
    ImGui::TextDisabled("%s: no data", title);
    return;
  }
  ImGui::TextUnformatted(title);
  ImVec2 p0 = ImGui::GetCursorScreenPos();
  float w = ImGui::GetContentRegionAvail().x;
  if (w < 120.f) w = 120.f;
  float h = height;
  const float pad_l = 44.f, pad_r = 12.f, pad_t = 8.f, pad_b = 28.f;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  draw_axes_frame(dl, p0, w, h, "");

  double xmin = *std::min_element(xs.begin(), xs.end());
  double xmax = *std::max_element(xs.begin(), xs.end());
  double ymin = *std::min_element(ys.begin(), ys.end());
  double ymax = *std::max_element(ys.begin(), ys.end());
  if (xmax <= xmin) xmax = xmin + 1;
  if (ymax <= ymin) ymax = ymin + 1;
  // pad y a little
  double ypad = (ymax - ymin) * 0.05;
  ymin -= ypad;
  ymax += ypad;

  float x0 = p0.x + pad_l, x1 = p0.x + w - pad_r;
  float y0 = p0.y + h - pad_b, y1 = p0.y + pad_t;

  if (mark_x > 0) {
    float mx = map_axis(mark_x, xmin, xmax, x0, x1, log_x);
    dl->AddLine(ImVec2(mx, y1), ImVec2(mx, y0), IM_COL32(80, 180, 100, 200), 2.f);
  }

  // polyline in index order when x is monotonic
  for (size_t i = 1; i < xs.size(); i++) {
    float xa = map_axis(xs[i - 1], xmin, xmax, x0, x1, log_x);
    float ya = map_axis(ys[i - 1], ymin, ymax, y0, y1, false);
    float xb = map_axis(xs[i], xmin, xmax, x0, x1, log_x);
    float yb = map_axis(ys[i], ymin, ymax, y0, y1, false);
    dl->AddLine(ImVec2(xa, ya), ImVec2(xb, yb), IM_COL32(70, 140, 220, 180), 1.5f);
  }
  for (size_t i = 0; i < xs.size(); i++) {
    float px = map_axis(xs[i], xmin, xmax, x0, x1, log_x);
    float py = map_axis(ys[i], ymin, ymax, y0, y1, false);
    dl->AddCircleFilled(ImVec2(px, py), 3.f, IM_COL32(220, 120, 60, 230));
  }

  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.3g", ymin);
  dl->AddText(ImVec2(p0.x + 2, y0 - 6), IM_COL32(150, 150, 160, 255), buf);
  std::snprintf(buf, sizeof(buf), "%.3g", ymax);
  dl->AddText(ImVec2(p0.x + 2, y1), IM_COL32(150, 150, 160, 255), buf);
  dl->AddText(ImVec2(x0, p0.y + h - 16), IM_COL32(150, 150, 160, 255), xlab);
  dl->AddText(ImVec2(p0.x + 2, p0.y + 2), IM_COL32(150, 150, 160, 255), ylab);

  ImGui::Dummy(ImVec2(w, h + 4));
}

static void draw_pler_bars(const std::vector<std::string>& labels,
                           const std::vector<double>& values) {
  if (labels.empty() || labels.size() != values.size()) {
    ImGui::TextDisabled("PLER by model: no data");
    return;
  }
  ImGui::TextUnformatted("PLER by model (dB)");
  ImVec2 p0 = ImGui::GetCursorScreenPos();
  float w = ImGui::GetContentRegionAvail().x;
  if (w < 120.f) w = 120.f;
  float h = 160.f;
  const float pad_l = 44.f, pad_r = 8.f, pad_t = 8.f, pad_b = 36.f;
  ImDrawList* dl = ImGui::GetWindowDrawList();
  draw_axes_frame(dl, p0, w, h, "");

  double ymax = *std::max_element(values.begin(), values.end());
  if (ymax < 1) ymax = 1;
  ymax *= 1.05;
  float x0 = p0.x + pad_l, x1 = p0.x + w - pad_r;
  float y0 = p0.y + h - pad_b, y1 = p0.y + pad_t;
  float bar_w = (x1 - x0) / static_cast<float>(values.size());
  float gap = bar_w * 0.15f;

  for (size_t i = 0; i < values.size(); i++) {
    float bx = x0 + static_cast<float>(i) * bar_w + gap * 0.5f;
    float bw = std::max(2.f, bar_w - gap);
    float top = map_axis(values[i], 0, ymax, y0, y1, false);
    dl->AddRectFilled(ImVec2(bx, top), ImVec2(bx + bw, y0),
                      IM_COL32(60, 130, 210, 220));
    // short label
    std::string lab = labels[i];
    if (lab.size() > 10) lab = lab.substr(0, 9) + "…";
    if (values.size() <= 20 || i % std::max<size_t>(1, values.size() / 12) == 0) {
      dl->AddText(ImVec2(bx, y0 + 2), IM_COL32(150, 150, 160, 255), lab.c_str());
    }
  }
  char buf[32];
  std::snprintf(buf, sizeof(buf), "%.0f", ymax);
  dl->AddText(ImVec2(p0.x + 2, y1), IM_COL32(150, 150, 160, 255), buf);
  dl->AddText(ImVec2(p0.x + 2, y0 - 6), IM_COL32(150, 150, 160, 255), "0");
  ImGui::Dummy(ImVec2(w, h + 4));
}

static void short_model_label(const std::string& full, std::string& out) {
  auto slash = full.find_last_of("/\\");
  std::string base = (slash == std::string::npos) ? full : full.substr(slash + 1);
  // strip " [rays]" suffix from research labels
  auto bracket = base.find(" [");
  if (bracket != std::string::npos) base = base.substr(0, bracket);
  out = base;
}

static void draw_research_charts(
    const std::vector<std::pair<std::string, pler::PlerResult>>& batch,
    const pler::CalibrateResult* cal) {
  if (batch.empty() && !(cal && cal->ok && !cal->points.empty())) return;

  if (!ImGui::CollapsingHeader("Charts", ImGuiTreeNodeFlags_DefaultOpen)) return;

  if (ImGui::BeginTabBar("research_charts")) {
    if (!batch.empty() && ImGui::BeginTabItem("PLER by model")) {
      std::vector<std::string> labels;
      std::vector<double> vals;
      // Prefer one point per model: if multi-ray, use last ok row per basename
      std::vector<std::pair<std::string, double>> ordered;
      for (const auto& row : batch) {
        if (!row.second.ok) continue;
        std::string lab;
        short_model_label(row.first, lab);
        ordered.push_back({lab, row.second.pler_db});
      }
      // If multi-ray duplicates, keep all in order (research natural order)
      for (const auto& p : ordered) {
        labels.push_back(p.first);
        vals.push_back(p.second);
      }
      draw_pler_bars(labels, vals);
      ImGui::TextDisabled("Batch order (natural sort). Multi-N runs show one bar per row.");
      ImGui::EndTabItem();
    }

    if (!batch.empty() && ImGui::BeginTabItem("PLER vs verts")) {
      std::vector<double> xs, ys;
      for (const auto& row : batch) {
        if (!row.second.ok || row.second.vertex_count_test == 0) continue;
        xs.push_back(static_cast<double>(row.second.vertex_count_test));
        ys.push_back(row.second.pler_db);
      }
      draw_xy_scatter("PLER vs vertex count", xs, ys, true, "verts (log)", "PLER dB");
      ImGui::EndTabItem();
    }

    if (!batch.empty() && ImGui::BeginTabItem("Time vs verts")) {
      std::vector<double> xs, ys;
      for (const auto& row : batch) {
        if (!row.second.ok || row.second.vertex_count_test == 0) continue;
        xs.push_back(static_cast<double>(row.second.vertex_count_test));
        ys.push_back(row.second.computation_time_s);
      }
      draw_xy_scatter("Time vs vertex count", xs, ys, true, "verts (log)", "time s");
      ImGui::EndTabItem();
    }

    if (!batch.empty() && ImGui::BeginTabItem("PLER vs rays")) {
      std::vector<double> xs, ys;
      for (const auto& row : batch) {
        if (!row.second.ok) continue;
        xs.push_back(static_cast<double>(row.second.num_rays));
        ys.push_back(row.second.pler_db);
      }
      if (xs.size() < 2) {
        ImGui::TextDisabled(
            "Need multiple ray counts in the batch (comma list) for a curve.");
      } else {
        draw_xy_scatter("PLER vs rays", xs, ys, false, "rays", "PLER dB");
      }
      ImGui::EndTabItem();
    }

    if (!batch.empty() && ImGui::BeginTabItem("MSE vs rays")) {
      std::vector<double> xs, ys;
      for (const auto& row : batch) {
        if (!row.second.ok) continue;
        xs.push_back(static_cast<double>(row.second.num_rays));
        ys.push_back(row.second.mse);
      }
      if (xs.size() < 2) {
        ImGui::TextDisabled(
            "Need multiple ray counts in the batch for a curve.");
      } else {
        draw_xy_scatter("MSE vs rays", xs, ys, false, "rays", "MSE");
      }
      ImGui::EndTabItem();
    }

    if (cal && cal->ok && !cal->points.empty() &&
        ImGui::BeginTabItem("Calibrate")) {
      std::vector<double> xs, ys_pler, ys_t;
      for (const auto& pt : cal->points) {
        xs.push_back(static_cast<double>(pt.num_rays));
        ys_pler.push_back(pt.pler_db);
        ys_t.push_back(pt.time_s);
      }
      draw_xy_scatter("PLER vs rays (calibrate)", xs, ys_pler, true, "rays (log)",
                      "PLER dB", 140.f,
                      static_cast<double>(cal->suggested_rays));
      ImGui::Text("Suggested N = %d (green line)", cal->suggested_rays);
      draw_xy_scatter("Time vs rays (calibrate)", xs, ys_t, true, "rays (log)",
                      "time s", 120.f,
                      static_cast<double>(cal->suggested_rays));
      ImGui::EndTabItem();
    }

    ImGui::EndTabBar();
  }
}

int main() {
  glfwSetErrorCallback(glfw_error);
  if (!glfwInit()) return 1;

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  GLFWwindow* window =
      glfwCreateWindow(1180, 760, "PLER " PLER_VERSION_STRING, nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::StyleColorsDark();
  ImGui::GetStyle().FrameRounding = 4.f;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 330");

  pler::PlerOptions defaults;
  pler::apply_default_config(defaults);

  char ref_path[512] = "models/simple_reference.obj";
  char test_path[512] = "models/simple_distorted.obj";
  char batch_folder[512] = "models";
  char ray_list[128] = "1000,5000,10000";

  int rays = defaults.num_rays > 0 ? defaults.num_rays : 5000;
  int min_rays = defaults.min_rays;
  int max_rays = defaults.max_rays;
  int voxel_res = defaults.voxel_resolution;
  bool auto_rays = defaults.num_rays <= 0;
  int align_mode = static_cast<int>(pler::resolve_align_mode(defaults));
  bool tsi = defaults.compute_tsi;
  bool converge = defaults.converge;
  bool prefer_cuda = defaults.prefer_cuda;
  bool display_mm = defaults.display_mm;
  bool dump_rays = defaults.dump_rays;
  bool use_float32 = defaults.use_float32;
  bool cache_enabled = defaults.cache_enabled;
  double units_to_mm = defaults.units_to_mm;
  int theme = defaults.theme;

  auto mem = pler::query_memory();
  int rec_max = pler::recommended_max_rays(mem);

  int tab = 0;
  pler::JobRunner runner;
  pler::PlerResult last_single{};
  bool have_single = false;
  bool open_view = false;
  std::string status_hint;
  std::string last_report;
  std::vector<std::pair<std::string, pler::PlerResult>> last_batch;
  pler::CalibrateResult last_calibrate{};
  bool have_calibrate = false;
  std::string last_applied_cal_csv;
  std::string config_used = pler::find_default_config_path();
  std::string log_path = pler::default_log_path(defaults.cache_dir);
  std::string log_tail;

  const char* mesh_filter =
      "Meshes\0*.obj;*.stl;*.ply;*.gltf;*.glb;*.fbx;*.dae\0All\0*.*\0";
  const char* align_items = "Off\0Volume IoU\0ICP\0IoU then ICP\0";

  while (!glfwWindowShouldClose(window)) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (theme == 1)
      ImGui::StyleColorsLight();
    else
      ImGui::StyleColorsDark();

    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);
    ImGui::Begin("PLER", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar);

    ImGui::Text("PLER %s - geometric fidelity", PLER_VERSION_STRING);
    ImGui::SameLine();
    ImGui::TextDisabled("  CUDA BVH / worker thread");
    ImGui::Separator();

    if (ImGui::BeginTabBar("modes")) {
      if (ImGui::BeginTabItem("Single")) {
        tab = 0;
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Research")) {
        tab = 1;
        ImGui::EndTabItem();
      }
      if (ImGui::BeginTabItem("Settings")) {
        tab = 2;
        ImGui::EndTabItem();
      }
      ImGui::EndTabBar();
    }

    pler::JobStatus st = runner.snapshot();
    bool busy =
        st.state == pler::JobState::Running || st.state == pler::JobState::Queued;
    if (st.kind == pler::JobKind::Single && st.state == pler::JobState::Done &&
        st.result.ok) {
      last_single = st.result;
      have_single = true;
    }
    if (st.kind == pler::JobKind::Research && st.state == pler::JobState::Done &&
        !st.last_report_dir.empty())
      last_report = st.last_report_dir;
    if (st.kind == pler::JobKind::Research && st.state == pler::JobState::Done &&
        !st.batch.empty())
      last_batch = st.batch;
    if (st.kind == pler::JobKind::Calibrate && st.state == pler::JobState::Done &&
        st.calibrate.ok) {
      last_calibrate = st.calibrate;
      have_calibrate = true;
      if (st.calibrate.csv_path != last_applied_cal_csv) {
        last_applied_cal_csv = st.calibrate.csv_path;
        rays = st.calibrate.suggested_rays;
        auto_rays = false;
        status_hint = "Applied suggested rays=" + std::to_string(rays);
      }
    }

    auto build_opt = [&](bool auto_r, bool conv) {
      return make_options(rays, auto_r, align_mode, tsi, conv, prefer_cuda,
                          min_rays, max_rays, voxel_res, display_mm, dump_rays,
                          use_float32, cache_enabled, units_to_mm, defaults);
    };

    if (tab == 0) {
      ImGui::Spacing();
      ImGui::InputText("Reference", ref_path, sizeof(ref_path));
      ImGui::SameLine();
      if (ImGui::Button("Browse##ref"))
        browse_open_file(ref_path, sizeof(ref_path), mesh_filter);

      ImGui::InputText("Test", test_path, sizeof(test_path));
      ImGui::SameLine();
      if (ImGui::Button("Browse##test"))
        browse_open_file(test_path, sizeof(test_path), mesh_filter);

      ImGui::Checkbox("Auto rays (mesh complexity)", &auto_rays);
      ImGui::BeginDisabled(auto_rays);
      ImGui::InputInt("Rays", &rays);
      if (rays < 100) rays = 100;
      ImGui::EndDisabled();
      if (rays > rec_max)
        ImGui::TextColored(ImVec4(0.95f, 0.7f, 0.3f, 1),
                           "Above recommended max rays (%d) for free memory",
                           rec_max);
      if (converge) {
        ImGui::TextDisabled(
            "Converge is on: ray count may double until PLER changes by <5%%.");
      }

      ImGui::Combo("Align", &align_mode, align_items);
      ImGui::SameLine();
      ImGui::Checkbox("TSI", &tsi);
      ImGui::SameLine();
      ImGui::Checkbox("Converge", &converge);
      ImGui::SameLine();
      ImGui::Checkbox("Prefer CUDA", &prefer_cuda);
      ImGui::Checkbox("Display mm", &display_mm);
      ImGui::SameLine();
      ImGui::Checkbox("Dump rays", &dump_rays);

      ImGui::Spacing();
      ImGui::Separator();
      ImGui::Spacing();

      bool run_pressed = false;
      draw_run_button("RUN", busy, &run_pressed);
      if (run_pressed) {
        if (rays > rec_max && !auto_rays)
          status_hint = "Warning: rays exceed recommended; running anyway";
        runner.start(ref_path, test_path, build_opt(auto_rays, converge));
        if (status_hint.empty()) status_hint = "Single measurement started";
      }
      if (busy && st.kind == pler::JobKind::Single &&
          ImGui::Button("Cancel", ImVec2(-1, 0)))
        runner.cancel();

      float bar = 0.f;
      const char* overlay = "Configure, then RUN";
      if (busy && st.kind == pler::JobKind::Single) {
        bar = st.progress;
        overlay = st.message.c_str();
      } else if (have_single) {
        bar = 0.f;
        overlay = "Idle";
      }
      ImGui::ProgressBar(bar, ImVec2(-1, 0), overlay);

      if (st.kind == pler::JobKind::Single && st.state == pler::JobState::Error) {
        ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "Error: %s",
                           st.message.c_str());
      } else if (st.kind == pler::JobKind::Single &&
                 st.state == pler::JobState::Cancelled) {
        ImGui::Text("Cancelled");
      } else if (have_single && (!busy || st.kind != pler::JobKind::Single)) {
        draw_single_result(last_single, display_mm, units_to_mm, &open_view);
      }
    } else if (tab == 1) {
      ImGui::Spacing();
      ImGui::InputText("Reference", ref_path, sizeof(ref_path));
      ImGui::SameLine();
      if (ImGui::Button("Browse##ref2"))
        browse_open_file(ref_path, sizeof(ref_path), mesh_filter);

      ImGui::InputText("Distorted folder", batch_folder, sizeof(batch_folder));
      ImGui::SameLine();
      if (ImGui::Button("Browse##folder"))
        browse_folder(batch_folder, sizeof(batch_folder));

      ImGui::InputText("Ray list (comma)", ray_list, sizeof(ray_list));

      ImGui::Combo("Align##b", &align_mode, align_items);
      ImGui::SameLine();
      ImGui::Checkbox("TSI##b", &tsi);
      ImGui::SameLine();
      ImGui::Checkbox("Prefer CUDA##b", &prefer_cuda);

      ImGui::Spacing();
      bool run_batch = false;
      draw_run_button("RUN BATCH", busy, &run_batch);
      if (run_batch) {
        auto models = list_meshes_in_folder(batch_folder);
        if (models.empty()) {
          status_hint = "No mesh files in folder";
        } else {
          auto ray_counts = parse_ray_list(ray_list);
          auto opt = build_opt(false, false);
          opt.num_rays = ray_counts.front();
          runner.start_research(ref_path, models, ray_counts, opt);
          status_hint = std::to_string(models.size()) + " models x " +
                        std::to_string(ray_counts.size()) + " ray counts";
        }
      }

      bool cal = false;
      ImGui::BeginDisabled(busy);
      if (ImGui::Button("Calibrate rays", ImVec2(-1, 32))) cal = true;
      ImGui::EndDisabled();
      if (cal) {
        auto models = list_meshes_in_folder(batch_folder);
        std::string test = test_path;
        if (!models.empty()) test = models.front();
        runner.start_calibrate(ref_path, test, build_opt(false, false));
        status_hint = "Calibrate started on " + test;
      }

      if (busy &&
          (st.kind == pler::JobKind::Research ||
           st.kind == pler::JobKind::Calibrate) &&
          ImGui::Button("Cancel##batch", ImVec2(-1, 0)))
        runner.cancel();

      float bar = (st.kind == pler::JobKind::Research ||
                   st.kind == pler::JobKind::Calibrate)
                      ? st.progress
                      : 0.f;
      const char* overlay =
          (busy && (st.kind == pler::JobKind::Research ||
                    st.kind == pler::JobKind::Calibrate))
              ? st.message.c_str()
              : "Idle";
      ImGui::ProgressBar(bar, ImVec2(-1, 0), overlay);
      if (!status_hint.empty()) ImGui::TextDisabled("%s", status_hint.c_str());

      if (!last_report.empty() && ImGui::Button("Open report folder"))
        open_folder(last_report);

      if (have_calibrate && last_calibrate.ok) {
        ImGui::Text("Suggested rays: %d  CSV: %s", last_calibrate.suggested_rays,
                    last_calibrate.csv_path.c_str());
      }

      // Live batch while research runs; otherwise last finished batch
      const auto& chart_batch =
          (!st.batch.empty()) ? st.batch : last_batch;
      const pler::CalibrateResult* chart_cal =
          have_calibrate ? &last_calibrate : nullptr;
      if ((st.kind == pler::JobKind::Calibrate && st.calibrate.ok))
        chart_cal = &st.calibrate;
      draw_research_charts(chart_batch, chart_cal);

      if (!chart_batch.empty()) {
        if (ImGui::Button("Export CSV")) {
          char save[512] = {};
          if (browse_save_csv(save, sizeof(save))) {
            if (pler::write_research_csv(save, chart_batch))
              status_hint = std::string("Saved ") + save;
            else
              status_hint = "Failed to write CSV";
          }
        }
        ImGui::SameLine();
        ImGui::TextDisabled("Also: %s",
                            pler::default_research_csv_path(defaults.cache_dir)
                                .c_str());

        if (ImGui::BeginTable("batch", 10,
                              ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                  ImGuiTableFlags_ScrollY |
                                  ImGuiTableFlags_Resizable,
                              ImVec2(0, 200))) {
          ImGui::TableSetupColumn("Model");
          ImGui::TableSetupColumn("PLER dB");
          ImGui::TableSetupColumn("MSE");
          ImGui::TableSetupColumn("Miss %");
          ImGui::TableSetupColumn("Rays");
          ImGui::TableSetupColumn("Verts");
          ImGui::TableSetupColumn("Backend");
          ImGui::TableSetupColumn("TSI");
          ImGui::TableSetupColumn("Time s");
          ImGui::TableSetupColumn("Status");
          ImGui::TableHeadersRow();
          for (const auto& row : chart_batch) {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(row.first.c_str());
            ImGui::TableSetColumnIndex(1);
            if (row.second.ok)
              ImGui::Text("%.3f", row.second.pler_db);
            else
              ImGui::TextUnformatted("-");
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%.4g", row.second.mse);
            ImGui::TableSetColumnIndex(3);
            ImGui::Text("%.2f/%.2f", row.second.miss_rate_ref * 100.0,
                        row.second.miss_rate_test * 100.0);
            ImGui::TableSetColumnIndex(4);
            ImGui::Text("%d", row.second.num_rays);
            ImGui::TableSetColumnIndex(5);
            ImGui::Text("%zu", row.second.vertex_count_test);
            ImGui::TableSetColumnIndex(6);
            ImGui::TextUnformatted(row.second.backend.c_str());
            ImGui::TableSetColumnIndex(7);
            if (row.second.tsi >= 0)
              ImGui::Text("%.3f", row.second.tsi);
            else
              ImGui::TextUnformatted("-");
            ImGui::TableSetColumnIndex(8);
            ImGui::Text("%.3f", row.second.computation_time_s);
            ImGui::TableSetColumnIndex(9);
            if (row.second.ok)
              ImGui::TextUnformatted("ok");
            else
              ImGui::TextWrapped("%s", row.second.error.c_str());
          }
          ImGui::EndTable();
        }
      }
    } else {
      ImGui::Spacing();
      ImGui::InputInt("Min rays (adaptive floor)", &min_rays);
      ImGui::InputInt("Max rays (adaptive / converge)", &max_rays);
      if (min_rays < 100) min_rays = 100;
      if (max_rays < min_rays) max_rays = min_rays;
      ImGui::Text("Recommended max rays (RAM/VRAM): %d", rec_max);
      if (ImGui::Button("Refresh memory hint")) {
        mem = pler::query_memory();
        rec_max = pler::recommended_max_rays(mem);
      }
      ImGui::InputInt("Voxel resolution (align)", &voxel_res);
      if (voxel_res < 8) voxel_res = 8;
      if (voxel_res > 128) voxel_res = 128;
      ImGui::Checkbox("Cache Fibonacci dirs", &cache_enabled);
      ImGui::Checkbox("CPU float32 precision", &use_float32);
      ImGui::InputDouble("units_to_mm", &units_to_mm);
      ImGui::Combo("Theme", &theme, "Dark\0Light\0");

      ImGui::Separator();
      ImGui::TextWrapped("Config file: %s",
                         config_used.empty() ? "(none found)" : config_used.c_str());
      ImGui::TextWrapped("Run log: %s", log_path.c_str());
      ImGui::TextWrapped(
          "CUDA path uses a GPU BVH (same as CPU). On OOM, falls back to CPU BVH.");
      if (ImGui::Button("Refresh log"))
        log_tail = pler::read_log_tail(defaults.cache_dir, 200);
      if (log_tail.empty()) log_tail = pler::read_log_tail(defaults.cache_dir, 200);
      ImGui::BeginChild("logpane", ImVec2(0, 220), true);
      ImGui::TextUnformatted(log_tail.c_str());
      ImGui::EndChild();
    }

    ImGui::End();
    draw_error_viewer(last_single, &open_view);

    ImGui::Render();
    int dw, dh;
    glfwGetFramebufferSize(window, &dw, &dh);
    glViewport(0, 0, dw, dh);
    ImVec4 clear = (theme == 1) ? ImVec4(0.92f, 0.92f, 0.94f, 1.f)
                                : ImVec4(0.08f, 0.08f, 0.1f, 1.f);
    glClearColor(clear.x, clear.y, clear.z, clear.w);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    std::this_thread::sleep_for(std::chrono::milliseconds(busy ? 16 : 33));
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
