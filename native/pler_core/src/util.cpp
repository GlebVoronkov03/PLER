#include "pler/util.hpp"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace pler {
namespace {

int cmp_natural(const char* a, const char* b) {
  while (*a && *b) {
    if (std::isdigit(static_cast<unsigned char>(*a)) &&
        std::isdigit(static_cast<unsigned char>(*b))) {
      unsigned long long na = 0, nb = 0;
      while (*a && std::isdigit(static_cast<unsigned char>(*a)))
        na = na * 10 + static_cast<unsigned>(*a++ - '0');
      while (*b && std::isdigit(static_cast<unsigned char>(*b)))
        nb = nb * 10 + static_cast<unsigned>(*b++ - '0');
      if (na != nb) return na < nb ? -1 : 1;
      continue;
    }
    char ca = static_cast<char>(std::tolower(static_cast<unsigned char>(*a)));
    char cb = static_cast<char>(std::tolower(static_cast<unsigned char>(*b)));
    if (ca != cb) return ca < cb ? -1 : 1;
    ++a;
    ++b;
  }
  if (*a == *b) return 0;
  return *a ? 1 : -1;
}

}  // namespace

bool natural_less(const std::string& a, const std::string& b) {
  return cmp_natural(a.c_str(), b.c_str()) < 0;
}

void natural_sort_paths(std::vector<std::string>& paths) {
  std::sort(paths.begin(), paths.end(), [](const std::string& a, const std::string& b) {
    auto slash_a = a.find_last_of("/\\");
    auto slash_b = b.find_last_of("/\\");
    std::string na = slash_a == std::string::npos ? a : a.substr(slash_a + 1);
    std::string nb = slash_b == std::string::npos ? b : b.substr(slash_b + 1);
    return natural_less(na, nb);
  });
}

std::string executable_dir() {
#ifdef _WIN32
  char buf[MAX_PATH] = {};
  DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
  if (!n) return {};
  std::string p(buf, buf + n);
  auto slash = p.find_last_of("/\\");
  if (slash == std::string::npos) return {};
  return p.substr(0, slash);
#else
  return {};
#endif
}

}  // namespace pler
