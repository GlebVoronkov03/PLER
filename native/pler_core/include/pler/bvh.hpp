#pragma once
#include "pler/types.hpp"
#include <atomic>
#include <functional>
#include <vector>

namespace pler {

struct AABB {
  Vec3 bmin{1e30, 1e30, 1e30};
  Vec3 bmax{-1e30, -1e30, -1e30};
  void expand(const Vec3& p);
  void expand(const AABB& o);
  Vec3 center() const;
};

struct BVHNode {
  AABB box;
  int left = -1, right = -1;  // -1 = leaf
  int tri_start = 0, tri_count = 0;
};

/** Flat float32 node layout matching CUDA upload. */
struct BVHNodeF32 {
  float bmin[3];
  float bmax[3];
  int left, right;
  int tri_start, tri_count;
};

struct BVH {
  std::vector<BVHNode> nodes;
  std::vector<int> tri_indices;
  const Mesh* mesh = nullptr;

  void build(const Mesh& m);
  double cast(const Vec3& o, const Vec3& d) const;

  /** Pack for CUDA: nodes (float AABB), remapped tri index list, mesh verts/tris. */
  void pack_f32(std::vector<BVHNodeF32>& out_nodes,
                std::vector<int>& out_tri_indices,
                std::vector<float>& out_verts,
                std::vector<int>& out_tris) const;
};

}  // namespace pler
