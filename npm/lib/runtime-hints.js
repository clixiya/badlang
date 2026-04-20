"use strict";

const WINDOWS_DLL_EXIT_CODES = new Set([3221225781, -1073741515]);
const LINUX_LINKER_EXIT_CODES = new Set([126, 127]);

function normalizeCode(value) {
  return String(value || "").trim().toUpperCase();
}

function getRuntimeFailureHint(platform = process.platform, failure = {}) {
  const errorCode = normalizeCode(failure.code);
  const exitCode = Number.isInteger(failure.exitCode) ? failure.exitCode : null;

  if (platform === "win32") {
    if (errorCode === "UNKNOWN" || errorCode === "ENOENT" || WINDOWS_DLL_EXIT_CODES.has(exitCode)) {
      return (
        "Windows could not launch BAD. This usually means required runtime DLLs are missing " +
        "(for example libcurl-4.dll). Install the latest badlang release or set BAD_BIN_PATH " +
        "to a local bad.exe built on this machine."
      );
    }
    return "";
  }

  if (platform === "linux") {
    if (
      errorCode === "ENOENT" ||
      errorCode === "ENOEXEC" ||
      errorCode === "ELIBBAD" ||
      LINUX_LINKER_EXIT_CODES.has(exitCode)
    ) {
      return (
        "Linux could not load the BAD binary. This is usually a shared-library mismatch " +
        "(glibc/libcurl). Install the latest badlang release or set BAD_BIN_PATH to a local " +
        "bad binary built for this distro."
      );
    }
    return "";
  }

  return "";
}

module.exports = {
  getRuntimeFailureHint
};
