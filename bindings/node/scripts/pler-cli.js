#!/usr/bin/env node
"use strict";

const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");
const { vendorDir, cliBinaryName } = require("./platform");

const bin = path.join(vendorDir(), cliBinaryName());
if (!fs.existsSync(bin)) {
  console.error(`pler CLI not found at ${bin}. Re-run npm install (postinstall downloads Release binaries).`);
  process.exit(1);
}
const r = spawnSync(bin, process.argv.slice(2), { stdio: "inherit" });
process.exit(r.status == null ? 1 : r.status);
