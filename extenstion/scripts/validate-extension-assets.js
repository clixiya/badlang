#!/usr/bin/env node

const fs = require("fs");
const path = require("path");

const rootDir = path.resolve(__dirname, "..");

function absPath(relativePath) {
  return path.join(rootDir, relativePath);
}

function readText(relativePath) {
  return fs.readFileSync(absPath(relativePath), "utf8");
}

function readJson(relativePath) {
  const text = readText(relativePath);
  try {
    return JSON.parse(text);
  } catch (error) {
    throw new Error(`Invalid JSON in ${relativePath}: ${error.message}`);
  }
}

function assert(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

function escapeRegex(value) {
  return value.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
}

function extractConstStringArray(source, constName) {
  const pattern = new RegExp(`const\\s+${constName}\\s*=\\s*\\[(.*?)\\];`, "s");
  const match = source.match(pattern);
  if (!match) {
    throw new Error(`Could not find ${constName} in extension.js`);
  }

  const values = [];
  const valuePattern = /"([^"\\]*(?:\\.[^"\\]*)*)"/g;
  let valueMatch;

  while ((valueMatch = valuePattern.exec(match[1])) !== null) {
    values.push(valueMatch[1]);
  }

  return values;
}

function collectSnippetBodyText(snippetsObject) {
  const lines = [];

  for (const snippet of Object.values(snippetsObject)) {
    if (!snippet || !Object.prototype.hasOwnProperty.call(snippet, "body")) {
      continue;
    }

    if (Array.isArray(snippet.body)) {
      lines.push(...snippet.body.map((line) => String(line)));
    } else {
      lines.push(String(snippet.body));
    }
  }

  return lines.join("\n");
}

function main() {
  const extensionSource = readText("extension.js");
  const snippets = readJson("snippets/bad.json");

  // Validate key JSON assets used by the extension at activation time.
  readJson("syntaxes/bad.tmLanguage.json");
  readJson("language-configuration.json");

  const topLevelOptions = extractConstStringArray(extensionSource, "TOP_LEVEL_OPTIONS");
  const snippetText = collectSnippetBodyText(snippets);

  const missingOptions = topLevelOptions.filter((option) => {
    const pattern = new RegExp(`\\b${escapeRegex(option)}\\b`);
    return !pattern.test(snippetText);
  });

  assert(
    missingOptions.length === 0,
    `Missing snippet coverage for TOP_LEVEL_OPTIONS: ${missingOptions.join(", ")}`
  );

  console.log("validate-extension-assets: ok");
}

try {
  main();
} catch (error) {
  console.error(`validate-extension-assets: ${error.message}`);
  process.exit(1);
}
