"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const { resolvePlatform, supportedTargets } = require("../lib/platform");

test("resolvePlatform returns expected darwin arm64 target", () => {
  const out = resolvePlatform("darwin", "arm64");
  assert.equal(out.assetName, "bad-darwin-arm64");
  assert.equal(out.executableName, "bad");
});

test("resolvePlatform returns expected linux x64 target", () => {
  const out = resolvePlatform("linux", "x64");
  assert.equal(out.assetName, "bad-linux-x64");
  assert.equal(out.executableName, "bad");
});

test("resolvePlatform returns expected win32 x64 target", () => {
  const out = resolvePlatform("win32", "x64");
  assert.equal(out.assetName, "bad-win32-x64.exe");
  assert.equal(out.executableName, "bad.exe");
  assert.equal(out.runtimeFiles.length, 2);
  assert.equal(out.runtimeFiles[0].assetName, "bad-win32-x64.exe");
  assert.equal(out.runtimeFiles[0].destinationName, "bad.exe");
  assert.equal(out.runtimeFiles[1].assetName, "bad-win32-x64-libcurl-4.dll");
  assert.equal(out.runtimeFiles[1].destinationName, "libcurl-4.dll");
});

test("resolvePlatform keeps linux runtime files to executable only", () => {
  const out = resolvePlatform("linux", "x64");
  assert.equal(out.runtimeFiles.length, 1);
  assert.equal(out.runtimeFiles[0].assetName, "bad-linux-x64");
  assert.equal(out.runtimeFiles[0].destinationName, "bad");
});

test("resolvePlatform throws on unsupported platform", () => {
  assert.throws(() => resolvePlatform("freebsd", "x64"), /Unsupported platform/);
});

test("resolvePlatform throws on unsupported architecture", () => {
  assert.throws(() => resolvePlatform("linux", "ppc64"), /Unsupported architecture/);
});

test("supportedTargets includes all expected tuples", () => {
  const list = supportedTargets();
  assert.ok(list.includes("darwin/x64"));
  assert.ok(list.includes("darwin/arm64"));
  assert.ok(list.includes("linux/x64"));
  assert.ok(list.includes("linux/arm64"));
  assert.ok(list.includes("win32/x64"));
  assert.ok(list.includes("win32/arm64"));
});
