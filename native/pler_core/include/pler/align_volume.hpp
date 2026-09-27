#pragma once
#include "pler/types.hpp"

namespace pler {

/**
 * Maximize occupancy IoU of test vs ref under rigid (R,t) on test.
 * Modifies test in-place. Returns best IoU achieved.
 */
double align_volume_iou(const Mesh& ref, Mesh& test, int resolution);

}  // namespace pler
