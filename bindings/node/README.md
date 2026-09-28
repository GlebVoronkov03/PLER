# pler-metric (Node)

npm package: **N-API** addon that `dlopen`s `libpler` / `pler.dll` downloaded from the [v2.0.0 Release](https://github.com/GlebVoronkov03/PLER/releases/tag/v2.0.0) on `postinstall`.

```bash
npm install pler-metric
```

```js
const pler = require("pler-metric");

const opt = pler.optionsInit();
opt.num_rays = 2000;
opt.prefer_cuda = 1; // CPU fallback if no GPU
const r = pler.computeFiles("ref.obj", "test.obj", opt);
console.log(r.pler_db, r.mse, r.backend, r.ok);
```

```bash
npx pler --version
npx pler selftest
```

CUDA-capable Linux Release assets are preferred when present; Windows zip may include cudart. Without a GPU, PLER falls back to CPU.

If `node-gyp` cannot build the N-API addon (no C++ toolchain), the package falls back to shelling out to the downloaded `pler` CLI with the same JS API. CI builds the real N-API binary.

Local build from this repo:

```bash
cd bindings/node
npm install
npm test
```
