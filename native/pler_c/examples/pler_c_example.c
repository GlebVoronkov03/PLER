#include "pler.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void make_unit_tetra(float** out_v, size_t* nv, uint32_t** out_f,
                            size_t* nf) {
  static const float V[] = {
      1.f, 1.f, 1.f,  -1.f, -1.f, 1.f,  -1.f, 1.f, -1.f,  1.f, -1.f, -1.f,
  };
  static const uint32_t F[] = {0, 1, 2, 0, 3, 1, 0, 2, 3, 1, 3, 2};
  *nv = 4;
  *nf = 4;
  *out_v = (float*)malloc(sizeof(V));
  *out_f = (uint32_t*)malloc(sizeof(F));
  memcpy(*out_v, V, sizeof(V));
  memcpy(*out_f, F, sizeof(F));
}

int main(int argc, char** argv) {
  printf("PLER C ABI %s (cuda_build=%s)\n", pler_version_string(),
         pler_build_has_cuda() ? "yes" : "no");

  if (argc >= 3) {
    const char* ref = argv[1];
    const char* test = argv[2];
    pler_options opt;
    pler_options_init(&opt);
    opt.num_rays = 1000;
    opt.prefer_cuda = 0;
    pler_result out;
    int rc = pler_compute_files(ref, test, &opt, &out);
    if (rc != 0) {
      fprintf(stderr, "FAIL files rc=%d err=%s\n", rc, pler_last_error());
      return 1;
    }
    printf("files ok PLER=%.4f raw=%.4f MSE=%.6g rays=%d backend=%s\n",
           out.pler_db, out.pler_db_raw, out.mse, out.num_rays, out.backend);
    return 0;
  }

  /* In-memory API smoke: identical tetrahedron → capped 100, raw +inf/huge */
  float* v = NULL;
  uint32_t* f = NULL;
  size_t nv = 0, nf = 0;
  make_unit_tetra(&v, &nv, &f, &nf);

  pler_options opt;
  pler_options_init(&opt);
  opt.num_rays = 512;
  opt.prefer_cuda = 0;

  pler_reference* ref = NULL;
  int rc = pler_reference_create(v, nv, f, nf, &opt, &ref);
  if (rc != 0 || !ref) {
    fprintf(stderr, "FAIL reference_create rc=%d err=%s\n", rc, pler_last_error());
    free(v);
    free(f);
    return 1;
  }

  pler_result out;
  rc = pler_compare_mesh(ref, v, nv, f, nf, &out);
  pler_reference_destroy(ref);
  free(v);
  free(f);
  if (rc != 0 || !out.ok) {
    fprintf(stderr, "FAIL compare_mesh rc=%d err=%s\n", rc, pler_last_error());
    return 1;
  }
  printf("memory ok PLER=%.4f raw=%g MSE=%.6g rays=%d backend=%s\n", out.pler_db,
         out.pler_db_raw, out.mse, out.num_rays, out.backend);
  if (out.pler_db < 99.0) {
    fprintf(stderr, "expected near-identical PLER~100, got %.4f\n", out.pler_db);
    return 2;
  }
  return 0;
}
