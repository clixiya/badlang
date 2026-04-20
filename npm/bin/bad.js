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
  const binaryExists = fs.existsSync(binaryPath);

  if (options.isOverridePath) {
    if (binaryExists) {
      return binaryPath;
    }

    fail(
      "BAD binary not found at BAD_BIN_PATH location: " +
        binaryPath +
        ". Update BAD_BIN_PATH to a valid executable path or unset it."
    );
  }

  let target = options.target;
  try {
    if (!target) {
      target = resolvePlatform();
    }
  } catch (err) {
    fail(
      (err && err.message ? err.message : String(err)) +
        ". Supported targets: " +
        supportedTargets().join(", ")
    );
  }

  const runtimeFiles =
    Array.isArray(target.runtimeFiles) && target.runtimeFiles.length
      ? target.runtimeFiles
      : [
          {
            assetName: target.assetName,
            destinationName: target.executableName
          }
        ];

  const missingRuntimeFiles = runtimeFiles.filter(
    (item) => !fs.existsSync(getBinaryPath(item.destinationName))
  );

  if (binaryExists && missingRuntimeFiles.length === 0) {
    return binaryPath;
  }

  if (process.env.BAD_SKIP_DOWNLOAD === "1") {
    const missing = missingRuntimeFiles.map((item) => item.destinationName).join(", ");
    fail(
      "BAD binary missing at " +
        binaryPath +
        (missing ? " or required runtime files are missing (" + missing + ")" : "") +
        ". BAD_SKIP_DOWNLOAD=1 is set, so auto-download is disabled. " +
        "Reinstall without BAD_SKIP_DOWNLOAD or run: npm rebuild badlang"
    );
  }

  const owner = String(process.env.BAD_GITHUB_OWNER || "clixiya").trim();
  const repo = String(process.env.BAD_GITHUB_REPO || "badlang").trim();
  const tag = String(process.env.BAD_GITHUB_TAG || "latest").trim();
  const baseUrl = String(process.env.BAD_DOWNLOAD_BASE_URL || "").trim();
  const token = String(process.env.BAD_GITHUB_TOKEN || "").trim();

  const headers = {};
  if (token) {
    headers.Authorization = "Bearer " + token;
  }

  if (binaryExists && missingRuntimeFiles.length > 0) {
    console.log(
      "[bad] Runtime support files missing (" +
        missingRuntimeFiles.map((item) => item.destinationName).join(", ") +
        "). Downloading required files..."
    );
  } else {
    console.log("[bad] Binary missing. Downloading " + target.assetName + "...");
  }

  try {
    for (const runtimeFile of runtimeFiles) {
      const runtimeDownloadUrl = buildDownloadUrl({
        owner,
        repo,
        tag,
        assetName: runtimeFile.assetName,
        baseUrl
      });

      const destinationPath =
        runtimeFile.destinationName === target.executableName
          ? binaryPath
          : getBinaryPath(runtimeFile.destinationName);

      await downloadToFile(runtimeDownloadUrl, destinationPath, { headers });
      if (target.platform !== "win32" && runtimeFile.destinationName === target.executableName) {
        await fs.promises.chmod(destinationPath, 0o755);
      }
    }
  } catch (err) {
    const downloadTarget =
      err && err.message && /status\s+\d+/i.test(err.message)
        ? "required runtime assets"
        : "BAD binary";
    fail(
      "Failed to auto-download " +
        downloadTarget +
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
      isOverridePath: true,
      target: null
    };
  }

  const target = resolvePlatform();
  return {
    path: getBinaryPath(target.executableName),
    isOverridePath: false,
    target
  };
}

async function main() {
  const resolved = resolveBinaryPath();
  const readyBinaryPath = await ensureBinary(resolved.path, {
    isOverridePath: resolved.isOverridePath,
    target: resolved.target
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
