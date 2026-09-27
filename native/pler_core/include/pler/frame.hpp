#pragma once
#include "pler/types.hpp"
#include <vector>

namespace pler {

/** Build shared sphere from reference (+ margin over both meshes). */
SphereFrame make_shared_sphere(const Mesh& ref, const Mesh& test, double margin);

/**
 * Place both meshes into the shared sphere frame in-place:
 * center on reference AABB/centroid mid, scale so max radius * margin = R (=1).
 * Returns the frame (radius always 1 after normalize).
 */
SphereFrame normalize_pair(Mesh& ref, Mesh& test, double margin);

/** Fibonacci (golden) unit directions, equal-area lattice. */
std::vector<Vec3> fibonacci_directions(int n);

/** Inward ray origins at R * n, directions -n. */
struct RayBatch {
  std::vector<Vec3> origins;
  std::vector<Vec3> directions;
};

RayBatch make_inward_rays(const std::vector<Vec3>& dirs, double R);

}  // namespace pler
