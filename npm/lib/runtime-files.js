"use strict";

const { buildDownloadUrl } = require("./github");

function normalizeRuntimeFile(file) {
  if (!file || typeof file !== "object") {
    return null;
  }

  const assetName = String(file.assetName || "").trim();
  const destinationName = String(file.destinationName || "").trim();
  if (!assetName || !destinationName) {
    return null;
  }

  return {
    assetName,
    destinationName
  };
}

function dedupeRuntimeFiles(runtimeFiles) {
  const deduped = [];
  const seenDestinationNames = new Set();

  for (const file of runtimeFiles) {
    const normalized = normalizeRuntimeFile(file);
    if (!normalized) {
      continue;
    }

    if (seenDestinationNames.has(normalized.destinationName)) {
      continue;
    }

    seenDestinationNames.add(normalized.destinationName);
    deduped.push(normalized);
  }

  return deduped;
}

async function fetchRuntimeManifestFiles(target, downloadOptions, requestOptions = {}) {
  if (!target || !target.runtimeManifestAsset) {
    return [];
  }

  if (typeof fetch !== "function") {
    throw new Error("Global fetch is not available. Use Node.js 18.17+.");
  }

  const manifestUrl = buildDownloadUrl({
    owner: downloadOptions.owner,
    repo: downloadOptions.repo,
    tag: downloadOptions.tag,
    baseUrl: downloadOptions.baseUrl,
    assetName: target.runtimeManifestAsset
  });

  const response = await fetch(manifestUrl, {
    method: "GET",
    headers: requestOptions.headers || {},
    redirect: "follow"
  });

  if (!response.ok) {
    throw new Error(
      "Failed to download runtime manifest from " +
        manifestUrl +
        " (status " +
        response.status +
        ")"
    );
  }

  let manifest;
  try {
    manifest = await response.json();
  } catch (err) {
    throw new Error(
      "Runtime manifest is not valid JSON: " +
        (err && err.message ? err.message : String(err))
    );
  }

  if (!manifest || !Array.isArray(manifest.runtimeFiles)) {
    throw new Error("Runtime manifest does not contain a runtimeFiles array");
  }

  return manifest.runtimeFiles.map(normalizeRuntimeFile).filter(Boolean);
}

async function resolveRuntimeFiles(target, downloadOptions, requestOptions = {}) {
  const baseRuntimeFiles = [
    {
      assetName: target.assetName,
      destinationName: target.executableName
    },
    ...(Array.isArray(target.companionAssets) ? target.companionAssets : [])
  ];

  const manifestRuntimeFiles = await fetchRuntimeManifestFiles(
    target,
    downloadOptions,
    requestOptions
  );

  return dedupeRuntimeFiles(baseRuntimeFiles.concat(manifestRuntimeFiles));
}

module.exports = {
  resolveRuntimeFiles,
  dedupeRuntimeFiles
};
