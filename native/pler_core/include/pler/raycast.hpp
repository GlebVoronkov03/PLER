#pragma once
#include "pler/frame.hpp"
#include "pler/types.hpp"
#include <atomic>
#include <functional>
#include <vector>

namespace pler {

enum class Backend { CPU, CUDA };

using CastProgress = std::function<void(float)>;

/** Cast rays; returns t_hit. Misses encoded as +inf before scoring. */
std::vector<double> cast_rays(const Mesh& mesh,
                              const RayBatch& rays,
                              Backend backend,
                              std::atomic<bool>* cancel = nullptr,
                              CastProgress progress = nullptr);

bool cuda_available();
Backend select_backend(bool prefer_cuda);

double compare_backends(const Mesh& mesh, const RayBatch& rays);

}  // namespace pler
