# GitHub Workflows

This folder contains release and deployment automation for BAD.

YouTube: https://www.youtube.com/@clixiya

## Workflows

- `release-binaries.yml`
  - Builds cross-OS binaries for macOS (x64 + arm64), Linux (x64 + arm64), and Windows (x64 + arm64)
  - Uploads artifacts to GitHub release for the selected tag
- `publish-npm.yml`
  - Publishes `npm/` (`badlang`) on version tag
  - Verifies package version matches tag and checks package archive preview before publishing
- `release-vsix.yml`
  - Packages VS Code extension into `.vsix`
  - Uploads `.vsix` to tagged GitHub release
- `playground-wasm.yml`
  - Builds BAD WebAssembly runtime for playground
  - Writes runtime assets to `web/dist`
  - Optionally deploys full `web/` site (including playground updates) to GitHub Pages via manual dispatch
- `deploy-website.yml`
  - Builds BAD WASM runtime into `web/dist`
  - Deploys `web/` static site to GitHub Pages on `main` pushes or manual dispatch

## Required Secrets

- `NPM_TOKEN` for npm publish

## Tag Convention

Use semantic tags, for example:

- `v1.0.0`
