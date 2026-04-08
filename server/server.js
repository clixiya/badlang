const express = require("express");
const cors = require("cors");
const morgan = require("morgan");
const dotenv = require("dotenv");
const mongoose = require("mongoose");
const fs = require("fs");
const path = require("path");
const os = require("os");
const { randomUUID } = require("crypto");
const { execFile } = require("child_process");
const { Readable } = require("stream");
const { pipeline } = require("stream/promises");

dotenv.config();

const app = express();
const PORT = Number(process.env.PORT || 3000);
const HOST = process.env.HOST || "127.0.0.1";
const USE_MONGO = String(process.env.USE_MONGO || "false").toLowerCase() === "true";
const MONGO_URI = process.env.MONGO_URI || "mongodb://127.0.0.1:27017/apitest";
const DATA_FILE = process.env.DATA_FILE || path.join(__dirname, "data", "db.json");

let BAD_PLAYGROUND_RELEASE_VERSION = process.env.BAD_PLAYGROUND_RELEASE_VERSION || "1.0.0";
try {
  const npmPkg = require("../npm/package.json");
  if (!process.env.BAD_PLAYGROUND_RELEASE_VERSION && npmPkg?.version) {
    BAD_PLAYGROUND_RELEASE_VERSION = npmPkg.version;
  }
} catch (_error) {
  // Fallback to hardcoded version when npm package metadata is unavailable.
}

const BAD_PLAYGROUND_BASE_URL = String(
  process.env.BAD_PLAYGROUND_BASE_URL || "https://github.com/clixiya/badlang/releases/download"
).replace(/\/+$/, "");
const BAD_PLAYGROUND_TIMEOUT_MS = Math.max(Number(process.env.BAD_PLAYGROUND_TIMEOUT_MS || 25000), 2000);
const BAD_PLAYGROUND_MAX_CODE_BYTES = Math.max(Number(process.env.BAD_PLAYGROUND_MAX_CODE_BYTES || 200000), 4096);
const BAD_PLAYGROUND_RUNTIME_DIR =
  process.env.BAD_PLAYGROUND_RUNTIME_DIR || path.join(__dirname, ".runtime");
const BAD_PLAYGROUND_LOCAL_BINARY = process.env.BAD_PLAYGROUND_LOCAL_BINARY || "";
const BAD_PLAYGROUND_REPO_BINARY = path.join(
  path.resolve(__dirname, ".."),
  process.platform === "win32" ? "bad.exe" : "bad"
);

const DB_SCHEMA_VERSION = 2;

const SUPPORTED_PLAYGROUND_TARGETS = new Set([
  "darwin-arm64",
  "darwin-x64",
  "linux-arm64",
  "linux-x64",
  "win32-arm64",
  "win32-x64"
]);

const playgroundRuntimeState = {
  installPromise: null,
  target: null,
  binaryPath: null,
  versionTag: null,
  source: "github-release"
};

function nowIso() {
  return new Date().toISOString();
}

function getVersionTag(version) {
  const normalized = String(version || "").trim() || "1.0.0";
  return normalized.startsWith("v") ? normalized : `v${normalized}`;
}

function getPlaygroundTarget(platform = process.platform, arch = process.arch) {
  const target = `${platform}-${arch}`;
  if (!SUPPORTED_PLAYGROUND_TARGETS.has(target)) {
    const supported = Array.from(SUPPORTED_PLAYGROUND_TARGETS).sort().join(", ");
    throw new Error(`Unsupported runtime target '${target}'. Supported targets: ${supported}`);
  }
  return target;
}

function getPlaygroundBinaryName(platform = process.platform) {
  return platform === "win32" ? "bad.exe" : "bad";
}

function getPlaygroundAssetName(target) {
  return target.startsWith("win32-") ? `bad-${target}.exe` : `bad-${target}`;
}

async function downloadReleaseAsset(url, destinationPath) {
  const response = await fetch(url, { redirect: "follow" });
  if (!response.ok || !response.body) {
    throw new Error(`Failed to download runtime (${response.status} ${response.statusText}) from ${url}`);
  }

  ensureDirForFile(destinationPath);
  const tempPath = `${destinationPath}.tmp`;
  const body = Readable.fromWeb(response.body);
  await pipeline(body, fs.createWriteStream(tempPath));

  if (process.platform !== "win32") {
    await fs.promises.chmod(tempPath, 0o755);
  }

  await fs.promises.rename(tempPath, destinationPath);
}

async function ensurePlaygroundBinary() {
  if (playgroundRuntimeState.binaryPath && fs.existsSync(playgroundRuntimeState.binaryPath)) {
    return playgroundRuntimeState;
  }

  if (playgroundRuntimeState.installPromise) {
    return playgroundRuntimeState.installPromise;
  }

  playgroundRuntimeState.installPromise = (async () => {
    const target = getPlaygroundTarget();
    const binaryName = getPlaygroundBinaryName();
    const binaryPath = path.join(BAD_PLAYGROUND_RUNTIME_DIR, target, binaryName);
    const versionTag = getVersionTag(BAD_PLAYGROUND_RELEASE_VERSION);
    const assetName = getPlaygroundAssetName(target);
    const downloadUrl = `${BAD_PLAYGROUND_BASE_URL}/${versionTag}/${assetName}`;
    const configuredLocalBinaryPath = BAD_PLAYGROUND_LOCAL_BINARY
      ? path.resolve(BAD_PLAYGROUND_LOCAL_BINARY)
      : "";

    await fs.promises.mkdir(path.dirname(binaryPath), { recursive: true });

    let source = "cache";

    if (configuredLocalBinaryPath) {
      await fs.promises.copyFile(configuredLocalBinaryPath, binaryPath);
      source = "local-binary";
      if (process.platform !== "win32") {
        await fs.promises.chmod(binaryPath, 0o755);
      }
    } else if (!fs.existsSync(binaryPath)) {
      try {
        await downloadReleaseAsset(downloadUrl, binaryPath);
        source = "github-release";
      } catch (downloadError) {
        if (fs.existsSync(BAD_PLAYGROUND_REPO_BINARY)) {
          await fs.promises.copyFile(BAD_PLAYGROUND_REPO_BINARY, binaryPath);
          source = "repo-local-fallback";
          if (process.platform !== "win32") {
            await fs.promises.chmod(binaryPath, 0o755);
          }
        } else {
          throw new Error(
            `${downloadError.message}. Set BAD_PLAYGROUND_BASE_URL/BAD_PLAYGROUND_RELEASE_VERSION or BAD_PLAYGROUND_LOCAL_BINARY.`
          );
        }
      }
    }

    playgroundRuntimeState.target = target;
    playgroundRuntimeState.binaryPath = binaryPath;
    playgroundRuntimeState.versionTag = versionTag;
    playgroundRuntimeState.source = source;
    return playgroundRuntimeState;
  })().finally(() => {
    playgroundRuntimeState.installPromise = null;
  });

  return playgroundRuntimeState.installPromise;
}

async function executeBadProgram(binaryPath, badFilePath) {
  return new Promise((resolve) => {
    execFile(
      binaryPath,
      [badFilePath, "--no-color"],
      {
        cwd: path.resolve(__dirname, ".."),
        env: process.env,
        timeout: BAD_PLAYGROUND_TIMEOUT_MS,
        maxBuffer: 2 * 1024 * 1024
      },
      (error, stdout, stderr) => {
        if (!error) {
          resolve({ exitCode: 0, stdout: stdout || "", stderr: stderr || "", timedOut: false });
          return;
        }

        const timedOut = Boolean(error.killed && error.signal === "SIGTERM");
        const exitCode = Number.isInteger(error.code) ? error.code : timedOut ? 124 : 1;

        resolve({
          exitCode,
          stdout: stdout || "",
          stderr: (stderr || error.message || "").trim(),
          timedOut
        });
      }
    );
  });
}

function ensureDirForFile(filePath) {
  const dir = path.dirname(filePath);
  fs.mkdirSync(dir, { recursive: true });
}

function defaultDb() {
  return {
    schema_version: DB_SCHEMA_VERSION,
    counters: { posts: 2, users: 1, logs: 0 },
    users: [
      {
        id: 1,
        name: "clixiya",
        email: "clixiya@example.com",
        address: {
          city: "Mumbai",
          geo: { lat: "19.0760", lng: "72.8777" }
        },
        company: {
          name: "Bad Labs",
          catchPhrase: "Ship tests fast"
        },
        createdAt: nowIso(),
        updatedAt: nowIso()
      }
    ],
    posts: [
      { id: 1, title: "hello", body: "first post", userId: 1, createdAt: nowIso(), updatedAt: nowIso() },
      { id: 2, title: "bad language", body: "api testing", userId: 1, createdAt: nowIso(), updatedAt: nowIso() }
    ],
    logs: []
  };
}

function migrateDb(db) {
  const out = db && typeof db === "object" ? db : defaultDb();
  out.schema_version = Number(out.schema_version || 1);
  out.counters = out.counters || { posts: 0, users: 0, logs: 0 };
  out.users = Array.isArray(out.users) ? out.users : [];
  out.posts = Array.isArray(out.posts) ? out.posts : [];
  out.logs = Array.isArray(out.logs) ? out.logs : [];

  if (out.schema_version < 2) {
    const stamp = nowIso();
    out.users = out.users.map((u) => ({ ...u, createdAt: u.createdAt || stamp, updatedAt: u.updatedAt || stamp }));
    out.posts = out.posts.map((p) => ({ ...p, createdAt: p.createdAt || stamp, updatedAt: p.updatedAt || stamp }));
    out.logs = out.logs.map((l) => ({ ...l, timestamp: l.timestamp || stamp }));
    out.schema_version = 2;
  }

  out.counters.posts = Math.max(out.counters.posts || 0, ...out.posts.map((p) => Number(p.id) || 0));
  out.counters.users = Math.max(out.counters.users || 0, ...out.users.map((u) => Number(u.id) || 0));
  out.counters.logs = Math.max(out.counters.logs || 0, ...out.logs.map((l) => Number(l.id) || 0));
  return out;
}

function loadDb() {
  ensureDirForFile(DATA_FILE);
  if (!fs.existsSync(DATA_FILE)) {
    const initial = defaultDb();
    fs.writeFileSync(DATA_FILE, JSON.stringify(initial, null, 2));
    return initial;
  }

  try {
    const raw = fs.readFileSync(DATA_FILE, "utf8");
    const parsed = raw.trim() ? JSON.parse(raw) : defaultDb();
    const migrated = migrateDb(parsed);
    fs.writeFileSync(DATA_FILE, JSON.stringify(migrated, null, 2));
    return migrated;
  } catch (err) {
    console.error("[server] failed to read db file, recreating default db:", err.message);
    const initial = defaultDb();
    fs.writeFileSync(DATA_FILE, JSON.stringify(initial, null, 2));
    return initial;
  }
}

let db = loadDb();

function saveDb() {
  ensureDirForFile(DATA_FILE);
  fs.writeFileSync(DATA_FILE, JSON.stringify(db, null, 2));
}

function nextId(counterKey) {
  db.counters[counterKey] = Number(db.counters[counterKey] || 0) + 1;
  return db.counters[counterKey];
}

app.use(cors());
app.use(express.json({ limit: "1mb" }));
app.use(morgan("dev"));

app.use((req, res, next) => {
  const started = Date.now();
  const requestId = nextId("logs");
  res.setHeader("x-request-id", String(requestId));

  const shouldSummarizePlaygroundBody = req.path === "/playground/run";
  const requestBody = shouldSummarizePlaygroundBody && typeof req.body?.code === "string"
    ? { codeBytes: Buffer.byteLength(req.body.code, "utf8") }
    : req.body || null;

  res.on("finish", () => {
    db.logs.push({
      id: requestId,
      timestamp: nowIso(),
      method: req.method,
      path: req.originalUrl,
      status: res.statusCode,
      durationMs: Date.now() - started,
      requestBody,
      userAgent: req.get("user-agent") || ""
    });

    if (db.logs.length > 2000) {
      db.logs = db.logs.slice(db.logs.length - 2000);
    }
    saveDb();
  });

  next();
});

app.get("/health", (_req, res) => {
  res.json({
    ok: true,
    service: "bad-local-server",
    mongo: USE_MONGO,
    schemaVersion: db.schema_version,
    dataFile: DATA_FILE,
    totals: { posts: db.posts.length, users: db.users.length, logs: db.logs.length }
  });
});

app.get("/meta", (_req, res) => {
  res.json({ schema_version: db.schema_version, counters: db.counters, data_file: DATA_FILE });
});

app.get("/logs", (req, res) => {
  const limit = Math.min(Math.max(Number(req.query.limit || 100), 1), 1000);
  const items = db.logs.slice(-limit);
  res.json({ count: items.length, items });
});

app.post("/playground/run", async (req, res) => {
  const code = typeof req.body?.code === "string" ? req.body.code : "";
  if (!code.trim()) {
    return res.status(400).json({ ok: false, error: "code is required" });
  }

  const codeBytes = Buffer.byteLength(code, "utf8");
  if (codeBytes > BAD_PLAYGROUND_MAX_CODE_BYTES) {
    return res.status(413).json({
      ok: false,
      error: `code payload exceeds limit of ${BAD_PLAYGROUND_MAX_CODE_BYTES} bytes`
    });
  }

  let tempFilePath = null;

  try {
    const runtime = await ensurePlaygroundBinary();
    const tmpDir = path.join(os.tmpdir(), "bad-playground-runs");
    await fs.promises.mkdir(tmpDir, { recursive: true });

    tempFilePath = path.join(tmpDir, `run-${Date.now()}-${randomUUID()}.bad`);
    await fs.promises.writeFile(tempFilePath, code, "utf8");

    const started = Date.now();
    const result = await executeBadProgram(runtime.binaryPath, tempFilePath);

    return res.status(200).json({
      ok: result.exitCode === 0,
      exitCode: result.exitCode,
      timedOut: result.timedOut,
      durationMs: Date.now() - started,
      stdout: result.stdout,
      stderr: result.stderr,
      runtime: {
        source: runtime.source,
        target: runtime.target,
        versionTag: runtime.versionTag
      }
    });
  } catch (error) {
    console.error("[server] playground run failed:", error.message);
    return res.status(500).json({ ok: false, error: error.message || "playground run failed" });
  } finally {
    if (tempFilePath) {
      fs.promises.unlink(tempFilePath).catch(() => {});
    }
  }
});

app.post("/auth/login", (req, res) => {
  const { email, password } = req.body || {};
  if (!email || !password) {
    return res.status(400).json({ error: "email and password required" });
  }
  return res.status(200).json({
    token: "demo.jwt.token",
    user: { id: 1, email }
  });
});

app.get("/users", (_req, res) => {
  res.json(db.users);
});

app.get("/users/:id", (req, res) => {
  const id = Number(req.params.id);
  const user = db.users.find((u) => u.id === id);
  if (!user) return res.status(404).json({ error: "user not found" });
  return res.json(user);
});

app.get("/posts", (_req, res) => {
  res.json(db.posts);
});

app.get("/posts/:id", (req, res) => {
  const id = Number(req.params.id);
  const post = db.posts.find((p) => p.id === id);
  if (!post) return res.status(404).json({ error: "post not found" });
  return res.json(post);
});

app.post("/posts", (req, res) => {
  const body = req.body || {};

  /* Backward-compatible login simulation for existing auth.bad example. */
  if (body.email && body.password && !body.title) {
    return res.status(200).json({ id: 1, token: "demo.jwt.token" });
  }

  const post = {
    id: nextId("posts"),
    title: body.title || "untitled",
    body: body.body || "",
    userId: Number(body.userId || 1),
    createdAt: nowIso(),
    updatedAt: nowIso()
  };
  db.posts.push(post);
  saveDb();
  return res.status(201).json(post);
});

app.put("/posts/:id", (req, res) => {
  const id = Number(req.params.id);
  const idx = db.posts.findIndex((p) => p.id === id);
  if (idx === -1) return res.status(404).json({ error: "post not found" });

  db.posts[idx] = {
    id,
    title: req.body?.title || "untitled",
    body: req.body?.body || "",
    userId: Number(req.body?.userId || 1),
    createdAt: db.posts[idx].createdAt || nowIso(),
    updatedAt: nowIso()
  };
  saveDb();
  return res.status(200).json(db.posts[idx]);
});

app.patch("/posts/:id", (req, res) => {
  const id = Number(req.params.id);
  const post = db.posts.find((p) => p.id === id);
  if (!post) return res.status(404).json({ error: "post not found" });

  if (req.body?.title !== undefined) post.title = req.body.title;
  if (req.body?.body !== undefined) post.body = req.body.body;
  if (req.body?.userId !== undefined) post.userId = Number(req.body.userId);
  post.updatedAt = nowIso();
  saveDb();
  return res.status(200).json(post);
});

app.delete("/posts/:id", (req, res) => {
  const id = Number(req.params.id);
  const idx = db.posts.findIndex((p) => p.id === id);
  if (idx === -1) return res.status(404).json({ error: "post not found" });
  db.posts.splice(idx, 1);
  saveDb();
  return res.status(200).json({ deleted: true, id });
});

app.get("/posts/:id/comments", (req, res) => {
  const id = Number(req.params.id);
  if (!db.posts.find((p) => p.id === id)) {
    return res.status(404).json({ error: "post not found" });
  }
  return res.json([
    { postId: id, id: 1, email: "a@example.com", body: "nice" },
    { postId: id, id: 2, email: "b@example.com", body: "good" }
  ]);
});

async function maybeConnectMongo() {
  if (!USE_MONGO) {
    console.log("[server] running in in-memory mode");
    return;
  }

  try {
    await mongoose.connect(MONGO_URI);
    console.log(`[server] connected to MongoDB: ${MONGO_URI}`);
  } catch (err) {
    console.error("[server] Mongo connect failed, continuing in memory mode");
    console.error(err.message);
  }
}

let server;

maybeConnectMongo().then(() => {
  server = app.listen(PORT, HOST, () => {
    console.log(`[server] listening on http://${HOST}:${PORT}`);
  });

  server.on("error", (err) => {
    if (err && err.code === "EADDRINUSE") {
      console.error(`[server] port ${PORT} is already in use (another server is running)`);
    } else {
      console.error("[server] http server error:", err);
    }
    process.exit(1);
  });

  server.on("close", () => {
    console.log("[server] http server closed");
  });
});

process.on("SIGINT", () => {
  console.log("[server] received SIGINT, shutting down");
  if (server) server.close(() => process.exit(0));
  else process.exit(0);
});

process.on("SIGTERM", () => {
  console.log("[server] received SIGTERM, shutting down");
  if (server) server.close(() => process.exit(0));
  else process.exit(0);
});

process.on("uncaughtException", (err) => {
  console.error("[server] uncaught exception:", err);
  process.exit(1);
});

process.on("unhandledRejection", (err) => {
  console.error("[server] unhandled rejection:", err);
  process.exit(1);
});