#pragma once
#include "pler/frame.hpp"
#include "pler/types.hpp"
#include <string>
#include <vector>

namespace pler {

/** Fibonacci cache: returns directions for N (disk-backed if cache_enabled). */
std::vector<Vec3> cached_fibonacci(int n, const std::string& cache_dir, bool enabled);

/** Mesh cache key helper (path + size + mtime). Empty path => no cache. */
std::string mesh_cache_key(const std::string& path);

}  // namespace pler
