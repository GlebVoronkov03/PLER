#include "pler/align_volume.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace pler {
namespace {

struct Grid {
  int res = 0;
  Vec3 origin;
  double cell = 1;
  std::vector<uint8_t> occ;

  int idx(int x, int y, int z) const {
    return (z * res + y) * res + x;
  }

  bool inside(int x, int y, int z) const {
    return x >= 0 && y >= 0 && z >= 0 && x < res && y < res && z < res;
  }
};

struct Bounds {
  Vec3 bmin{1e30, 1e30, 1e30};
  Vec3 bmax{-1e30, -1e30, -1e30};
  void expand(const Vec3& p) {
    bmin.x = std::min(bmin.x, p.x); bmin.y = std::min(bmin.y, p.y); bmin.z = std::min(bmin.z, p.z);
    bmax.x = std::max(bmax.x, p.x); bmax.y = std::max(bmax.y, p.y); bmax.z = std::max(bmax.z, p.z);
  }
  void expand(const Bounds& o) { expand(o.bmin); expand(o.bmax); }
  Vec3 extent() const { return bmax - bmin; }
  Vec3 center() const { return (bmin + bmax) * 0.5; }
};

Bounds mesh_bounds(const Mesh& m) {
  Bounds b;
  for (const auto& v : m.vertices) b.expand(v);
  return b;
}

void raster_triangle(Grid& g, Vec3 v0, Vec3 v1, Vec3 v2) {
  Bounds b;
  b.expand(v0); b.expand(v1); b.expand(v2);
  auto to_i = [&](double x) {
    return static_cast<int>(std::floor((x - g.origin.x) / g.cell));
  };
  int x0 = std::max(0, to_i(b.bmin.x) - 1);
  int y0 = std::max(0, to_i(b.bmin.y) - 1);
  int z0 = std::max(0, to_i(b.bmin.z) - 1);
  int x1 = std::min(g.res - 1, to_i(b.bmax.x) + 1);
  int y1 = std::min(g.res - 1, to_i(b.bmax.y) + 1);
  int z1 = std::min(g.res - 1, to_i(b.bmax.z) + 1);

  // barycentric in 3D plane: mark voxels near triangle via distance to plane + UV
  Vec3 e1 = v1 - v0, e2 = v2 - v0;
  Vec3 n = e1.cross(e2);
  double nn = n.norm();
  if (nn < 1e-15) return;
  n = n / nn;
  double plane_d = -n.dot(v0);

  for (int z = z0; z <= z1; z++) {
    for (int y = y0; y <= y1; y++) {
      for (int x = x0; x <= x1; x++) {
        Vec3 p = g.origin + Vec3{(x + 0.5) * g.cell, (y + 0.5) * g.cell, (z + 0.5) * g.cell};
        double dist = std::fabs(n.dot(p) + plane_d);
        if (dist > g.cell * 0.9) continue;
        // project to 2D via dominant axis
        Vec3 w = p - v0;
        double uu = e1.dot(e1), uv = e1.dot(e2), vv = e2.dot(e2);
        double wu = w.dot(e1), wv = w.dot(e2);
        double den = uv * uv - uu * vv;
        if (std::fabs(den) < 1e-18) continue;
        double s = (uv * wv - vv * wu) / den;
        double t = (uv * wu - uu * wv) / den;
        if (s >= -0.05 && t >= -0.05 && s + t <= 1.05) {
          g.occ[static_cast<size_t>(g.idx(x, y, z))] = 1;
        }
      }
    }
  }
}

Grid voxelize(const Mesh& mesh, const Bounds& world, int res) {
  Grid g;
  g.res = res;
  Vec3 ext = world.extent();
  double maxe = std::max({ext.x, ext.y, ext.z, 1e-9});
  // pad
  g.origin = world.bmin - Vec3{maxe, maxe, maxe} * 0.05;
  double span = maxe * 1.1;
  g.cell = span / res;
  g.occ.assign(static_cast<size_t>(res * res * res), 0);
  for (const auto& t : mesh.triangles) {
    raster_triangle(g,
                    mesh.vertices[static_cast<size_t>(t[0])],
                    mesh.vertices[static_cast<size_t>(t[1])],
                    mesh.vertices[static_cast<size_t>(t[2])]);
  }
  return g;
}

double iou(const Grid& a, const Grid& b) {
  uint64_t inter = 0, uni = 0;
  const size_t n = a.occ.size();
  for (size_t i = 0; i < n; i++) {
    int av = a.occ[i], bv = b.occ[i];
    inter += static_cast<uint64_t>(av & bv);
    uni += static_cast<uint64_t>(av | bv);
  }
  if (uni == 0) return 0.0;
  return static_cast<double>(inter) / static_cast<double>(uni);
}

Grid transform_and_voxelize(const Mesh& mesh, const Transform& T,
                            const Bounds& world, int res) {
  Mesh copy = mesh;
  copy.apply(T);
  return voxelize(copy, world, res);
}

Transform make_rigid(double yaw, double pitch, double roll, const Vec3& t) {
  // R = Rz * Ry * Rx
  double cy = std::cos(yaw), sy = std::sin(yaw);
  double cp = std::cos(pitch), sp = std::sin(pitch);
  double cr = std::cos(roll), sr = std::sin(roll);
  Transform T;
  T.s = 1.0;
  T.t = t;
  // columns of R
  T.ex = {cy * cp, sy * cp, -sp};
  T.ey = {cy * sp * sr - sy * cr, sy * sp * sr + cy * cr, cp * sr};
  T.ez = {cy * sp * cr + sy * sr, sy * sp * cr - cy * sr, cp * cr};
  return T;
}

}  // namespace

double align_volume_iou(const Mesh& ref, Mesh& test, int resolution) {
  if (resolution < 8) resolution = 8;
  Bounds world = mesh_bounds(ref);
  world.expand(mesh_bounds(test));
  // expand for search room
  Vec3 e = world.extent();
  double pad = std::max({e.x, e.y, e.z}) * 0.25;
  world.bmin += Vec3{-pad, -pad, -pad};
  world.bmax += Vec3{pad, pad, pad};

  Grid gref = voxelize(ref, world, resolution);

  double best_iou = -1.0;
  Transform best = Transform::identity();

  // Coarse-to-fine: translation grid + yaw (primary), then refine pitch/roll
  Vec3 ref_c = mesh_bounds(ref).center();
  Vec3 test_c = mesh_bounds(test).center();
  Vec3 base_t = ref_c - test_c;

  auto evaluate = [&](const Transform& T) {
    Grid gt = transform_and_voxelize(test, T, world, resolution);
    return iou(gref, gt);
  };

  // Stage 1: translation offsets around base_t + yaw
  const double extent = std::max({e.x, e.y, e.z, 1e-6});
  const double step1 = extent / 8.0;
  for (int ix = -2; ix <= 2; ix++) {
    for (int iy = -2; iy <= 2; iy++) {
      for (int iz = -2; iz <= 2; iz++) {
        for (int iyaw = 0; iyaw < 8; iyaw++) {
          Vec3 t = base_t + Vec3{ix * step1, iy * step1, iz * step1};
          Transform T = make_rigid(iyaw * (M_PI / 4.0), 0, 0, t);
          // Transform applies R*(p*s)+t but vertices are absolute; we need
          // p' = R*(p - test_c) + test_c + (t_delta) or p' = R*p + t with t absorbing.
          // Use: p' = R * (p - test_c) + ref_c + delta
          Transform U;
          U.s = 1;
          U.ex = T.ex; U.ey = T.ey; U.ez = T.ez;
          // apply: R*(p) *1 + t_eff where t_eff = -R*test_c + ref_c + delta
          Vec3 delta = Vec3{ix * step1, iy * step1, iz * step1};
          Vec3 Rt = T.ex * test_c.x + T.ey * test_c.y + T.ez * test_c.z;
          U.t = ref_c + delta - Rt;
          double v = evaluate(U);
          if (v > best_iou) {
            best_iou = v;
            best = U;
          }
        }
      }
    }
  }

  // Stage 2: local refine around best
  const double step2 = step1 * 0.35;
  Vec3 best_t = best.t;
  // extract approximate yaw from best.ex — skip; perturb t and small angles
  for (int ix = -2; ix <= 2; ix++) {
    for (int iy = -2; iy <= 2; iy++) {
      for (int iz = -2; iz <= 2; iz++) {
        for (int a = -2; a <= 2; a++) {
          Transform U = best;
          U.t = best_t + Vec3{ix * step2, iy * step2, iz * step2};
          // small yaw tweak via composing rotation around Z
          double yaw = a * (M_PI / 36.0);
          double cy = std::cos(yaw), sy = std::sin(yaw);
          Vec3 nex = U.ex * cy + U.ey * sy;
          Vec3 ney = U.ey * cy - U.ex * sy;
          U.ex = nex; U.ey = ney;
          double v = evaluate(U);
          if (v > best_iou) {
            best_iou = v;
            best = U;
          }
        }
      }
    }
  }

  test.apply(best);
  return best_iou;
}

}  // namespace pler
