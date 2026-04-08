"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const { buildDownloadUrl } = require("../lib/github");

test("buildDownloadUrl uses latest release path by default", () => {
  const out = buildDownloadUrl({
    owner: "clixiya",
    repo: "badlang",
    assetName: "bad-linux-x64"
  });

  assert.equal(
    out,
    "https://github.com/clixiya/badlang/releases/latest/download/bad-linux-x64"
  );
});

test("buildDownloadUrl uses explicit tag path", () => {
  const out = buildDownloadUrl({
    owner: "clixiya",
    repo: "badlang",
    tag: "v1.2.3",
    assetName: "bad-darwin-arm64"
  });

  assert.equal(
    out,
    "https://github.com/clixiya/badlang/releases/download/v1.2.3/bad-darwin-arm64"
  );
});

test("buildDownloadUrl uses baseUrl override", () => {
  const out = buildDownloadUrl({
    baseUrl: "https://cdn.example.com/bad/",
    assetName: "bad-win32-x64.exe"
  });

  assert.equal(out, "https://cdn.example.com/bad/bad-win32-x64.exe");
});

test("buildDownloadUrl throws when assetName missing", () => {
  assert.throws(() => buildDownloadUrl({ owner: "clixiya", repo: "badlang" }), /assetName is required/);
});

test("buildDownloadUrl throws when owner/repo missing without baseUrl", () => {
  assert.throws(() => buildDownloadUrl({ assetName: "bad" }), /owner and repo are required/);
});
