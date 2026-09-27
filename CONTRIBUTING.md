# Contributing to PLER

Thanks for your interest in improving PLER.

## Contact

- Author: Gleb Alekseevich Voronkov
- Email: glebvoronkov03@gmail.com

## Development setup

1. Install CMake ≥ 3.20 and a C++17 toolchain (MSVC 2017+ on Windows).
2. Optional: CUDA Toolkit with `nvcc` for the GPU BVH path.
3. Build:

```bat
cd native
build.bat
```

4. Run `native\build\pler_cli\Release\pler.exe selftest`.

See [docs/DEVELOPER_GUIDE.md](docs/DEVELOPER_GUIDE.md) for architecture and the C / C++ APIs.

## Pull requests

- Keep the PLER-2.0 score path unchanged unless the PR is explicitly about the metric definition.
- Prefer small, focused changes with a short description of *why*.
- Do not commit `native/build/`, `dist/`, `.pler_cache/`, or `pler_env/`.
- Run `pler selftest` before opening a PR when you touch raycast, scoring, or the C ABI.

## Code style

Match existing C++17 style in `native/pler_core`. Public C ABI lives in `native/pler_c/include/pler.h` and must stay ABI-stable within a major version.
