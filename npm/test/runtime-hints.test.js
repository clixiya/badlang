"use strict";

const test = require("node:test");
const assert = require("node:assert/strict");
const { getRuntimeFailureHint } = require("../lib/runtime-hints");

test("getRuntimeFailureHint reports Windows DLL guidance for STATUS_DLL_NOT_FOUND", () => {
  const hint = getRuntimeFailureHint("win32", { exitCode: 3221225781 });
  assert.match(hint, /runtime DLLs/i);
  assert.match(hint, /libcurl-4\.dll/i);
});

test("getRuntimeFailureHint reports Linux shared-library guidance for linker exit code", () => {
  const hint = getRuntimeFailureHint("linux", { exitCode: 127 });
  assert.match(hint, /shared-library mismatch/i);
  assert.match(hint, /glibc\/libcurl/i);
});

test("getRuntimeFailureHint is empty when failure does not match known platform issues", () => {
  const hint = getRuntimeFailureHint("darwin", { exitCode: 1 });
  assert.equal(hint, "");
});
