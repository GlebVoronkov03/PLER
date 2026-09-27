#include "pler/align_icp.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace pler {
namespace {

Vec3 mesh_centroid(const Mesh& m) {
  Vec3 c;
  for (const auto& v : m.vertices) c += v;
  if (m.vertices.empty()) return c;
  return c * (1.0 / static_cast<double>(m.vertices.size()));
}

/** Closest vertex on ref (brute; fine for moderate meshes). */
int nearest_vertex(const Mesh& ref, const Vec3& p) {
  int best = 0;
  double best_d = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < ref.vertices.size(); i++) {
    double d = (ref.vertices[i] - p).norm();
    if (d < best_d) {
      best_d = d;
      best = static_cast<int>(i);
    }
  }
  return best;
}

/** Kabsch SVD for 3x3 covariance -> rotation (Jacobi-ish via analytic 3D). */
Transform rigid_from_correspondences(const std::vector<Vec3>& src,
                                     const std::vector<Vec3>& dst) {
  const size_t n = src.size();
  if (n == 0) return Transform::identity();
  Vec3 cs, cd;
  for (size_t i = 0; i < n; i++) {
    cs += src[i];
    cd += dst[i];
  }
  cs = cs * (1.0 / static_cast<double>(n));
  cd = cd * (1.0 / static_cast<double>(n));

  // Covariance H = sum (src-cs)(dst-cd)^T
  double H[3][3] = {};
  for (size_t i = 0; i < n; i++) {
    Vec3 a = src[i] - cs;
    Vec3 b = dst[i] - cd;
    H[0][0] += a.x * b.x;
    H[0][1] += a.x * b.y;
    H[0][2] += a.x * b.z;
    H[1][0] += a.y * b.x;
    H[1][1] += a.y * b.y;
    H[1][2] += a.y * b.z;
    H[2][0] += a.z * b.x;
    H[2][1] += a.z * b.y;
    H[2][2] += a.z * b.z;
  }

  // Polar decomposition approximation via iterative orthonormalization of H
  // Start with H, Gram-Schmidt columns -> rotation estimate (stable enough for ICP)
  Vec3 c0{H[0][0], H[1][0], H[2][0]};
  Vec3 c1{H[0][1], H[1][1], H[2][1]};
  Vec3 c2{H[0][2], H[1][2], H[2][2]};
  c0 = c0.normalized();
  c1 = (c1 - c0 * c1.dot(c0)).normalized();
  c2 = c0.cross(c1);
  // Ensure right-handed
  if (c2.dot(Vec3{H[0][2], H[1][2], H[2][2]}) < 0) {
    c2 = c2 * (-1.0);
    c1 = c1 * (-1.0);
  }

  Transform T;
  // R maps src-centered to dst-centered: columns are basis in world
  // Apply: R * (p - cs) + cd
  T.ex = {c0.x, c1.x, c2.x};  // wait - we need rows as R*v
  // Store R such that R * v = ex*v.x + ey*v.y + ez*v.z (columns of R)
  T.ex = c0;
  T.ey = c1;
  T.ez = c2;
  // t = cd - R * cs
  Vec3 Rcs = T.ex * cs.x + T.ey * cs.y + T.ez * cs.z;
  T.t = cd - Rcs;
  T.s = 1.0;
  return T;
}

}  // namespace

double align_icp(const Mesh& ref, Mesh& test, int iterations) {
  if (ref.empty() || test.empty()) throw PlerError("ICP on empty mesh");
  if (iterations < 1) iterations = 1;

  // Subsample test for speed
  const size_t max_samples = 2000;
  std::vector<size_t> sample_idx;
  if (test.vertices.size() <= max_samples) {
    sample_idx.resize(test.vertices.size());
    for (size_t i = 0; i < sample_idx.size(); i++) sample_idx[i] = i;
  } else {
    sample_idx.resize(max_samples);
    for (size_t i = 0; i < max_samples; i++)
      sample_idx[i] = i * test.vertices.size() / max_samples;
  }

  double mean_d = 0;
  for (int it = 0; it < iterations; it++) {
    std::vector<Vec3> src, dst;
    src.reserve(sample_idx.size());
    dst.reserve(sample_idx.size());
    mean_d = 0;
    for (size_t si : sample_idx) {
      const Vec3& p = test.vertices[si];
      int ni = nearest_vertex(ref, p);
      src.push_back(p);
      dst.push_back(ref.vertices[static_cast<size_t>(ni)]);
      mean_d += (p - ref.vertices[static_cast<size_t>(ni)]).norm();
    }
    mean_d /= static_cast<double>(sample_idx.size());
    Transform T = rigid_from_correspondences(src, dst);
    test.apply(T);
  }
  return mean_d;
}

}  // namespace pler
