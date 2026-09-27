#pragma once
#include "pler/types.hpp"
#include <vector>

namespace pler {

struct LengthStats {
  std::vector<double> L;
  int miss_count = 0;
};

/** Convert t_hit to L; miss -> R. */
LengthStats hits_to_lengths(const std::vector<double>& t_hit, double R);

PlerResult score_pler(const LengthStats& ref,
                      const LengthStats& test,
                      double R,
                      int num_rays);

}  // namespace pler
