#pragma once
#include "pler/types.hpp"
#include <string>

namespace pler {

/** Load triangle mesh from OBJ/STL (tinyobj) or Assimp formats when enabled. */
Mesh load_mesh(const std::string& path);

/** Assimp loader (throws if Assimp not linked). */
Mesh load_assimp(const std::string& path);

/** Fan-triangulate is applied during load. Empty mesh throws. */
void validate_mesh(const Mesh& mesh, const std::string& path);

}  // namespace pler
