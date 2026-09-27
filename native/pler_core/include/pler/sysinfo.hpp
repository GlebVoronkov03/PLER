#pragma once
#include <cstddef>

namespace pler {

struct MemoryInfo {
  std::size_t free_ram_bytes = 0;
  std::size_t total_ram_bytes = 0;
  std::size_t free_vram_bytes = 0;
  std::size_t total_vram_bytes = 0;
};

MemoryInfo query_memory();

/**
 * Recommended max ray count from free memory.
 * Rough: ~200 bytes/ray host + mesh BVH already loaded.
 */
int recommended_max_rays(const MemoryInfo& mem, int hard_cap = 200000);

}  // namespace pler
