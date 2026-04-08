# BAD Local API Server

This server is a local backend companion for BAD DSL examples.

It provides predictable endpoints for quick testing and supports file-based persistence.

## Contents

1. Overview
2. Install and Start
3. Environment Variables
4. Data Storage
5. Routes
6. Auth Route
7. Logs and Meta
8. Reset and Debug Tips
9. Troubleshooting

---

## 1. Overview

The local server is useful when you want:

- stable local endpoints
- repeatable request bodies
- predictable auth simulation
- request log visibility

Default base URL:

- `http://localhost:3000`

---

## 2. Install and Start

```bash
cd server
npm install
npm start
```

If port 3000 is occupied, configure a different `PORT`.

---

## 3. Environment Variables

Create `.env` (optional):

```env
PORT=3000
USE_MONGO=false
MONGO_URI=mongodb://localhost:27017/apitest
DATA_FILE=server/data/db.json
```

Variables:

- `PORT`: server port
- `USE_MONGO`: `true` or `false`
- `MONGO_URI`: MongoDB connection string
- `DATA_FILE`: file path for JSON persistence mode
- `BAD_PLAYGROUND_BASE_URL`: GitHub release download base (default `https://github.com/clixiya/badlang/releases/download`)
- `BAD_PLAYGROUND_RELEASE_VERSION`: release version/tag used for runtime binary (defaults to npm package version)
- `BAD_PLAYGROUND_LOCAL_BINARY`: optional local binary path override for playground runtime
- `BAD_PLAYGROUND_TIMEOUT_MS`: runtime timeout for playground execution
- `BAD_PLAYGROUND_MAX_CODE_BYTES`: max request code size for playground runs

If release download is unavailable, the server also falls back to the repository root binary (`../bad` or `../bad.exe`) when present.

---

## 4. Data Storage

When `USE_MONGO=false`:

- data is stored in JSON file
- startup performs lightweight schema migration
- counters and logs are persisted

Data typically includes:

- users
- posts
- todos
- counters
- request logs
- schema version

---

## 5. Routes

Common routes used by BAD examples:

- `GET /users/:id`
- `GET /posts`
- `GET /posts/:id`
- `POST /posts`
- `PUT /posts/:id`
- `PATCH /posts/:id`
- `DELETE /posts/:id`
- `GET /todos/:id`
- `POST /playground/run` (real BAD execution for web playground)

Behavior is example-friendly and intentionally predictable.

---

## 6. Auth Route

Auth simulation route:

- `POST /auth/login`

Typical response includes token-like value for auth flow examples.

---

## 7. Logs and Meta

Diagnostics endpoints:

- `GET /meta`
- `GET /logs?limit=100`

Useful for:

- request inspection
- response timing checks
- local debugging

---

## 8. Reset and Debug Tips

If data becomes inconsistent:

1. stop server
2. remove/reset `DATA_FILE`
3. start server again

For quick BAD + server loop:

```bash
# terminal 1
cd server && npm start

# terminal 2
./bad examples/01-basics/quick_start_demo.bad --base http://localhost:3000
```

---

## 9. Troubleshooting

### Server won't start

- verify `npm install`
- check `PORT` conflict
- inspect startup logs

### BAD tests timeout

- ensure server is running
- confirm base URL/port in `.bad` file
- increase BAD timeout

### 404 for known route

- verify route path and method
- check if endpoint belongs to remote JSONPlaceholder-only examples

---

End of server README.
