#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawn } = require("node:child_process");
const { resolvePlatform, supportedTargets } = require("../lib/platform");
const { getBinaryPath } = require("../lib/paths");
const { buildDownloadUrl } = require("../lib/github");
const { downloadToFile } = require("../lib/downloader");
const { getRuntimeFailureHint } = require("../lib/runtime-hints");

function fail(message) {
  console.error("[bad] " + message);
  process.exit(1);
}

async function ensureBinary(binaryPath, options = {}) {
  if (fs.existsSync(binaryPath)) {
    return binaryPath;
  }

  if (options.isOverridePath) {
    fail(
      "BAD binary not found at BAD_BIN_PATH location: " +
        binaryPath +
        ". Update BAD_BIN_PATH to a valid executable path or unset it."
    );
  }

  if (process.env.BAD_SKIP_DOWNLOAD === "1") {
    fail(
      "BAD binary missing at " +
        binaryPath +
        ". BAD_SKIP_DOWNLOAD=1 is set, so auto-download is disabled. " +
        "Reinstall without BAD_SKIP_DOWNLOAD or run: npm rebuild badlang"
    );
  }

  let target;
  try {
    target = resolvePlatform();
  } catch (err) {
    fail(
      (err && err.message ? err.message : String(err)) +
        ". Supported targets: " +
        supportedTargets().join(", ")
    );
  }

  const owner = String(process.env.BAD_GITHUB_OWNER || "clixiya").trim();
  const repo = String(process.env.BAD_GITHUB_REPO || "badlang").trim();
  const tag = String(process.env.BAD_GITHUB_TAG || "latest").trim();
  const baseUrl = String(process.env.BAD_DOWNLOAD_BASE_URL || "").trim();
  const token = String(process.env.BAD_GITHUB_TOKEN || "").trim();

  const downloadUrl = buildDownloadUrl({
    owner,
    repo,
    tag,
    assetName: target.assetName,
    baseUrl
  });

  const headers = {};
  if (token) {
    headers.Authorization = "Bearer " + token;
  }

  console.log("[bad] Binary missing. Downloading " + target.assetName + "...");
  try {
    await downloadToFile(downloadUrl, binaryPath, { headers });
    if (target.platform !== "win32") {
      await fs.promises.chmod(binaryPath, 0o755);
    }
  } catch (err) {
    fail(
      "Failed to auto-download BAD binary from " +
        downloadUrl +
        ". " +
        (err && err.message ? err.message : String(err))
    );
  }

  return binaryPath;
}

function resolveBinaryPath() {
  const override = String(process.env.BAD_BIN_PATH || "").trim();
  if (override) {
    return {
      path: path.isAbsolute(override) ? override : path.resolve(process.cwd(), override),
      isOverridePath: true
    };
  }

  const target = resolvePlatform();
  return {
    path: getBinaryPath(target.executableName),
    isOverridePath: false
  };
}

async function main() {
  const resolved = resolveBinaryPath();
  const readyBinaryPath = await ensureBinary(resolved.path, {
    isOverridePath: resolved.isOverridePath
  });

  const child = spawn(readyBinaryPath, process.argv.slice(2), {
    stdio: "inherit"
  });

  child.on("error", (err) => {
    const message = "Failed to run BAD binary: " + (err && err.message ? err.message : String(err));
    const hint = getRuntimeFailureHint(process.platform, {
      code: err && err.code
    });
    fail(hint ? message + ". " + hint : message);
  });

  child.on("exit", (code) => {
    const hint = getRuntimeFailureHint(process.platform, {
      exitCode: code
    });
    if (hint) {
      console.error("[bad] " + hint);
    }
    process.exit(code == null ? 1 : code);
  });
}

main().catch((err) => {
  fail(err && err.message ? err.message : String(err));
});
