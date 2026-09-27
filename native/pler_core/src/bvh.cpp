#include "pler/bvh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace pler {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();
constexpr double kEps = 1e-12;

bool intersect_aabb(const Vec3& orig, const Vec3& dir, const AABB& box, double t_max) {
  double tmin = 0.0, tmax = t_max;
  const double* o = &orig.x;
  const double* d = &dir.x;
  const double* mn = &box.bmin.x;
  const double* mx = &box.bmax.x;
  for (int a = 0; a < 3; a++) {
    double inv = 1.0 / (std::fabs(d[a]) < kEps ? (d[a] >= 0 ? kEps : -kEps) : d[a]);
    double t0 = (mn[a] - o[a]) * inv;
    double t1 = (mx[a] - o[a]) * inv;
    if (inv < 0) std::swap(t0, t1);
    tmin = t0 > tmin ? t0 : tmin;
    tmax = t1 < tmax ? t1 : tmax;
    if (tmax < tmin) return false;
  }
  return true;
}

bool intersect_triangle(const Vec3& orig, const Vec3& dir,
                        const Vec3& v0, const Vec3& v1, const Vec3& v2,
                        double& t_out) {
  Vec3 e1 = v1 - v0;
  Vec3 e2 = v2 - v0;
  Vec3 pvec = dir.cross(e2);
  double det = e1.dot(pvec);
  if (std::fabs(det) < kEps) return false;
  double inv_det = 1.0 / det;
  Vec3 tvec = orig - v0;
  double u = tvec.dot(pvec) * inv_det;
  if (u < 0.0 || u > 1.0) return false;
  Vec3 qvec = tvec.cross(e1);
  double v = dir.dot(qvec) * inv_det;
  if (v < 0.0 || u + v > 1.0) return false;
  double t = e2.dot(qvec) * inv_det;
  if (t <= kEps) return false;
  t_out = t;
  return true;
}

int build_rec(BVH& bvh, int start, int count) {
  BVHNode node;
  for (int i = 0; i < count; i++) {
    const auto& t = bvh.mesh->triangles[static_cast<size_t>(
        bvh.tri_indices[static_cast<size_t>(start + i)])];
    node.box.expand(bvh.mesh->vertices[static_cast<size_t>(t[0])]);
    node.box.expand(bvh.mesh->vertices[static_cast<size_t>(t[1])]);
    node.box.expand(bvh.mesh->vertices[static_cast<size_t>(t[2])]);
  }
  int idx = static_cast<int>(bvh.nodes.size());
  bvh.nodes.push_back(node);

  if (count <= 8) {
    bvh.nodes[static_cast<size_t>(idx)].tri_start = start;
    bvh.nodes[static_cast<size_t>(idx)].tri_count = count;
    return idx;
  }

  Vec3 ext = bvh.nodes[static_cast<size_t>(idx)].box.bmax -
             bvh.nodes[static_cast<size_t>(idx)].box.bmin;
  int axis = 0;
  if (ext.y > ext.x) axis = 1;
  if (ext.z > (axis == 0 ? ext.x : ext.y)) axis = 2;

  auto mid = start + count / 2;
  auto centroid = [&](int ti) {
    const auto& t = bvh.mesh->triangles[static_cast<size_t>(ti)];
    return (bvh.mesh->vertices[static_cast<size_t>(t[0])] +
            bvh.mesh->vertices[static_cast<size_t>(t[1])] +
            bvh.mesh->vertices[static_cast<size_t>(t[2])]) *
           (1.0 / 3.0);
  };
  std::nth_element(bvh.tri_indices.begin() + start, bvh.tri_indices.begin() + mid,
                   bvh.tri_indices.begin() + start + count,
                   [&](int a, int b) {
                     auto ca = centroid(a);
                     auto cb = centroid(b);
                     double va = axis == 0 ? ca.x : (axis == 1 ? ca.y : ca.z);
                     double vb = axis == 0 ? cb.x : (axis == 1 ? cb.y : cb.z);
                     return va < vb;
                   });

  int left = build_rec(bvh, start, mid - start);
  int right = build_rec(bvh, mid, start + count - mid);
  bvh.nodes[static_cast<size_t>(idx)].left = left;
  bvh.nodes[static_cast<size_t>(idx)].right = right;
  return idx;
}

}  // namespace

void AABB::expand(const Vec3& p) {
  bmin.x = std::min(bmin.x, p.x);
  bmin.y = std::min(bmin.y, p.y);
  bmin.z = std::min(bmin.z, p.z);
  bmax.x = std::max(bmax.x, p.x);
  bmax.y = std::max(bmax.y, p.y);
  bmax.z = std::max(bmax.z, p.z);
}

void AABB::expand(const AABB& o) {
  expand(o.bmin);
  expand(o.bmax);
}

Vec3 AABB::center() const { return (bmin + bmax) * 0.5; }

void BVH::build(const Mesh& m) {
  mesh = &m;
  nodes.clear();
  tri_indices.resize(m.triangles.size());
  for (size_t i = 0; i < tri_indices.size(); i++)
    tri_indices[i] = static_cast<int>(i);
  nodes.reserve(m.triangles.size() * 2);
  if (!tri_indices.empty())
    build_rec(*this, 0, static_cast<int>(tri_indices.size()));
}

double BVH::cast(const Vec3& o, const Vec3& d) const {
  double best = kInf;
  if (nodes.empty() || !mesh) return best;
  int stack[64];
  int sp = 0;
  stack[sp++] = 0;
  while (sp > 0) {
    int ni = stack[--sp];
    const BVHNode& node = nodes[static_cast<size_t>(ni)];
    if (!intersect_aabb(o, d, node.box, best)) continue;
    if (node.left < 0) {
      for (int i = 0; i < node.tri_count; i++) {
        int ti = tri_indices[static_cast<size_t>(node.tri_start + i)];
        const auto& t = mesh->triangles[static_cast<size_t>(ti)];
        double hit = 0;
        if (intersect_triangle(o, d,
                               mesh->vertices[static_cast<size_t>(t[0])],
                               mesh->vertices[static_cast<size_t>(t[1])],
                               mesh->vertices[static_cast<size_t>(t[2])],
                               hit)) {
          if (hit < best) best = hit;
        }
      }
    } else {
      if (sp < 62) {
        stack[sp++] = node.left;
        stack[sp++] = node.right;
      }
    }
  }
  return best;
}

void BVH::pack_f32(std::vector<BVHNodeF32>& out_nodes,
                   std::vector<int>& out_tri_indices,
                   std::vector<float>& out_verts,
                   std::vector<int>& out_tris) const {
  out_nodes.resize(nodes.size());
  for (size_t i = 0; i < nodes.size(); i++) {
    const BVHNode& n = nodes[i];
    BVHNodeF32& o = out_nodes[i];
    o.bmin[0] = static_cast<float>(n.box.bmin.x);
    o.bmin[1] = static_cast<float>(n.box.bmin.y);
    o.bmin[2] = static_cast<float>(n.box.bmin.z);
    o.bmax[0] = static_cast<float>(n.box.bmax.x);
    o.bmax[1] = static_cast<float>(n.box.bmax.y);
    o.bmax[2] = static_cast<float>(n.box.bmax.z);
    o.left = n.left;
    o.right = n.right;
    o.tri_start = n.tri_start;
    o.tri_count = n.tri_count;
  }
  out_tri_indices = tri_indices;
  out_verts.resize(mesh->vertices.size() * 3);
  for (size_t i = 0; i < mesh->vertices.size(); i++) {
    out_verts[i * 3] = static_cast<float>(mesh->vertices[i].x);
    out_verts[i * 3 + 1] = static_cast<float>(mesh->vertices[i].y);
    out_verts[i * 3 + 2] = static_cast<float>(mesh->vertices[i].z);
  }
  out_tris.resize(mesh->triangles.size() * 3);
  for (size_t i = 0; i < mesh->triangles.size(); i++) {
    out_tris[i * 3] = mesh->triangles[i][0];
    out_tris[i * 3 + 1] = mesh->triangles[i][1];
    out_tris[i * 3 + 2] = mesh->triangles[i][2];
  }
}

}  // namespace pler
