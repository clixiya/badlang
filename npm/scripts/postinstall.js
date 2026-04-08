#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { resolvePlatform, supportedTargets } = require("../lib/platform");
const { buildDownloadUrl } = require("../lib/github");
const { VENDOR_DIR, getBinaryPath } = require("../lib/paths");
const { downloadToFile } = require("../lib/downloader");

function info(message) {
  console.log("[bad] " + message);
}

function fail(message) {
  console.error("[bad] " + message);
  process.exit(1);
}

async function main() {
  if (process.env.BAD_SKIP_DOWNLOAD === "1") {
    info("Skipping binary download because BAD_SKIP_DOWNLOAD=1");
    return;
  }

  let target;
  try {
    target = resolvePlatform();
  } catch (err) {
    fail(
      err.message +
        ". Supported targets: " +
        supportedTargets().join(", ")
    );
  }

  const owner = String(process.env.BAD_GITHUB_OWNER || "clixiya").trim();
  const repo = String(process.env.BAD_GITHUB_REPO || "badlang").trim();
  const tag = String(process.env.BAD_GITHUB_TAG || "latest").trim();
  const baseUrl = String(process.env.BAD_DOWNLOAD_BASE_URL || "").trim();
  const token = String(process.env.BAD_GITHUB_TOKEN || "").trim();
  const force = process.env.BAD_FORCE_DOWNLOAD === "1";

  const downloadUrl = buildDownloadUrl({
    owner,
    repo,
    tag,
    assetName: target.assetName,
    baseUrl
  });

  const binaryPath = getBinaryPath(target.executableName);

  if (!force && fs.existsSync(binaryPath)) {
    info("Binary already exists at " + binaryPath + ". Use BAD_FORCE_DOWNLOAD=1 to refresh.");
    return;
  }

  const headers = {};
  if (token) {
    headers.Authorization = "Bearer " + token;
  }

  info("Downloading " + target.assetName + " from GitHub releases...");
  await downloadToFile(downloadUrl, binaryPath, { headers });

  if (target.platform !== "win32") {
    await fs.promises.chmod(binaryPath, 0o755);
  }

  await fs.promises.mkdir(VENDOR_DIR, { recursive: true });
  const manifestPath = path.join(VENDOR_DIR, "install.json");
  const manifest = {
    owner,
    repo,
    tag,
    downloadUrl,
    platform: target.platform,
    arch: target.arch,
    assetName: target.assetName,
    executableName: target.executableName,
    installedAt: new Date().toISOString()
  };
  await fs.promises.writeFile(manifestPath, JSON.stringify(manifest, null, 2) + "\n", "utf8");

  info("Installed BAD binary: " + binaryPath);
}

if (require.main === module) {
  main().catch((err) => {
    fail(err && err.message ? err.message : String(err));
  });
}

module.exports = {
  main
};
