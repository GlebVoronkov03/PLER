#include "pler/frame.hpp"

#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pler {
namespace {

Vec3 mesh_centroid(const Mesh& m) {
  Vec3 c;
  for (const auto& v : m.vertices) c += v;
  return c * (1.0 / static_cast<double>(m.vertices.size()));
}

double max_radius_from(const Mesh& m, const Vec3& center) {
  double r = 0;
  for (const auto& v : m.vertices) {
    r = std::max(r, (v - center).norm());
  }
  return r;
}

}  // namespace

SphereFrame make_shared_sphere(const Mesh& ref, const Mesh& test, double margin) {
  Vec3 center = mesh_centroid(ref);
  double r = std::max(max_radius_from(ref, center), max_radius_from(test, center));
  if (r <= 1e-15) throw PlerError("Degenerate mesh extent");
  return {center, r * margin};
}

SphereFrame normalize_pair(Mesh& ref, Mesh& test, double margin) {
  SphereFrame frame = make_shared_sphere(ref, test, margin);
  const double inv = 1.0 / frame.radius;
  for (auto& v : ref.vertices) v = (v - frame.center) * inv;
  for (auto& v : test.vertices) v = (v - frame.center) * inv;
  frame.center = {0, 0, 0};
  frame.radius = 1.0;
  return frame;
}

std::vector<Vec3> fibonacci_directions(int n) {
  if (n <= 0) throw PlerError("num_rays must be positive");
  std::vector<Vec3> dirs(static_cast<size_t>(n));
  const double golden = M_PI * (1.0 + std::sqrt(5.0));
  // Equal-area: z = 1 - (2i+1)/N , azimuth golden * (i+0.5)
  for (int i = 0; i < n; i++) {
    double zi = 1.0 - (2.0 * i + 1.0) / static_cast<double>(n);
    double phi = golden * (i + 0.5);
    double r = std::sqrt(std::max(0.0, 1.0 - zi * zi));
    dirs[static_cast<size_t>(i)] = {r * std::cos(phi), r * std::sin(phi), zi};
  }
  return dirs;
}

RayBatch make_inward_rays(const std::vector<Vec3>& dirs, double R) {
  RayBatch batch;
  batch.origins.resize(dirs.size());
  batch.directions.resize(dirs.size());
  for (size_t i = 0; i < dirs.size(); i++) {
    Vec3 n = dirs[i].normalized();
    batch.origins[i] = n * R;
    batch.directions[i] = n * (-1.0);
  }
  return batch;
}

}  // namespace pler
