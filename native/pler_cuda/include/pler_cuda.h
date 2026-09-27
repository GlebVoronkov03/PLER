#pragma once
#ifdef __cplusplus
extern "C" {
#endif

/** Flat BVH node (must match pler::BVHNodeF32). */
typedef struct PlerBvhNode {
  float bmin[3];
  float bmax[3];
  int left;       /* -1 = leaf */
  int right;
  int tri_start;
  int tri_count;
} PlerBvhNode;

/** Returns 1 if a CUDA device is usable. */
int pler_cuda_available(void);

/** Free/total VRAM in bytes; returns 0 on success. */
int pler_cuda_mem_info(unsigned long long* free_bytes,
                       unsigned long long* total_bytes);

/**
 * Cast rays on GPU via BVH.
 * vertices: nverts*3, triangles: ntris*3 (mesh),
 * nodes: nnodes BVH nodes, tri_indices: leaf remapping,
 * origins/directions: nrays*3. Writes t_hit (inf on miss).
 * Returns 0 on success.
 */
int pler_cuda_cast_bvh(const float* vertices, int nverts,
                       const int* triangles, int ntris,
                       const PlerBvhNode* nodes, int nnodes,
                       const int* tri_indices, int ntri_indices,
                       const float* origins, const float* directions,
                       int nrays, float* t_hit_out);

/** Opaque uploaded mesh+BVH for multi-batch casts without re-upload. */
typedef struct PlerCudaAccel PlerCudaAccel;

PlerCudaAccel* pler_cuda_accel_create(const float* vertices, int nverts,
                                      const int* triangles, int ntris,
                                      const PlerBvhNode* nodes, int nnodes,
                                      const int* tri_indices, int ntri_indices);
int pler_cuda_accel_cast(PlerCudaAccel* accel,
                         const float* origins, const float* directions,
                         int nrays, float* t_hit_out);
void pler_cuda_accel_destroy(PlerCudaAccel* accel);

#ifdef __cplusplus
}
#endif
