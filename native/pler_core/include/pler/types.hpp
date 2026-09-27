#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace pler {

struct Vec3 {
  double x = 0, y = 0, z = 0;

  Vec3() = default;
  Vec3(double x_, double y_, double z_) : x(x_), y(y_), z(z_) {}

  Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
  Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
  Vec3 operator*(double s) const { return {x * s, y * s, z * s}; }
  Vec3 operator/(double s) const { return {x / s, y / s, z / s}; }
  Vec3& operator+=(const Vec3& o) {
    x += o.x;
    y += o.y;
    z += o.z;
    return *this;
  }

  double dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
  Vec3 cross(const Vec3& o) const {
    return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
  }
  double norm() const { return std::sqrt(dot(*this)); }
  Vec3 normalized() const {
    double n = norm();
    if (n <= 1e-15) return {0, 0, 0};
    return (*this) / n;
  }
};

inline Vec3 operator*(double s, const Vec3& v) { return v * s; }

/** Row-major 4x4 rigid+scale transform: p' = R*p*s + t */
struct Transform {
  Vec3 ex{1, 0, 0}, ey{0, 1, 0}, ez{0, 0, 1};
  Vec3 t{0, 0, 0};
  double s = 1.0;

  Vec3 apply(const Vec3& p) const {
    Vec3 q = ex * (p.x * s) + ey * (p.y * s) + ez * (p.z * s);
    return q + t;
  }

  static Transform identity() { return {}; }

  static Transform from_center_scale(const Vec3& center, double inv_scale) {
    Transform T;
    T.t = center * (-inv_scale);
    T.s = inv_scale;
    return T;
  }
};

struct Mesh {
  std::vector<Vec3> vertices;
  std::vector<std::array<int, 3>> triangles;
  std::string source_path;
  /** Optional vertex colors (RGB 0..1), unused by PLER score (chip 20). */
  std::vector<Vec3> colors;
  /** Optional UVs, unused by PLER score. */
  std::vector<std::array<float, 2>> uvs;

  bool empty() const { return vertices.empty() || triangles.empty(); }
  std::size_t vertex_count() const { return vertices.size(); }
  std::size_t triangle_count() const { return triangles.size(); }

  void apply(const Transform& T) {
    for (auto& v : vertices) v = T.apply(v);
  }
};

struct SphereFrame {
  Vec3 center{0, 0, 0};
  double radius = 1.0;  // R after normalization (usually 1)
};

enum class AlignMode { Off = 0, VolumeIoU = 1, ICP = 2, IoUThenICP = 3 };

struct PlerOptions {
  int num_rays = 5000;  // <= 0 => adaptive from mesh complexity
  int min_rays = 1000;
  int max_rays = 20000;
  AlignMode align_mode = AlignMode::Off;
  bool align_volume = false;  // legacy; synced from align_mode when set
  bool compute_tsi = false;
  bool converge = false;
  double converge_rel = 0.05;  // 5%
  int voxel_resolution = 48;
  double sphere_margin = 1.02;
  bool prefer_cuda = true;
  double w_pler = -1.0;  // <0 means do not mix
  double w_tsi = -1.0;
  std::string cache_dir = ".pler_cache";
  bool cache_enabled = true;
  bool use_float32 = false;   // CPU cast in float32 precision
  bool display_mm = false;    // display-only scale
  double units_to_mm = 1.0;   // multiply world units by this for mm display
  bool dump_rays = false;
  bool keep_lengths = false;  // fill L_ref/L_test in result
  int theme = 0;              // 0 dark, 1 light (GUI)
  int icp_iterations = 30;
};

using ProgressFn = std::function<void(float, const std::string&)>;

struct TopologyFeatures {
  int connected_components = 0;
  int boundary_loops = 0;
  int genus_estimate = 0;
  int euler = 0;
};

struct PlerResult {
  double pler_db = 0;
  double mse = 0;
  double mean_error = 0;
  double max_error = 0;
  double miss_rate_ref = 0;
  double miss_rate_test = 0;
  double peak = 0;
  int num_rays = 0;
  double computation_time_s = 0;
  bool align_volume_used = false;
  double volume_iou = -1.0;
  std::string backend = "cpu";
  TopologyFeatures topo_ref{};
  TopologyFeatures topo_test{};
  double tsi = -1.0;
  double combined = -1.0;
  std::string error;
  bool ok = true;
  // Sidecar (optional)
  std::vector<double> L_ref;
  std::vector<double> L_test;
  std::vector<Vec3> ray_dirs;
  std::size_t vertex_count_ref = 0;
  std::size_t vertex_count_test = 0;
  double sphere_radius_world = 0;  // pre-normalize R for mm display
  std::string align_mode_used = "off";
};

class PlerError : public std::runtime_error {
 public:
  explicit PlerError(const std::string& msg) : std::runtime_error(msg) {}
};

inline AlignMode resolve_align_mode(const PlerOptions& opt) {
  if (opt.align_mode != AlignMode::Off) return opt.align_mode;
  if (opt.align_volume) return AlignMode::VolumeIoU;
  return AlignMode::Off;
}

inline const char* align_mode_name(AlignMode m) {
  switch (m) {
    case AlignMode::VolumeIoU:
      return "volume_iou";
    case AlignMode::ICP:
      return "icp";
    case AlignMode::IoUThenICP:
      return "iou+icp";
    default:
      return "off";
  }
}

}  // namespace pler
