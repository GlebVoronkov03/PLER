#include "pler/sysinfo.hpp"
#include "pler/raycast.hpp"

#include <algorithm>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#if defined(PLER_HAS_CUDA) && PLER_HAS_CUDA
#include "pler_cuda.h"
#endif

namespace pler {

MemoryInfo query_memory() {
  MemoryInfo m;
#ifdef _WIN32
  MEMORYSTATUSEX st{};
  st.dwLength = sizeof(st);
  if (GlobalMemoryStatusEx(&st)) {
    m.free_ram_bytes = static_cast<std::size_t>(st.ullAvailPhys);
    m.total_ram_bytes = static_cast<std::size_t>(st.ullTotalPhys);
  }
#endif
#if defined(PLER_HAS_CUDA) && PLER_HAS_CUDA
  if (cuda_available()) {
    unsigned long long free_b = 0, total_b = 0;
    if (pler_cuda_mem_info(&free_b, &total_b) == 0) {
      m.free_vram_bytes = static_cast<std::size_t>(free_b);
      m.total_vram_bytes = static_cast<std::size_t>(total_b);
    }
  }
#endif
  return m;
}

int recommended_max_rays(const MemoryInfo& mem, int hard_cap) {
  const std::size_t per_ray = 64;
  std::size_t budget = mem.free_ram_bytes / 4;
  if (mem.free_vram_bytes > 0)
    budget = std::min(budget, mem.free_vram_bytes / 2);
  if (budget < (32ull << 20)) budget = 32ull << 20;
  int n = static_cast<int>(budget / per_ray);
  if (n < 1000) n = 1000;
  if (n > hard_cap) n = hard_cap;
  return n;
}

}  // namespace pler
