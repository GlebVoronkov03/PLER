#include "pler.h"

#include <stdio.h>
#include <string.h>

int main(int argc, char** argv) {
  printf("PLER C ABI %s (cuda_build=%s)\n", pler_version_string(),
         pler_build_has_cuda() ? "yes" : "no");

  const char* ref = argc > 1 ? argv[1] : "../../samples/ref_unit_sphere.obj";
  const char* test = argc > 2 ? argv[2] : "../../samples/test_unit_sphere_lod.obj";

  pler_options opt;
  pler_options_init(&opt);
  opt.num_rays = 1000;
  opt.prefer_cuda = 1;

  pler_result out;
  int rc = pler_compute_files(ref, test, &opt, &out);
  if (rc != 0) {
    fprintf(stderr, "FAIL rc=%d err=%s\n", rc, pler_last_error());
    return 1;
  }
  printf("ok PLER=%.4f dB MSE=%.6g rays=%d backend=%s time=%.3fs\n", out.pler_db,
         out.mse, out.num_rays, out.backend, out.computation_time_s);
  return 0;
}
