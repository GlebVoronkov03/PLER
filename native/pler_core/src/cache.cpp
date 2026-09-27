#include "pler/cache.hpp"
#include "pler/frame.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pler {
namespace {

std::string fib_path(const std::string& cache_dir, int n) {
  return cache_dir + "/fib_" + std::to_string(n) + ".bin";
}

}  // namespace

std::string mesh_cache_key(const std::string& path) {
  if (path.empty()) return {};
  std::error_code ec;
  auto sz = std::filesystem::file_size(path, ec);
  if (ec) return {};
  auto ft = std::filesystem::last_write_time(path, ec);
  if (ec) return {};
  auto ticks = ft.time_since_epoch().count();
  std::ostringstream os;
  os << path << "|" << sz << "|" << ticks;
  return os.str();
}

std::vector<Vec3> cached_fibonacci(int n, const std::string& cache_dir, bool enabled) {
  if (!enabled || cache_dir.empty() || n <= 0)
    return fibonacci_directions(n);

  std::error_code ec;
  std::filesystem::create_directories(cache_dir, ec);
  std::string path = fib_path(cache_dir, n);
  {
    std::ifstream in(path, std::ios::binary);
    if (in) {
      int stored = 0;
      in.read(reinterpret_cast<char*>(&stored), sizeof(stored));
      if (stored == n) {
        std::vector<Vec3> dirs(static_cast<size_t>(n));
        in.read(reinterpret_cast<char*>(dirs.data()),
                static_cast<std::streamsize>(sizeof(Vec3) * static_cast<size_t>(n)));
        if (in) return dirs;
      }
    }
  }

  auto dirs = fibonacci_directions(n);
  std::ofstream out(path, std::ios::binary);
  if (out) {
    out.write(reinterpret_cast<const char*>(&n), sizeof(n));
    out.write(reinterpret_cast<const char*>(dirs.data()),
              static_cast<std::streamsize>(sizeof(Vec3) * dirs.size()));
  }
  return dirs;
}

}  // namespace pler
