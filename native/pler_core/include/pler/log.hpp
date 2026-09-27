#pragma once
#include "pler/types.hpp"
#include <string>
#include <utility>
#include <vector>

namespace pler {

void append_log(const std::string& cache_dir, const std::string& line);

void log_result(const std::string& cache_dir, const std::string& ref,
                const std::string& test, const PlerResult& r);

bool write_research_csv(const std::string& path,
                        const std::vector<std::pair<std::string, PlerResult>>& rows);

std::string default_log_path(const std::string& cache_dir);
std::string default_research_csv_path(const std::string& cache_dir);

}  // namespace pler
