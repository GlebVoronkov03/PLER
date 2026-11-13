#pragma once
#include "pler/types.hpp"
#include "pler/calibrate.hpp"
#include "pler/frame.hpp"
#include "pler/raycast.hpp"
#include "pler/score.hpp"
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

/** Compare two in-memory meshes (copies are normalized internally). */
PlerResult compute_pler_meshes(Mesh ref, Mesh test, const PlerOptions& opt,
                               ProgressFn progress = nullptr,
                               std::atomic<bool>* cancel = nullptr);

/**
 * Cached reference for repeated comparisons (optimizer loop).
 * Frame + Fibonacci directions + reference ray lengths are fixed at create.
 */
class PlerReference {
 public:
  static PlerReference create(Mesh ref, const PlerOptions& opt,
                              ProgressFn progress = nullptr,
                              std::atomic<bool>* cancel = nullptr);

  PlerResult compare(Mesh test, ProgressFn progress = nullptr,
                     std::atomic<bool>* cancel = nullptr) const;

  int num_rays() const { return num_rays_; }
  const SphereFrame& world_sphere() const { return world_; }

 private:
  PlerOptions opt_{};
  SphereFrame world_{};
  Mesh ref_unit_{};
  std::vector<Vec3> dirs_{};
  LengthStats L_ref_{};
  int num_rays_ = 0;
  Backend backend_ = Backend::CPU;
  TopologyFeatures topo_ref_{};
  double tsi_placeholder_ = -1.0;
  std::size_t vertex_count_ref_ = 0;
};

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
