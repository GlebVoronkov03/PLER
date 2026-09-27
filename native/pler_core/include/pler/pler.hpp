#pragma once
#include "pler/types.hpp"
#include "pler/calibrate.hpp"
#include <atomic>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace pler {

enum class JobState { Idle, Queued, Running, Done, Error, Cancelled };
enum class JobKind { None, Single, Research, Calibrate };

struct JobStatus {
  JobState state = JobState::Idle;
  JobKind kind = JobKind::None;
  float progress = 0.f;
  std::string message;
  PlerResult result;
  std::vector<std::pair<std::string, PlerResult>> batch;
  CalibrateResult calibrate;
  std::string last_report_dir;
};

int suggest_rays(std::size_t vertices, std::size_t triangles, int min_rays,
                 int max_rays);

PlerResult compute_pler(const std::string& ref_path,
                        const std::string& test_path,
                        const PlerOptions& opt,
                        ProgressFn progress = nullptr,
                        std::atomic<bool>* cancel = nullptr);

class JobRunner {
 public:
  void start(const std::string& ref, const std::string& test, PlerOptions opt);
  void start_batch(const std::string& ref,
                   const std::vector<std::string>& tests,
                   PlerOptions opt);
  void start_research(const std::string& ref,
                      const std::vector<std::string>& tests,
                      const std::vector<int>& ray_counts,
                      PlerOptions opt);
  void start_calibrate(const std::string& ref, const std::string& test,
                       PlerOptions opt);
  void cancel();
  JobStatus snapshot() const;

 private:
  mutable std::mutex mu_;
  JobStatus status_;
  std::atomic<bool> cancel_{false};
};

}  // namespace pler
