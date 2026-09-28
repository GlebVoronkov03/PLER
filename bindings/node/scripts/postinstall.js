"use strict";

/**
 * Download platform natives from GitHub Release v2.0.0 into vendor/<platform>/.
 * Prefers CUDA Linux asset when present; Windows zip may already include cudart.
 */
const fs = require("fs");
const path = require("path");
const https = require("https");
const { execFileSync } = require("child_process");
const {
  platformKey,
  vendorDir,
  sharedLibraryName,
  cliBinaryName,
  releaseAssetName,
} = require("./platform");

const TAG = process.env.PLER_RELEASE_TAG || "v2.0.0";
const REPO = process.env.PLER_GITHUB_REPO || "GlebVoronkov03/PLER";

function httpGetJson(url) {
  return new Promise((resolve, reject) => {
    https
      .get(url, { headers: { "User-Agent": "pler-metric-postinstall" } }, (res) => {
        if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
          httpGetJson(res.headers.location).then(resolve, reject);
          return;
        }
        let data = "";
        res.on("data", (c) => (data += c));
        res.on("end", () => {
          try {
            resolve(JSON.parse(data));
          } catch (e) {
            reject(e);
          }
        });
      })
      .on("error", reject);
  });
}

function downloadFile(url, dest) {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(dest);
    https
      .get(url, { headers: { "User-Agent": "pler-metric-postinstall" } }, (res) => {
        if (res.statusCode >= 300 && res.statusCode < 400 && res.headers.location) {
          file.close();
          fs.unlinkSync(dest);
          downloadFile(res.headers.location, dest).then(resolve, reject);
          return;
        }
        if (res.statusCode !== 200) {
          reject(new Error(`Download failed ${res.statusCode} ${url}`));
          return;
        }
        res.pipe(file);
        file.on("finish", () => file.close(() => resolve()));
      })
      .on("error", reject);
  });
}

function findFile(root, name) {
  const stack = [root];
  while (stack.length) {
    const d = stack.pop();
    for (const ent of fs.readdirSync(d, { withFileTypes: true })) {
      const p = path.join(d, ent.name);
      if (ent.isDirectory()) stack.push(p);
      else if (ent.name === name) return p;
    }
  }
  return null;
}

async function main() {
  const dest = vendorDir();
  const libName = sharedLibraryName();
  const cliName = cliBinaryName();
  if (fs.existsSync(path.join(dest, libName)) && fs.existsSync(path.join(dest, cliName))) {
    console.log(`pler-metric: vendor already present (${platformKey()})`);
    return;
  }

  console.log(`pler-metric: fetching ${TAG} natives for ${platformKey()}...`);
  const release = await httpGetJson(`https://api.github.com/repos/${REPO}/releases/tags/${TAG}`);
  const names = (release.assets || []).map((a) => a.name);
  const assetName = releaseAssetName(true, names);
  const asset = (release.assets || []).find((a) => a.name === assetName);
  if (!asset) throw new Error(`Release asset not found: ${assetName}`);

  const tmp = fs.mkdtempSync(path.join(require("os").tmpdir(), "pler-npm-"));
  const archive = path.join(tmp, assetName);
  await downloadFile(asset.browser_download_url, archive);

  const extractDir = path.join(tmp, "x");
  fs.mkdirSync(extractDir, { recursive: true });
  if (assetName.endsWith(".tar.gz")) {
    execFileSync("tar", ["-xzf", archive, "-C", extractDir], { stdio: "inherit" });
  } else {
    if (process.platform === "win32") {
      execFileSync(
        "powershell",
        [
          "-NoProfile",
          "-Command",
          `Expand-Archive -Path '${archive.replace(/'/g, "''")}' -DestinationPath '${extractDir.replace(/'/g, "''")}' -Force`,
        ],
        { stdio: "inherit" }
      );
    } else {
      execFileSync("unzip", ["-qo", archive, "-d", extractDir], { stdio: "inherit" });
    }
  }

  fs.mkdirSync(dest, { recursive: true });
  const cli = findFile(extractDir, cliName);
  const lib = findFile(extractDir, libName);
  if (!cli || !lib) throw new Error(`Could not find ${cliName}/${libName} in ${assetName}`);
  fs.copyFileSync(cli, path.join(dest, cliName));
  fs.copyFileSync(lib, path.join(dest, libName));
  if (process.platform !== "win32") {
    try {
      fs.chmodSync(path.join(dest, cliName), 0o755);
    } catch (_) {}
  }
  const binDir = path.dirname(cli);
  for (const f of fs.readdirSync(binDir)) {
    if (/^cudart64_.*\.dll$/i.test(f) || /^libcudart\.so/.test(f)) {
      fs.copyFileSync(path.join(binDir, f), path.join(dest, f));
    }
  }
  console.log(`pler-metric: staged vendor/${platformKey()}`);
}

module.exports = { main };

if (require.main === module) {
  main().catch((err) => {
    console.error("pler-metric postinstall warning:", err.message || err);
    process.exitCode = 0;
  });
}
