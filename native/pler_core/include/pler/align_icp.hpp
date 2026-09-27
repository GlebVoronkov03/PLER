#pragma once
#include "pler/types.hpp"

namespace pler {

/**
 * Rigid point-to-point ICP: move test toward ref.
 * Modifies test in-place. Returns final mean closest-point distance.
 */
double align_icp(const Mesh& ref, Mesh& test, int iterations = 30);

}  // namespace pler
