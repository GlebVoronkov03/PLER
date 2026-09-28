"use strict";

/**
 * Fallback when N-API .node is missing: shell out to vendor pler CLI and parse stdout.
 * Primary path remains the N-API addon (built on CI / machines with a C++ toolchain).
 */
const { spawnSync } = require("child_process");
const fs = require("fs");
const path = require("path");
const { vendorDir, cliBinaryName } = require("../scripts/platform");

function parseStdout(text) {
  const out = {};
  for (const line of String(text).split(/\r?\n/)) {
    const m = line.match(
      /^(PLER|MSE|Peak|Mean err|Miss ref|Miss test|Rays|Backend|Align|Time|TSI):\s*(.+)$/
    );
    if (m) out[m[1]] = m[2].trim();
  }
  const pler = out.PLER ? parseFloat(out.PLER) : NaN;
  const mse = out.MSE ? parseFloat(out.MSE) : NaN;
  return {
    pler_db: pler,
    mse,
    mean_error: out["Mean err"] ? parseFloat(out["Mean err"]) : 0,
    max_error: 0,
    peak: out.Peak ? parseFloat(out.Peak) : 0,
    miss_rate_ref: 0,
    miss_rate_test: 0,
    num_rays: out.Rays ? parseInt(out.Rays, 10) : 0,
    computation_time_s: out.Time ? parseFloat(out.Time) : 0,
    tsi: out.TSI ? parseFloat(out.TSI) : -1,
    volume_iou: -1,
    ok: Number.isFinite(pler),
    backend: (out.Backend || "").split(/\s/)[0] || "",
    error: Number.isFinite(pler) ? "" : text.slice(0, 400),
    return_code: Number.isFinite(pler) ? 0 : 1,
  };
}

function loadCliBinding() {
  const bin = path.join(vendorDir(), cliBinaryName());
  if (!fs.existsSync(bin)) {
    throw new Error(`PLER CLI missing at ${bin}`);
  }
  return {
    loadNative() {
      return true;
    },
    versionString() {
      const r = spawnSync(bin, ["--version"], { encoding: "utf8" });
      return (r.stdout || "").trim() || "PLER";
    },
    versionMajor: () => 2,
    versionMinor: () => 0,
    versionPatch: () => 0,
    buildHasCuda() {
      const r = spawnSync(bin, ["--version"], { encoding: "utf8" });
      return /cuda=yes/i.test(r.stdout || "");
    },
    optionsInit() {
      return {
        num_rays: 5000,
        min_rays: 1000,
        max_rays: 20000,
        align_mode: 0,
        compute_tsi: 0,
        converge: 0,
        prefer_cuda: 1,
        voxel_resolution: 48,
        sphere_margin: 1.02,
        cache_dir: ".pler_cache",
      };
    },
    lastError: () => "",
    computeFiles(ref, test, options) {
      const args = [ref, test];
      const o = options || {};
      if (o.num_rays) args.push("--rays", String(o.num_rays));
      const alignMap = { 0: "off", 1: "iou", 2: "icp", 3: "iou+icp" };
      if (o.align_mode != null) args.push("--align", alignMap[o.align_mode] || "off");
      if (o.compute_tsi) args.push("--tsi");
      if (o.prefer_cuda === 0 || o.prefer_cuda === false) args.push("--cpu");
      const r = spawnSync(bin, args, { encoding: "utf8" });
      const parsed = parseStdout((r.stdout || "") + "\n" + (r.stderr || ""));
      if (r.status !== 0 && parsed.ok) parsed.ok = false;
      parsed.return_code = r.status == null ? 1 : r.status;
      return parsed;
    },
  };
}

module.exports = { loadCliBinding };
