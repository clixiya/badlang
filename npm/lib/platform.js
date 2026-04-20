"use strict";

const TARGET_MATRIX = Object.freeze({
  darwin: Object.freeze({
    x64: Object.freeze({
      assetName: "bad-darwin-x64",
      executableName: "bad"
    }),
    arm64: Object.freeze({
      assetName: "bad-darwin-arm64",
      executableName: "bad"
    })
  }),
  linux: Object.freeze({
    x64: Object.freeze({
      assetName: "bad-linux-x64",
      executableName: "bad"
    }),
    arm64: Object.freeze({
      assetName: "bad-linux-arm64",
      executableName: "bad"
    })
  }),
  win32: Object.freeze({
    x64: Object.freeze({
      assetName: "bad-win32-x64.exe",
      executableName: "bad.exe",
      runtimeManifestAsset: "bad-win32-x64-runtime-manifest.json",
      companionAssets: Object.freeze([
        Object.freeze({
          assetName: "bad-win32-x64-libcurl-4.dll",
          destinationName: "libcurl-4.dll"
        })
      ])
    }),
    arm64: Object.freeze({
      assetName: "bad-win32-arm64.exe",
      executableName: "bad.exe",
      runtimeManifestAsset: "bad-win32-arm64-runtime-manifest.json",
      companionAssets: Object.freeze([
        Object.freeze({
          assetName: "bad-win32-arm64-libcurl-4.dll",
          destinationName: "libcurl-4.dll"
        })
      ])
    })
  })
});

function resolvePlatform(platform = process.platform, arch = process.arch) {
  const platformTargets = TARGET_MATRIX[platform];
  if (!platformTargets) {
    throw new Error("Unsupported platform: " + platform);
  }

  const target = platformTargets[arch];
  if (!target) {
    throw new Error("Unsupported architecture for " + platform + ": " + arch);
  }

  return {
    platform,
    arch,
    assetName: target.assetName,
    executableName: target.executableName,
    runtimeManifestAsset: target.runtimeManifestAsset || "",
    companionAssets: Array.isArray(target.companionAssets) ? target.companionAssets : [],
    runtimeFiles: [
      {
        assetName: target.assetName,
        destinationName: target.executableName
      },
      ...(Array.isArray(target.companionAssets) ? target.companionAssets : [])
    ]
  };
}

function supportedTargets() {
  const out = [];
  for (const [platform, arches] of Object.entries(TARGET_MATRIX)) {
    for (const arch of Object.keys(arches)) {
      out.push(platform + "/" + arch);
    }
  }
  return out;
}

module.exports = {
  TARGET_MATRIX,
  resolvePlatform,
  supportedTargets
};
