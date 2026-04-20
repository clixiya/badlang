#!/usr/bin/env node
"use strict";

const fs = require("node:fs");
const path = require("node:path");
const { spawn } = require("node:child_process");
const { resolvePlatform, supportedTargets } = require("../lib/platform");
const { buildDownloadUrl } = require("../lib/github");
const { VENDOR_DIR, getBinaryPath } = require("../lib/paths");
const { downloadToFile } = require("../lib/downloader");
const { getRuntimeFailureHint } = require("../lib/runtime-hints");

function info(message) {
  console.log("[bad] " + message);
}

function fail(message) {
  console.error("[bad] " + message);
  process.exit(1);
}

function describeFailure(failure) {
  if (!failure) {
    return "unknown failure";
  }

  if (failure.message) {
    return failure.message;
  }

  if (failure.signal) {
    return "terminated by signal " + failure.signal;
  }

  if (Number.isInteger(failure.exitCode)) {
    return "exited with code " + failure.exitCode;
  }

  return "unknown failure";
}

async function smokeTestBinary(binaryPath) {
  return new Promise((resolve) => {
    let settled = false;
    const child = spawn(binaryPath, ["--help"], {
      stdio: "ignore",
      windowsHide: true
    });

    const finish = (value) => {
      if (settled) {
        return;
      }
      settled = true;
      resolve(value);
    };

    child.once("error", (err) => {
      finish({
        code: err && err.code,
        message: err && err.message ? err.message : String(err)
      });
    });

    child.once("exit", (exitCode, signal) => {
      if (exitCode === 0) {
        finish(null);
        return;
      }

      finish({
        exitCode,
        signal,
        message: signal ? "terminated by signal " + signal : "exited with code " + exitCode
      });
    });
  });
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

  const runtimeFiles =
    Array.isArray(target.runtimeFiles) && target.runtimeFiles.length
      ? target.runtimeFiles
      : [
          {
            assetName: target.assetName,
            destinationName: target.executableName
          }
        ];

  const executableRuntimeFile =
    runtimeFiles.find((item) => item.destinationName === target.executableName) || runtimeFiles[0];

  const downloadUrl = buildDownloadUrl({
    owner,
    repo,
    tag,
    assetName: executableRuntimeFile.assetName,
    baseUrl
  });

  const binaryPath = getBinaryPath(target.executableName);
  const missingRuntimeFiles = runtimeFiles.filter(
    (item) => !fs.existsSync(getBinaryPath(item.destinationName))
  );

  if (!force && fs.existsSync(binaryPath) && missingRuntimeFiles.length === 0) {
    const failure = await smokeTestBinary(binaryPath);
    if (!failure) {
      info("Binary already exists at " + binaryPath + ". Use BAD_FORCE_DOWNLOAD=1 to refresh.");
      return;
    }

    info(
      "Existing binary failed startup check (" +
        describeFailure(failure) +
        "). Re-downloading " +
        target.assetName +
        "..."
    );
  } else if (!force && fs.existsSync(binaryPath) && missingRuntimeFiles.length > 0) {
    info(
      "Runtime support files missing (" +
        missingRuntimeFiles.map((item) => item.destinationName).join(", ") +
        "). Re-downloading..."
    );
  }

  const headers = {};
  if (token) {
    headers.Authorization = "Bearer " + token;
  }

  for (const runtimeFile of runtimeFiles) {
    const runtimePath = getBinaryPath(runtimeFile.destinationName);
    const runtimeDownloadUrl = buildDownloadUrl({
      owner,
      repo,
      tag,
      assetName: runtimeFile.assetName,
      baseUrl
    });

    info("Downloading " + runtimeFile.assetName + " from GitHub releases...");
    await downloadToFile(runtimeDownloadUrl, runtimePath, { headers });

    if (target.platform !== "win32" && runtimeFile.destinationName === target.executableName) {
      await fs.promises.chmod(runtimePath, 0o755);
    }
  }

  const startupFailure = await smokeTestBinary(binaryPath);
  if (startupFailure) {
    const hint = getRuntimeFailureHint(target.platform, startupFailure);
    fail(
      "Downloaded binary failed startup check (" +
        describeFailure(startupFailure) +
        "). " +
        (hint
          ? hint + " "
          : "") +
        "Set BAD_BIN_PATH to a local build if release binaries are not compatible with this system."
    );
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
    runtimeFiles,
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
