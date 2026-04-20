# badlang

Cross-platform npm wrapper for the BAD runtime.

On install, this package detects your OS/CPU and downloads the matching BAD binary from GitHub releases.

If download is skipped during install (for example in CI), the `bad` command will auto-download the correct binary on first run.

## Install

```bash
npm install -g badlang
```

Then run:

```bash
bad --help
bad ./examples/01-basics/quick_start_demo.bad
```

## How It Works

1. `postinstall` resolves your platform (`darwin`, `linux`, `win32`) and arch (`x64`, `arm64`).
2. It builds a GitHub release URL for the right asset.
3. It downloads the binary into `vendor/bin/` inside this package.
4. The `bad` bin script forwards your CLI args to that binary.
5. If the binary is missing at runtime, it is fetched automatically before executing your file.

## Expected Release Asset Names

- `bad-darwin-x64`
- `bad-darwin-arm64`
- `bad-linux-x64`
- `bad-linux-arm64`
- `bad-win32-x64.exe`
- `bad-win32-arm64.exe`
- `bad-win32-x64-runtime-manifest.json`
- `bad-win32-arm64-runtime-manifest.json`
- Additional `bad-win32-*-*.dll` assets listed by the runtime manifest files.

## Environment Variables

- `BAD_GITHUB_OWNER` (default: `clixiya`)
- `BAD_GITHUB_REPO` (default: `badlang`)
- `BAD_GITHUB_TAG` (default: `latest`)
- `BAD_DOWNLOAD_BASE_URL` (optional)
	- If set, installer uses `<base-url>/<asset-name>` directly.
- `BAD_GITHUB_TOKEN` (optional)
	- Added as `Authorization: Bearer <token>` for private/rate-limited downloads.
- `BAD_SKIP_DOWNLOAD=1`
	- Skips postinstall download.
- `BAD_FORCE_DOWNLOAD=1`
	- Re-downloads even if binary already exists.
- `BAD_BIN_PATH` (runtime override)
	- Use a custom local binary path when running `bad`.

## Troubleshooting (Windows/Linux)

If `bad` installs but fails immediately when you run a `.bad` file:

1. Re-download and validate the binary:

```bash
npx bad-install
```

2. Upgrade to the latest package version:

```bash
npm install -g badlang@latest
```

3. If your machine cannot run the published release binary, build BAD locally and point npm to it:

```bash
BAD_BIN_PATH=/absolute/path/to/bad bad --help
```

Common symptoms:

- Windows: process exits with status `3221225781` or reports missing DLLs.
- Linux: process exits with `126/127` or prints `GLIBC_x.y not found`.

## Development

```bash
npm test
npm run clean
```
