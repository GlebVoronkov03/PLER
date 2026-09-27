#include "pler_cuda.h"

#include <cuda_runtime.h>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

struct Accel {
  float* d_v = nullptr;
  int* d_tri = nullptr;
  PlerBvhNode* d_nodes = nullptr;
  int* d_ti = nullptr;
  int nverts = 0;
  int ntris = 0;
  int nnodes = 0;
  int nti = 0;
};

__device__ bool aabb_hit(const float3& o, const float3& d,
                        const float* bmin, const float* bmax, float t_max) {
  float tmin = 0.f, tmax = t_max;
  const float eps = 1e-12f;
#pragma unroll
  for (int a = 0; a < 3; a++) {
    float od = (&o.x)[a];
    float dd = (&d.x)[a];
    float inv = 1.0f / (fabsf(dd) < eps ? (dd >= 0.f ? eps : -eps) : dd);
    float t0 = (bmin[a] - od) * inv;
    float t1 = (bmax[a] - od) * inv;
    if (inv < 0.f) {
      float tmp = t0;
      t0 = t1;
      t1 = tmp;
    }
    tmin = t0 > tmin ? t0 : tmin;
    tmax = t1 < tmax ? t1 : tmax;
    if (tmax < tmin) return false;
  }
  return true;
}

__device__ bool tri_hit(const float3& o, const float3& d,
                       const float3& v0, const float3& v1, const float3& v2,
                       float& t_out) {
  const float eps = 1e-7f;
  float3 e1 = make_float3(v1.x - v0.x, v1.y - v0.y, v1.z - v0.z);
  float3 e2 = make_float3(v2.x - v0.x, v2.y - v0.y, v2.z - v0.z);
  float3 p = make_float3(d.y * e2.z - d.z * e2.y,
                         d.z * e2.x - d.x * e2.z,
                         d.x * e2.y - d.y * e2.x);
  float det = e1.x * p.x + e1.y * p.y + e1.z * p.z;
  if (fabsf(det) < eps) return false;
  float inv = 1.0f / det;
  float3 tvec = make_float3(o.x - v0.x, o.y - v0.y, o.z - v0.z);
  float u = (tvec.x * p.x + tvec.y * p.y + tvec.z * p.z) * inv;
  if (u < 0.f || u > 1.f) return false;
  float3 q = make_float3(tvec.y * e1.z - tvec.z * e1.y,
                         tvec.z * e1.x - tvec.x * e1.z,
                         tvec.x * e1.y - tvec.y * e1.x);
  float v = (d.x * q.x + d.y * q.y + d.z * q.z) * inv;
  if (v < 0.f || u + v > 1.f) return false;
  float t = (e2.x * q.x + e2.y * q.y + e2.z * q.z) * inv;
  if (t <= eps) return false;
  t_out = t;
  return true;
}

__global__ void cast_bvh_kernel(const float* verts, int nverts,
                                const int* tris, int /*ntris*/,
                                const PlerBvhNode* nodes, int nnodes,
                                const int* tri_indices, int /*nti*/,
                                const float* origins, const float* dirs,
                                int nrays, float* thit) {
  int i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i >= nrays) return;
  if (nnodes <= 0) {
    thit[i] = INFINITY;
    return;
  }
  float3 o = make_float3(origins[i * 3], origins[i * 3 + 1], origins[i * 3 + 2]);
  float3 d = make_float3(dirs[i * 3], dirs[i * 3 + 1], dirs[i * 3 + 2]);
  float best = 1e30f;
  bool hit = false;

  int stack[64];
  int sp = 0;
  stack[sp++] = 0;
  while (sp > 0) {
    int ni = stack[--sp];
    if (ni < 0 || ni >= nnodes) continue;
    const PlerBvhNode& node = nodes[ni];
    if (!aabb_hit(o, d, node.bmin, node.bmax, best)) continue;
    if (node.left < 0) {
      for (int k = 0; k < node.tri_count; k++) {
        int ti = tri_indices[node.tri_start + k];
        int i0 = tris[ti * 3], i1 = tris[ti * 3 + 1], i2 = tris[ti * 3 + 2];
        if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= nverts || i1 >= nverts || i2 >= nverts)
          continue;
        float3 v0 = make_float3(verts[i0 * 3], verts[i0 * 3 + 1], verts[i0 * 3 + 2]);
        float3 v1 = make_float3(verts[i1 * 3], verts[i1 * 3 + 1], verts[i1 * 3 + 2]);
        float3 v2 = make_float3(verts[i2 * 3], verts[i2 * 3 + 1], verts[i2 * 3 + 2]);
        float tt;
        if (tri_hit(o, d, v0, v1, v2, tt) && tt < best) {
          best = tt;
          hit = true;
        }
      }
    } else if (sp < 62) {
      stack[sp++] = node.left;
      stack[sp++] = node.right;
    }
  }
  thit[i] = hit ? best : INFINITY;
}

void free_accel(Accel* a) {
  if (!a) return;
  if (a->d_v) cudaFree(a->d_v);
  if (a->d_tri) cudaFree(a->d_tri);
  if (a->d_nodes) cudaFree(a->d_nodes);
  if (a->d_ti) cudaFree(a->d_ti);
  delete a;
}

Accel* upload_accel(const float* vertices, int nverts,
                    const int* triangles, int ntris,
                    const PlerBvhNode* nodes, int nnodes,
                    const int* tri_indices, int ntri_indices) {
  if (nverts <= 0 || ntris <= 0 || nnodes <= 0 || ntri_indices <= 0) return nullptr;

  size_t bytes = sizeof(float) * (size_t)nverts * 3 +
                 sizeof(int) * (size_t)ntris * 3 +
                 sizeof(PlerBvhNode) * (size_t)nnodes +
                 sizeof(int) * (size_t)ntri_indices;
  size_t free_b = 0, total_b = 0;
  if (cudaMemGetInfo(&free_b, &total_b) == cudaSuccess) {
    if (bytes + (32ull << 20) > free_b) return nullptr;
  }

  Accel* a = new Accel();
  a->nverts = nverts;
  a->ntris = ntris;
  a->nnodes = nnodes;
  a->nti = ntri_indices;

  auto fail = [&]() {
    free_accel(a);
    return (Accel*)nullptr;
  };

  if (cudaMalloc(&a->d_v, sizeof(float) * nverts * 3) != cudaSuccess) return fail();
  if (cudaMalloc(&a->d_tri, sizeof(int) * ntris * 3) != cudaSuccess) return fail();
  if (cudaMalloc(&a->d_nodes, sizeof(PlerBvhNode) * nnodes) != cudaSuccess) return fail();
  if (cudaMalloc(&a->d_ti, sizeof(int) * ntri_indices) != cudaSuccess) return fail();

  cudaMemcpy(a->d_v, vertices, sizeof(float) * nverts * 3, cudaMemcpyHostToDevice);
  cudaMemcpy(a->d_tri, triangles, sizeof(int) * ntris * 3, cudaMemcpyHostToDevice);
  cudaMemcpy(a->d_nodes, nodes, sizeof(PlerBvhNode) * nnodes, cudaMemcpyHostToDevice);
  cudaMemcpy(a->d_ti, tri_indices, sizeof(int) * ntri_indices, cudaMemcpyHostToDevice);
  return a;
}

int cast_with_accel(Accel* a, const float* origins, const float* directions,
                    int nrays, float* t_hit_out) {
  if (!a || nrays <= 0) return -2;

  size_t ray_bytes = sizeof(float) * (size_t)nrays * 7;
  size_t free_b = 0, total_b = 0;
  if (cudaMemGetInfo(&free_b, &total_b) == cudaSuccess) {
    if (ray_bytes + (16ull << 20) > free_b) return -3;
  }

  float *d_o = nullptr, *d_d = nullptr, *d_t = nullptr;
  auto fail = [&](int code) {
    if (d_o) cudaFree(d_o);
    if (d_d) cudaFree(d_d);
    if (d_t) cudaFree(d_t);
    return code;
  };

  if (cudaMalloc(&d_o, sizeof(float) * nrays * 3) != cudaSuccess) return fail(-4);
  if (cudaMalloc(&d_d, sizeof(float) * nrays * 3) != cudaSuccess) return fail(-4);
  if (cudaMalloc(&d_t, sizeof(float) * nrays) != cudaSuccess) return fail(-4);

  cudaMemcpy(d_o, origins, sizeof(float) * nrays * 3, cudaMemcpyHostToDevice);
  cudaMemcpy(d_d, directions, sizeof(float) * nrays * 3, cudaMemcpyHostToDevice);

  int block = 128;
  int grid = (nrays + block - 1) / block;
  cast_bvh_kernel<<<grid, block>>>(a->d_v, a->nverts, a->d_tri, a->ntris,
                                   a->d_nodes, a->nnodes, a->d_ti, a->nti,
                                   d_o, d_d, nrays, d_t);
  if (cudaDeviceSynchronize() != cudaSuccess) return fail(-5);

  cudaMemcpy(t_hit_out, d_t, sizeof(float) * nrays, cudaMemcpyDeviceToHost);
  fail(0);
  return 0;
}

}  // namespace

extern "C" int pler_cuda_available(void) {
  int n = 0;
  if (cudaGetDeviceCount(&n) != cudaSuccess) return 0;
  return n > 0 ? 1 : 0;
}

extern "C" int pler_cuda_mem_info(unsigned long long* free_bytes,
                                  unsigned long long* total_bytes) {
  size_t free_b = 0, total_b = 0;
  if (cudaMemGetInfo(&free_b, &total_b) != cudaSuccess) return -1;
  if (free_bytes) *free_bytes = free_b;
  if (total_bytes) *total_bytes = total_b;
  return 0;
}

extern "C" int pler_cuda_cast_bvh(const float* vertices, int nverts,
                                  const int* triangles, int ntris,
                                  const PlerBvhNode* nodes, int nnodes,
                                  const int* tri_indices, int ntri_indices,
                                  const float* origins, const float* directions,
                                  int nrays, float* t_hit_out) {
  if (!pler_cuda_available()) return -1;
  Accel* a = upload_accel(vertices, nverts, triangles, ntris,
                          nodes, nnodes, tri_indices, ntri_indices);
  if (!a) return -4;
  int rc = cast_with_accel(a, origins, directions, nrays, t_hit_out);
  free_accel(a);
  return rc;
}

extern "C" PlerCudaAccel* pler_cuda_accel_create(const float* vertices, int nverts,
                                                 const int* triangles, int ntris,
                                                 const PlerBvhNode* nodes, int nnodes,
                                                 const int* tri_indices, int ntri_indices) {
  if (!pler_cuda_available()) return nullptr;
  return reinterpret_cast<PlerCudaAccel*>(
      upload_accel(vertices, nverts, triangles, ntris, nodes, nnodes,
                   tri_indices, ntri_indices));
}

extern "C" int pler_cuda_accel_cast(PlerCudaAccel* accel,
                                    const float* origins, const float* directions,
                                    int nrays, float* t_hit_out) {
  return cast_with_accel(reinterpret_cast<Accel*>(accel), origins, directions,
                         nrays, t_hit_out);
}

extern "C" void pler_cuda_accel_destroy(PlerCudaAccel* accel) {
  free_accel(reinterpret_cast<Accel*>(accel));
}
