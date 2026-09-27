#pragma once
#include <string>
#include <vector>

namespace pler {

bool natural_less(const std::string& a, const std::string& b);
void natural_sort_paths(std::vector<std::string>& paths);
std::string executable_dir();

}  // namespace pler
