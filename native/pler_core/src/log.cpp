#include "pler/log.hpp"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace pler {
namespace {

std::string stamp() {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &t);
#else
  localtime_r(&t, &tm);
#endif
  std::ostringstream os;
  os << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
  return os.str();
}

void ensure_dir(const std::string& cache_dir) {
  std::error_code ec;
  std::filesystem::create_directories(cache_dir, ec);
}

std::string csv_escape(const std::string& s) {
  if (s.find_first_of(",\"\n") == std::string::npos) return s;
  std::string o = "\"";
  for (char c : s) {
    if (c == '"') o += "\"\"";
    else o += c;
  }
  o += "\"";
  return o;
}

}  // namespace

std::string default_log_path(const std::string& cache_dir) {
  return cache_dir + "/pler.log";
}

std::string default_research_csv_path(const std::string& cache_dir) {
  return cache_dir + "/research_last.csv";
}

void append_log(const std::string& cache_dir, const std::string& line) {
  if (cache_dir.empty()) return;
  ensure_dir(cache_dir);
  std::ofstream out(default_log_path(cache_dir), std::ios::app);
  if (!out) return;
  out << stamp() << "  " << line << "\n";
}

void log_result(const std::string& cache_dir, const std::string& ref,
                const std::string& test, const PlerResult& r) {
  std::ostringstream os;
  os << "ref=" << ref << " test=" << test;
  if (!r.ok) {
    os << " ERROR=" << r.error;
  } else {
    os << " PLER=" << r.pler_db << "dB MSE=" << r.mse << " rays=" << r.num_rays
       << " backend=" << r.backend << " time=" << r.computation_time_s << "s";
    if (r.tsi >= 0) os << " TSI=" << r.tsi;
    if (r.align_volume_used) os << " IoU=" << r.volume_iou;
  }
  append_log(cache_dir, os.str());
}

bool write_research_csv(const std::string& path,
                        const std::vector<std::pair<std::string, PlerResult>>& rows) {
  std::filesystem::path p(path);
  if (p.has_parent_path()) {
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
  }
  std::ofstream out(path);
  if (!out) return false;
  out << "model,pler_db,mse,peak,miss_ref_pct,miss_test_pct,rays,backend,time_s,"
         "tsi,align_iou,verts_ref,verts_test,ok,error\n";
  for (const auto& row : rows) {
    const auto& r = row.second;
    out << csv_escape(row.first) << "," << r.pler_db << "," << r.mse << ","
        << r.peak << "," << (r.miss_rate_ref * 100.0) << ","
        << (r.miss_rate_test * 100.0) << "," << r.num_rays << ","
        << csv_escape(r.backend) << "," << r.computation_time_s << ","
        << r.tsi << "," << r.volume_iou << "," << r.vertex_count_ref << ","
        << r.vertex_count_test << "," << (r.ok ? 1 : 0) << ","
        << csv_escape(r.error) << "\n";
  }
  return true;
}

}  // namespace pler
