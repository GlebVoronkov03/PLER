#pragma once
#include "pler/types.hpp"

namespace pler {

TopologyFeatures compute_topology(const Mesh& mesh);

/** Jaccard over (components, boundary_loops) multisets as pairs. */
double topological_similarity(const TopologyFeatures& a, const TopologyFeatures& b);

}  // namespace pler
