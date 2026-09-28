"use strict";

const fs = require("fs");
const path = require("path");
const { platformKey, vendorDir, sharedLibraryName, cliBinaryName } = require("./scripts/platform");

let binding = null;
try {
  binding = require("./build/Release/pler_napi.node");
} catch (_) {
  binding = null;
}

function ensureNativeLoaded() {
  if (!binding) {
    const { loadCliBinding } = require("./lib/cli_binding");
    binding = loadCliBinding();
  }
  if (binding && typeof binding.loadNative === "function") {
    // N-API addon: load shared library from vendor/
    try {
      const lib = path.join(vendorDir(), sharedLibraryName());
      if (fs.existsSync(lib)) binding.loadNative(lib);
    } catch (_) {
      /* CLI fallback has no-op loadNative */
    }
  }
  if (!binding) {
    throw new Error("pler-metric binding unavailable");
  }
  return binding;
}

function withBinding(fn) {
  return fn(ensureNativeLoaded());
}

const AlignMode = {
  OFF: 0,
  VOLUME_IOU: 1,
  ICP: 2,
  IOU_THEN_ICP: 3,
};

module.exports = {
  AlignMode,
  platformKey,
  vendorDir,
  cliPath() {
    return path.join(vendorDir(), cliBinaryName());
  },
  versionString() {
    return withBinding((b) => b.versionString());
  },
  versionMajor() {
    return withBinding((b) => b.versionMajor());
  },
  versionMinor() {
    return withBinding((b) => b.versionMinor());
  },
  versionPatch() {
    return withBinding((b) => b.versionPatch());
  },
  buildHasCuda() {
    return withBinding((b) => b.buildHasCuda());
  },
  optionsInit() {
    return withBinding((b) => b.optionsInit());
  },
  lastError() {
    return withBinding((b) => b.lastError());
  },
  computeFiles(ref, test, options) {
    return withBinding((b) => b.computeFiles(ref, test, options));
  },
};
