#include "pler/raycast.hpp"
#include "pler/bvh.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

#if defined(PLER_HAS_CUDA) && PLER_HAS_CUDA
#include "pler_cuda.h"
#endif

namespace pler {
namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

std::vector<double> cast_cpu(const Mesh& mesh, const RayBatch& rays,
                             std::atomic<bool>* cancel, CastProgress progress) {
  BVH bvh;
  bvh.build(mesh);
  std::vector<double> out(rays.origins.size(), kInf);
  const size_t n = rays.origins.size();
  for (size_t i = 0; i < n; i++) {
    if (cancel && cancel->load()) break;
    out[i] = bvh.cast(rays.origins[i], rays.directions[i]);
    if (progress && (i % 256 == 0 || i + 1 == n))
      progress(static_cast<float>(i + 1) / static_cast<float>(n));
  }
  return out;
}

std::vector<double> cast_cuda_impl(const Mesh& mesh, const RayBatch& rays,
                                   std::atomic<bool>* cancel, CastProgress progress) {
#if defined(PLER_HAS_CUDA) && PLER_HAS_CUDA
  BVH bvh;
  bvh.build(mesh);
  std::vector<BVHNodeF32> nodes;
  std::vector<int> tri_indices;
  std::vector<float> verts;
  std::vector<int> tris;
  bvh.pack_f32(nodes, tri_indices, verts, tris);

  static_assert(sizeof(BVHNodeF32) == sizeof(PlerBvhNode),
                "BVHNodeF32 must match PlerBvhNode layout");

  PlerCudaAccel* accel = pler_cuda_accel_create(
      verts.data(), static_cast<int>(mesh.vertices.size()),
      tris.data(), static_cast<int>(mesh.triangles.size()),
      reinterpret_cast<const PlerBvhNode*>(nodes.data()),
      static_cast<int>(nodes.size()),
      tri_indices.data(), static_cast<int>(tri_indices.size()));
  if (!accel) throw PlerError("CUDA BVH upload failed");

  const int nr = static_cast<int>(rays.origins.size());
  std::vector<float> origins(static_cast<size_t>(nr) * 3);
  std::vector<float> dirs(static_cast<size_t>(nr) * 3);
  for (int i = 0; i < nr; i++) {
    origins[i * 3] = static_cast<float>(rays.origins[static_cast<size_t>(i)].x);
    origins[i * 3 + 1] = static_cast<float>(rays.origins[static_cast<size_t>(i)].y);
    origins[i * 3 + 2] = static_cast<float>(rays.origins[static_cast<size_t>(i)].z);
    dirs[i * 3] = static_cast<float>(rays.directions[static_cast<size_t>(i)].x);
    dirs[i * 3 + 1] = static_cast<float>(rays.directions[static_cast<size_t>(i)].y);
    dirs[i * 3 + 2] = static_cast<float>(rays.directions[static_cast<size_t>(i)].z);
  }
  std::vector<float> thit(static_cast<size_t>(nr),
                          std::numeric_limits<float>::infinity());

  const int batch = 512;
  int rc = 0;
  for (int start = 0; start < nr; start += batch) {
    if (cancel && cancel->load()) break;
    int count = std::min(batch, nr - start);
    rc = pler_cuda_accel_cast(accel,
                              origins.data() + start * 3,
                              dirs.data() + start * 3,
                              count, thit.data() + start);
    if (rc != 0) break;
    if (progress)
      progress(static_cast<float>(start + count) / static_cast<float>(nr));
  }
  pler_cuda_accel_destroy(accel);
  if (rc != 0) throw PlerError("CUDA BVH ray cast failed");

  std::vector<double> out(static_cast<size_t>(nr));
  for (int i = 0; i < nr; i++) {
    out[static_cast<size_t>(i)] =
        std::isfinite(thit[static_cast<size_t>(i)])
            ? static_cast<double>(thit[static_cast<size_t>(i)])
            : kInf;
  }
  return out;
#else
  (void)mesh;
  (void)rays;
  (void)cancel;
  (void)progress;
  throw PlerError("CUDA backend not compiled");
#endif
}

}  // namespace

bool cuda_available() {
#if defined(PLER_HAS_CUDA) && PLER_HAS_CUDA
  return pler_cuda_available() != 0;
#else
  return false;
#endif
}

Backend select_backend(bool prefer_cuda) {
  if (prefer_cuda && cuda_available()) return Backend::CUDA;
  return Backend::CPU;
}

std::vector<double> cast_rays(const Mesh& mesh, const RayBatch& rays,
                              Backend backend, std::atomic<bool>* cancel,
                              CastProgress progress) {
  if (mesh.empty()) throw PlerError("Cannot cast on empty mesh");
  if (rays.origins.empty()) throw PlerError("Empty ray batch");
  if (backend == Backend::CUDA) {
    try {
      return cast_cuda_impl(mesh, rays, cancel, progress);
    } catch (...) {
      return cast_cpu(mesh, rays, cancel, progress);
    }
  }
  return cast_cpu(mesh, rays, cancel, progress);
}

double compare_backends(const Mesh& mesh, const RayBatch& rays) {
  auto cpu = cast_rays(mesh, rays, Backend::CPU, nullptr);
  if (!cuda_available()) return 0.0;
  auto gpu = cast_rays(mesh, rays, Backend::CUDA, nullptr);
  double max_diff = 0;
  for (size_t i = 0; i < cpu.size(); i++) {
    double a = std::isfinite(cpu[i]) ? cpu[i] : 1e9;
    double b = std::isfinite(gpu[i]) ? gpu[i] : 1e9;
    max_diff = std::max(max_diff, std::fabs(a - b));
  }
  return max_diff;
}

}  // namespace pler
