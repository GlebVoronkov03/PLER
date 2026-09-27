#pragma once

/**
 * PLER public C ABI (v2).
 * Link against pler.dll / libpler.so. C++ advanced API remains under pler/*.hpp.
 */

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_WIN32)
#  if defined(PLER_BUILD_SHARED)
#    define PLER_API __declspec(dllexport)
#  else
#    define PLER_API __declspec(dllimport)
#  endif
#else
#  define PLER_API __attribute__((visibility("default")))
#endif

enum pler_align_mode {
  PLER_ALIGN_OFF = 0,
  PLER_ALIGN_VOLUME_IOU = 1,
  PLER_ALIGN_ICP = 2,
  PLER_ALIGN_IOU_THEN_ICP = 3
};

typedef struct pler_options {
  int num_rays;          /* <=0 => adaptive */
  int min_rays;
  int max_rays;
  int align_mode;        /* pler_align_mode */
  int compute_tsi;       /* 0/1 */
  int converge;          /* 0/1 */
  int prefer_cuda;       /* 0/1 */
  int voxel_resolution;
  double sphere_margin;
  char cache_dir[512];
} pler_options;

typedef struct pler_result {
  double pler_db;
  double mse;
  double mean_error;
  double max_error;
  double peak;
  double miss_rate_ref;
  double miss_rate_test;
  int num_rays;
  double computation_time_s;
  double tsi;            /* <0 if not computed */
  double volume_iou;     /* <0 if unused */
  int ok;                /* 1 success */
  char backend[32];
  char error[512];
} pler_result;

PLER_API const char* pler_version_string(void);
PLER_API int pler_version_major(void);
PLER_API int pler_version_minor(void);
PLER_API int pler_version_patch(void);

/** 1 if this build was compiled with CUDA support (device may still be absent). */
PLER_API int pler_build_has_cuda(void);

/** Fill options with library defaults. */
PLER_API void pler_options_init(pler_options* opt);

/**
 * Compare two mesh files (OBJ/STL; Assimp formats if built with Assimp).
 * Returns 0 on success, non-zero on failure (see out->error / pler_last_error).
 */
PLER_API int pler_compute_files(const char* ref_path, const char* test_path,
                                const pler_options* opt, pler_result* out);

/** Thread-local last error message (never NULL). */
PLER_API const char* pler_last_error(void);

#ifdef __cplusplus
}
#endif
