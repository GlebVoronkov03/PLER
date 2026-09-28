#include <napi.h>

#include <cstring>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

enum pler_align_mode {
  PLER_ALIGN_OFF = 0,
  PLER_ALIGN_VOLUME_IOU = 1,
  PLER_ALIGN_ICP = 2,
  PLER_ALIGN_IOU_THEN_ICP = 3
};

struct pler_options {
  int num_rays;
  int min_rays;
  int max_rays;
  int align_mode;
  int compute_tsi;
  int converge;
  int prefer_cuda;
  int voxel_resolution;
  double sphere_margin;
  char cache_dir[512];
};

struct pler_result {
  double pler_db;
  double mse;
  double mean_error;
  double max_error;
  double peak;
  double miss_rate_ref;
  double miss_rate_test;
  int num_rays;
  double computation_time_s;
  double tsi;
  double volume_iou;
  int ok;
  char backend[32];
  char error[512];
};

using pler_version_string_fn = const char* (*)();
using pler_version_part_fn = int (*)();
using pler_build_has_cuda_fn = int (*)();
using pler_options_init_fn = void (*)(pler_options*);
using pler_compute_files_fn = int (*)(const char*, const char*, const pler_options*,
                                      pler_result*);
using pler_last_error_fn = const char* (*)();

struct Lib {
#ifdef _WIN32
  HMODULE handle = nullptr;
#else
  void* handle = nullptr;
#endif
  pler_version_string_fn version_string = nullptr;
  pler_version_part_fn version_major = nullptr;
  pler_version_part_fn version_minor = nullptr;
  pler_version_part_fn version_patch = nullptr;
  pler_build_has_cuda_fn build_has_cuda = nullptr;
  pler_options_init_fn options_init = nullptr;
  pler_compute_files_fn compute_files = nullptr;
  pler_last_error_fn last_error = nullptr;
};

Lib g_lib;

void* Sym(const char* name) {
#ifdef _WIN32
  return reinterpret_cast<void*>(GetProcAddress(g_lib.handle, name));
#else
  return dlsym(g_lib.handle, name);
#endif
}

bool LoadLibraryAt(const std::string& path, std::string* err) {
  if (g_lib.handle) return true;
#ifdef _WIN32
  g_lib.handle = LoadLibraryA(path.c_str());
  if (!g_lib.handle) {
    *err = "LoadLibrary failed for " + path;
    return false;
  }
#else
  g_lib.handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!g_lib.handle) {
    *err = std::string("dlopen failed: ") + (dlerror() ? dlerror() : path);
    return false;
  }
#endif
  g_lib.version_string = reinterpret_cast<pler_version_string_fn>(Sym("pler_version_string"));
  g_lib.version_major = reinterpret_cast<pler_version_part_fn>(Sym("pler_version_major"));
  g_lib.version_minor = reinterpret_cast<pler_version_part_fn>(Sym("pler_version_minor"));
  g_lib.version_patch = reinterpret_cast<pler_version_part_fn>(Sym("pler_version_patch"));
  g_lib.build_has_cuda = reinterpret_cast<pler_build_has_cuda_fn>(Sym("pler_build_has_cuda"));
  g_lib.options_init = reinterpret_cast<pler_options_init_fn>(Sym("pler_options_init"));
  g_lib.compute_files = reinterpret_cast<pler_compute_files_fn>(Sym("pler_compute_files"));
  g_lib.last_error = reinterpret_cast<pler_last_error_fn>(Sym("pler_last_error"));
  if (!g_lib.version_string || !g_lib.options_init || !g_lib.compute_files) {
    *err = "PLER symbols missing in " + path;
    return false;
  }
  return true;
}

Napi::Value LoadNative(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (info.Length() < 1 || !info[0].IsString()) {
    Napi::TypeError::New(env, "loadNative(path) requires a string").ThrowAsJavaScriptException();
    return env.Null();
  }
  std::string path = info[0].As<Napi::String>().Utf8Value();
  std::string err;
  if (!LoadLibraryAt(path, &err)) {
    Napi::Error::New(env, err).ThrowAsJavaScriptException();
    return env.Null();
  }
  return Napi::Boolean::New(env, true);
}

bool EnsureLoaded(Napi::Env env) {
  if (g_lib.handle) return true;
  Napi::Error::New(env, "Native library not loaded; call loadNative(path) first or run postinstall")
      .ThrowAsJavaScriptException();
  return false;
}

Napi::Object OptionsToJs(Napi::Env env, const pler_options& o) {
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("num_rays", o.num_rays);
  obj.Set("min_rays", o.min_rays);
  obj.Set("max_rays", o.max_rays);
  obj.Set("align_mode", o.align_mode);
  obj.Set("compute_tsi", o.compute_tsi);
  obj.Set("converge", o.converge);
  obj.Set("prefer_cuda", o.prefer_cuda);
  obj.Set("voxel_resolution", o.voxel_resolution);
  obj.Set("sphere_margin", o.sphere_margin);
  obj.Set("cache_dir", Napi::String::New(env, o.cache_dir));
  return obj;
}

void OptionsFromJs(const Napi::Object& obj, pler_options* o) {
  if (obj.Has("num_rays")) o->num_rays = obj.Get("num_rays").As<Napi::Number>().Int32Value();
  if (obj.Has("min_rays")) o->min_rays = obj.Get("min_rays").As<Napi::Number>().Int32Value();
  if (obj.Has("max_rays")) o->max_rays = obj.Get("max_rays").As<Napi::Number>().Int32Value();
  if (obj.Has("align_mode")) o->align_mode = obj.Get("align_mode").As<Napi::Number>().Int32Value();
  if (obj.Has("compute_tsi"))
    o->compute_tsi = obj.Get("compute_tsi").As<Napi::Number>().Int32Value();
  if (obj.Has("converge")) o->converge = obj.Get("converge").As<Napi::Number>().Int32Value();
  if (obj.Has("prefer_cuda"))
    o->prefer_cuda = obj.Get("prefer_cuda").As<Napi::Number>().Int32Value();
  if (obj.Has("voxel_resolution"))
    o->voxel_resolution = obj.Get("voxel_resolution").As<Napi::Number>().Int32Value();
  if (obj.Has("sphere_margin"))
    o->sphere_margin = obj.Get("sphere_margin").As<Napi::Number>().DoubleValue();
  if (obj.Has("cache_dir") && obj.Get("cache_dir").IsString()) {
    std::string s = obj.Get("cache_dir").As<Napi::String>().Utf8Value();
    std::memset(o->cache_dir, 0, sizeof(o->cache_dir));
    std::strncpy(o->cache_dir, s.c_str(), sizeof(o->cache_dir) - 1);
  }
}

Napi::Object ResultToJs(Napi::Env env, const pler_result& r) {
  Napi::Object obj = Napi::Object::New(env);
  obj.Set("pler_db", r.pler_db);
  obj.Set("mse", r.mse);
  obj.Set("mean_error", r.mean_error);
  obj.Set("max_error", r.max_error);
  obj.Set("peak", r.peak);
  obj.Set("miss_rate_ref", r.miss_rate_ref);
  obj.Set("miss_rate_test", r.miss_rate_test);
  obj.Set("num_rays", r.num_rays);
  obj.Set("computation_time_s", r.computation_time_s);
  obj.Set("tsi", r.tsi);
  obj.Set("volume_iou", r.volume_iou);
  obj.Set("ok", r.ok != 0);
  obj.Set("backend", Napi::String::New(env, r.backend));
  obj.Set("error", Napi::String::New(env, r.error));
  return obj;
}

Napi::Value VersionString(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  const char* s = g_lib.version_string();
  return Napi::String::New(env, s ? s : "");
}

Napi::Value VersionMajor(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  return Napi::Number::New(env, g_lib.version_major ? g_lib.version_major() : 0);
}

Napi::Value VersionMinor(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  return Napi::Number::New(env, g_lib.version_minor ? g_lib.version_minor() : 0);
}

Napi::Value VersionPatch(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  return Napi::Number::New(env, g_lib.version_patch ? g_lib.version_patch() : 0);
}

Napi::Value BuildHasCuda(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  return Napi::Boolean::New(env, g_lib.build_has_cuda ? g_lib.build_has_cuda() != 0 : false);
}

Napi::Value OptionsInit(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  pler_options o{};
  g_lib.options_init(&o);
  return OptionsToJs(env, o);
}

Napi::Value LastError(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  const char* s = g_lib.last_error ? g_lib.last_error() : "";
  return Napi::String::New(env, s ? s : "");
}

Napi::Value ComputeFiles(const Napi::CallbackInfo& info) {
  Napi::Env env = info.Env();
  if (!EnsureLoaded(env)) return env.Null();
  if (info.Length() < 2 || !info[0].IsString() || !info[1].IsString()) {
    Napi::TypeError::New(env, "computeFiles(ref, test, options?)").ThrowAsJavaScriptException();
    return env.Null();
  }
  std::string ref = info[0].As<Napi::String>().Utf8Value();
  std::string test = info[1].As<Napi::String>().Utf8Value();
  pler_options o{};
  g_lib.options_init(&o);
  if (info.Length() >= 3 && info[2].IsObject()) {
    OptionsFromJs(info[2].As<Napi::Object>(), &o);
  }
  pler_result out{};
  int rc = g_lib.compute_files(ref.c_str(), test.c_str(), &o, &out);
  Napi::Object js = ResultToJs(env, out);
  js.Set("return_code", rc);
  return js;
}

Napi::Object Init(Napi::Env env, Napi::Object exports) {
  exports.Set("loadNative", Napi::Function::New(env, LoadNative));
  exports.Set("versionString", Napi::Function::New(env, VersionString));
  exports.Set("versionMajor", Napi::Function::New(env, VersionMajor));
  exports.Set("versionMinor", Napi::Function::New(env, VersionMinor));
  exports.Set("versionPatch", Napi::Function::New(env, VersionPatch));
  exports.Set("buildHasCuda", Napi::Function::New(env, BuildHasCuda));
  exports.Set("optionsInit", Napi::Function::New(env, OptionsInit));
  exports.Set("lastError", Napi::Function::New(env, LastError));
  exports.Set("computeFiles", Napi::Function::New(env, ComputeFiles));
  exports.Set("AlignMode", Napi::Object::New(env));
  Napi::Object am = exports.Get("AlignMode").As<Napi::Object>();
  am.Set("OFF", 0);
  am.Set("VOLUME_IOU", 1);
  am.Set("ICP", 2);
  am.Set("IOU_THEN_ICP", 3);
  return exports;
}

}  // namespace

NODE_API_MODULE(pler_napi, Init)
