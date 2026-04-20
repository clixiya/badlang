"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const { resolveRuntimeFiles } = require("../lib/runtime-files");

test("resolveRuntimeFiles returns base executable when no manifest configured", async () => {
  const files = await resolveRuntimeFiles(
    {
      assetName: "bad-linux-x64",
      executableName: "bad"
    },
    {
      owner: "clixiya",
      repo: "badlang",
      tag: "latest",
      baseUrl: ""
    }
  );

  assert.equal(files.length, 1);
  assert.equal(files[0].assetName, "bad-linux-x64");
  assert.equal(files[0].destinationName, "bad");
});

test("resolveRuntimeFiles merges runtime manifest files with base files", async () => {
  const originalFetch = global.fetch;
  global.fetch = async () => {
    return {
      ok: true,
      async json() {
        return {
          runtimeFiles: [
            {
              assetName: "bad-win32-x64-libssh2-1.dll",
              destinationName: "libssh2-1.dll"
            },
            {
              assetName: "bad-win32-x64-libcurl-4.dll",
              destinationName: "libcurl-4.dll"
            }
          ]
        };
      }
    };
  };

  try {
    const files = await resolveRuntimeFiles(
      {
        assetName: "bad-win32-x64.exe",
        executableName: "bad.exe",
        companionAssets: [
          {
            assetName: "bad-win32-x64-libcurl-4.dll",
            destinationName: "libcurl-4.dll"
          }
        ],
        runtimeManifestAsset: "bad-win32-x64-runtime-manifest.json"
      },
      {
        owner: "clixiya",
        repo: "badlang",
        tag: "v1.0.3",
        baseUrl: ""
      }
    );

    assert.equal(files.length, 3);
    assert.equal(files[0].assetName, "bad-win32-x64.exe");
    assert.equal(files[0].destinationName, "bad.exe");
    assert.equal(files[1].assetName, "bad-win32-x64-libcurl-4.dll");
    assert.equal(files[1].destinationName, "libcurl-4.dll");
    assert.equal(files[2].assetName, "bad-win32-x64-libssh2-1.dll");
    assert.equal(files[2].destinationName, "libssh2-1.dll");
  } finally {
    global.fetch = originalFetch;
  }
});

test("resolveRuntimeFiles throws when runtime manifest download fails", async () => {
  const originalFetch = global.fetch;
  global.fetch = async () => {
    return {
      ok: false,
      status: 404
    };
  };

  try {
    await assert.rejects(
      () =>
        resolveRuntimeFiles(
          {
            assetName: "bad-win32-x64.exe",
            executableName: "bad.exe",
            runtimeManifestAsset: "bad-win32-x64-runtime-manifest.json"
          },
          {
            owner: "clixiya",
            repo: "badlang",
            tag: "v1.0.3",
            baseUrl: ""
          }
        ),
      /Failed to download runtime manifest/
    );
  } finally {
    global.fetch = originalFetch;
  }
});
