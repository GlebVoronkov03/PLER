#include "pler/pler.hpp"
#include "pler/calibrate.hpp"
#include "pler/log.hpp"
#include "pler/report.hpp"
#include "pler/util.hpp"

#include <algorithm>
#include <thread>
#include <utility>
#include <vector>

namespace pler {
namespace {

std::string basename_of(const std::string& path) {
  auto slash = path.find_last_of("/\\");
  if (slash == std::string::npos) return path;
  return path.substr(slash + 1);
}

}  // namespace

void JobRunner::start(const std::string& ref, const std::string& test, PlerOptions opt) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    status_ = JobStatus{};
    status_.state = JobState::Queued;
    status_.kind = JobKind::Single;
    status_.message = "queued";
  }
  cancel_.store(false);

  std::thread([this, ref, test, opt]() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      status_.state = JobState::Running;
      status_.message = "running";
    }
    auto prog = [this](float p, const std::string& msg) {
      std::lock_guard<std::mutex> lock(mu_);
      status_.progress = p;
      status_.message = msg;
    };
    PlerOptions local = opt;
    local.keep_lengths = true;
    PlerResult r = compute_pler(ref, test, local, prog, &cancel_);
    std::lock_guard<std::mutex> lock(mu_);
    status_.result = r;
    status_.kind = JobKind::Single;
    status_.progress = r.ok ? 1.f : status_.progress;
    if (cancel_.load()) {
      status_.state = JobState::Cancelled;
      status_.message = "cancelled";
    } else if (!r.ok) {
      status_.state = JobState::Error;
      status_.message = r.error;
    } else {
      status_.state = JobState::Done;
      status_.message = "done";
    }
  }).detach();
}

void JobRunner::start_batch(const std::string& ref,
                            const std::vector<std::string>& tests,
                            PlerOptions opt) {
  start_research(ref, tests, {opt.num_rays > 0 ? opt.num_rays : 5000}, opt);
}

void JobRunner::start_research(const std::string& ref,
                               const std::vector<std::string>& tests,
                               const std::vector<int>& ray_counts,
                               PlerOptions opt) {
  std::vector<std::string> ordered = tests;
  natural_sort_paths(ordered);

  {
    std::lock_guard<std::mutex> lock(mu_);
    status_ = JobStatus{};
    status_.state = JobState::Queued;
    status_.kind = JobKind::Research;
    status_.message = "queued research";
  }
  cancel_.store(false);

  std::thread([this, ref, ordered, ray_counts, opt]() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      status_.state = JobState::Running;
      status_.message = "research running";
    }

    const int n_models = static_cast<int>(ordered.size());
    const int n_rays = static_cast<int>(ray_counts.size());
    const int total = std::max(1, n_models * n_rays);
    int done = 0;

    for (int mi = 0; mi < n_models; mi++) {
      for (int ri = 0; ri < n_rays; ri++) {
        if (cancel_.load()) break;
        const std::string& path = ordered[static_cast<size_t>(mi)];
        std::string name = basename_of(path);
        int rays = ray_counts[static_cast<size_t>(ri)];
        if (rays <= 0) rays = 5000;

        {
          std::lock_guard<std::mutex> lock(mu_);
          status_.progress = static_cast<float>(done) / static_cast<float>(total);
          status_.message = name + " @ " + std::to_string(rays) + " rays";
        }

        PlerOptions local = opt;
        local.num_rays = rays;
        local.converge = false;

        auto prog = [this, done, total](float p, const std::string& msg) {
          std::lock_guard<std::mutex> lock(mu_);
          status_.progress =
              (static_cast<float>(done) + p) / static_cast<float>(total);
          status_.message = msg;
        };

        PlerResult r = compute_pler(ref, path, local, prog, &cancel_);
        std::string label =
            name + " [" + std::to_string(r.num_rays ? r.num_rays : rays) + "]";
        {
          std::lock_guard<std::mutex> lock(mu_);
          status_.batch.emplace_back(label, r);
          status_.result = r;
        }
        done++;
      }
      if (cancel_.load()) break;
    }

    auto batch_copy = [&]() {
      std::lock_guard<std::mutex> lock(mu_);
      return status_.batch;
    }();
    write_research_csv(default_research_csv_path(opt.cache_dir), batch_copy);
    ReportPaths rp = write_research_report(opt.cache_dir, batch_copy);
    append_log(opt.cache_dir, "research finished rows=" + std::to_string(done));

    std::lock_guard<std::mutex> lock(mu_);
    status_.last_report_dir = rp.dir;
    status_.progress = 1.f;
    status_.kind = JobKind::Research;
    if (cancel_.load()) {
      status_.state = JobState::Cancelled;
      status_.message = "cancelled";
    } else if (status_.batch.empty()) {
      status_.state = JobState::Error;
      status_.message = "no models processed";
    } else {
      status_.state = JobState::Done;
      status_.message = "research done";
    }
  }).detach();
}

void JobRunner::start_calibrate(const std::string& ref, const std::string& test,
                                PlerOptions opt) {
  {
    std::lock_guard<std::mutex> lock(mu_);
    status_ = JobStatus{};
    status_.state = JobState::Queued;
    status_.kind = JobKind::Calibrate;
    status_.message = "queued calibrate";
  }
  cancel_.store(false);

  std::thread([this, ref, test, opt]() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      status_.state = JobState::Running;
      status_.message = "calibrating";
    }
    auto prog = [this](float p, const std::string& msg) {
      std::lock_guard<std::mutex> lock(mu_);
      status_.progress = p;
      status_.message = msg;
    };
    CalibrateResult cr = calibrate_rays(ref, test, opt, prog, &cancel_);
    std::lock_guard<std::mutex> lock(mu_);
    status_.calibrate = cr;
    status_.kind = JobKind::Calibrate;
    status_.progress = cr.ok ? 1.f : status_.progress;
    if (cancel_.load()) {
      status_.state = JobState::Cancelled;
      status_.message = "cancelled";
    } else if (!cr.ok) {
      status_.state = JobState::Error;
      status_.message = cr.error;
    } else {
      status_.state = JobState::Done;
      status_.message =
          "calibrate done; suggested rays=" + std::to_string(cr.suggested_rays);
    }
  }).detach();
}

void JobRunner::cancel() { cancel_.store(true); }

JobStatus JobRunner::snapshot() const {
  std::lock_guard<std::mutex> lock(mu_);
  return status_;
}

}  // namespace pler
