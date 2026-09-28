"use strict";

const os = require("os");
const path = require("path");

function platformKey() {
  const system = os.platform();
  const arch = os.arch();
  if (system === "win32") return "windows-x64";
  if (system === "linux") return "linux-x64";
  if (system === "darwin") {
    return arch === "arm64" ? "macos-arm64" : "macos-x64";
  }
  throw new Error(`Unsupported platform: ${system} ${arch}`);
}

function vendorDir() {
  return path.join(__dirname, "..", "vendor", platformKey());
}

function sharedLibraryName() {
  const key = platformKey();
  if (key.startsWith("windows")) return "pler.dll";
  if (key.startsWith("macos")) return "libpler.dylib";
  return "libpler.so";
}

function cliBinaryName() {
  return platformKey().startsWith("windows") ? "pler.exe" : "pler";
}

function releaseAssetName(preferCuda, availableNames) {
  const key = platformKey();
  if (key === "windows-x64") return "pler-2.0-win64.zip";
  if (key === "linux-x64") {
    const cuda = "pler-2.0-linux-x64-cuda.tar.gz";
    if (preferCuda && availableNames && availableNames.includes(cuda)) return cuda;
    return "pler-2.0-linux-x64.tar.gz";
  }
  if (key === "macos-arm64") return "pler-2.0-macos-arm64.zip";
  if (key === "macos-x64") return "pler-2.0-macos-x64.zip";
  throw new Error(`No asset mapping for ${key}`);
}

module.exports = {
  platformKey,
  vendorDir,
  sharedLibraryName,
  cliBinaryName,
  releaseAssetName,
};
