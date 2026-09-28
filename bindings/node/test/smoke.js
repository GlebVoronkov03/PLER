"use strict";

const path = require("path");
const { main: fetchVendor } = require("../scripts/postinstall");
const pler = require("../index");

const root = path.resolve(__dirname, "..", "..", "..");
const ref = path.join(root, "samples", "ref_unit_sphere.obj");
const test = path.join(root, "samples", "test_unit_sphere_lod.obj");

async function main() {
  await fetchVendor();
  const opt = pler.optionsInit();
  opt.num_rays = 1000;
  opt.prefer_cuda = 0;
  const r = pler.computeFiles(ref, test, opt);
  if (!r.ok) {
    console.error(r);
    process.exit(1);
  }
  if (!Number.isFinite(r.pler_db)) {
    console.error("non-finite pler_db", r);
    process.exit(1);
  }
  console.log("smoke ok", r.pler_db, r.backend);
}

main().catch((e) => {
  console.error(e);
  process.exit(1);
});
