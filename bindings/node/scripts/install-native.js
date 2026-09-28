"use strict";
const { spawnSync } = require("child_process");
const r = spawnSync("node-gyp", ["rebuild"], { stdio: "inherit", shell: true });
if (r.status !== 0) {
  console.warn("pler-metric: native addon build failed; CLI may still work after postinstall");
}
