/**
 * BAD Language Website — Playground Page
 * Class: PlaygroundPage — Runs real BAD runtime via local server
 */

class BADInterpreter {
  /**
   * Lightweight simulation of the BAD runtime for the playground.
   * Not a full parser — demonstrates output and catches common patterns.
   */

  static examples = {
    'basic': `base_url = "https://jsonplaceholder.typicode.com"
timeout = 5000

test "get a post" {
    send GET "/posts/1"
    expect status 200
    expect json.id exists
    expect json.id == 1
    expect json.title exists
}

test "create a post" {
    send POST "/posts" {
        body {
            title: "Hello bad"
            body:  "testing bad language"
            userId: 1
        }
    }
    expect status 201
    expect json.id exists
    expect json.title == "Hello bad"
}

test "delete a post" {
    send DELETE "/posts/1"
    expect status 200
}`,

    'auth': `base_url = "https://jsonplaceholder.typicode.com"

# Simulate login and token capture
test "login" {
    send POST "/posts" {
        body {
            email:    "user@example.com"
            password: "secret123"
        }
    }
    expect status 201
    expect json.id exists
}

let auth_token = "Bearer eyJhbGciOiJIUzI1NiJ9"
let user_path  = "/users/1"

test "get authenticated user" {
    send GET user_path {
        header {
            Authorization: auth_token
        }
    }
    expect status 200
    expect json.id exists
    expect json.name exists
    expect json.email exists
}`,

    'assertions': `base_url = "https://jsonplaceholder.typicode.com"

test "json path assertions" {
    send GET "/users/1"
    expect status 200
    expect json.id == 1
    expect json.id > 0
    expect json.name exists
    expect json.email contains "@"
    expect json.email ends_with ".biz"
    expect json.username starts_with "Bret"
    expect json.website exists
    expect json.address.city exists
    expect json.company.name exists
}

test "status range check" {
    send GET "/posts/1"
    expect status 200
    expect status in [200, 201, 204]
    expect time_ms < 5000
}`,

    'control': `base_url = "https://jsonplaceholder.typicode.com"

test "conditional logic" {
    send GET "/posts/1"
    
    if status == 200 {
        print "request succeeded"
    } else {
        stop because "unexpected status"
    }
    
    expect json.id exists
    
    if json.id > 0 {
        print "valid id received"
    }
    
    expect status 200
}

test "skip if not found" {
    send GET "/posts/9999"
    skip_if status == 404 because "post not found"
    expect status 200
}

test "timing assertion" {
    time_start fetch
    send GET "/users"
    time_stop fetch
    expect time fetch < 10000
    expect time_ms < 10000
}`,

    'retry': `base_url = "https://jsonplaceholder.typicode.com"

# Retry configuration
retry_count   = 2
retry_delay_ms = 100

test "reliable get" {
    send GET "/posts/1" {
        retry 3
        retry_delay_ms 200
        retry_backoff exponential
        retry_jitter_ms 25
    }
    expect status 200
    expect json.id exists
}

test "list with retry" {
    send GET "/posts" {
        retry 2
        retry_delay_ms 100
        retry_backoff linear
    }
    expect status 200
    expect json.0.id exists
}`,

    'groups': `base_url = "https://jsonplaceholder.typicode.com"

before_all {
    print "suite starting"
}

before_each {
    print "test starting"
}

after_each {
    print "test complete"
}

after_all {
    print "suite done"
}

group "posts" {
    test "get post" {
        send GET "/posts/1"
        expect status 200
        expect json.id == 1
    }
    
    test "list posts" {
        send GET "/posts"
        expect status 200
        expect json.0.userId exists
    }
}

group "users" {
    test "get user" {
        send GET "/users/1"
        expect status 200
        expect json.name exists
    }
    }`,

      'templatehooks': `base_url = "https://jsonplaceholder.typicode.com"

    let shared_headers = {
      Accept: "application/json"
    }

    let default_todo = {
      userId: 1
      completed: false
    }

    on_error {
      print "failure observed"
    }

    before_url "/todos*" {
      print "before todos request"
    }

    template create_todo {
      method POST
      path "/todos"
      header shared_headers
      body default_todo
      expect status 201
    }

    test "merge template body defaults" {
      send req create_todo with {
        body {
          title: "from override"
        }
        body_merge true
      }
      expect status 201
      expect json.id exists
    }`
  };

  /**
   * Parse and simulate running a bad file.
   * Returns an array of output lines with type info.
   */
  static async simulate(code) {
    const lines = code.split('\n');
    const output = [];
    let tests = [];
    let currentTest = null;
    let inGroup = false;
    let groupName = '';
    let hasBeforeAll = false, hasAfterAll = false;
    let hasBefore = false, hasAfter = false;

    // Extract test blocks
    for (let i = 0; i < lines.length; i++) {
      const line = lines[i].trim();
      if (line.startsWith('before_all')) hasBeforeAll = true;
      if (line.startsWith('after_all')) hasAfterAll = true;
      if (line.startsWith('before_each')) hasBefore = true;
      if (line.startsWith('after_each')) hasAfter = true;

      const groupMatch = line.match(/^group\s+"([^"]+)"/);
      if (groupMatch) { inGroup = true; groupName = groupMatch[1]; }

      const testMatch = line.match(/^test\s+"([^"]+)"/);
      const skipMatch = line.match(/^skip\s+test\s+"([^"]+)"/);

      if (skipMatch) {
        const reasonMatch = line.match(/because\s+"([^"]+)"/);
        tests.push({ name: skipMatch[1], skipped: true, reason: reasonMatch?.[1] || 'skipped', group: groupName });
      }
      else if (testMatch && !line.startsWith('skip')) {
        const body = [];
        let depth = 0, j = i;
        while (j < lines.length) {
          const l = lines[j];
          body.push(l);
          if (l.includes('{')) depth++;
          if (l.includes('}')) depth--;
          if (depth === 0 && j > i) break;
          j++;
        }
        tests.push({ name: testMatch[1], body: body.join('\n'), group: inGroup ? groupName : null });
      }

      if (line === '}' && inGroup) { inGroup = false; groupName = ''; }
    }

    // Run hooks
    if (hasBeforeAll) {
      output.push({ text: '  ✦ before_all', cls: 'out-dim' });
      output.push({ text: '    print "suite starting"', cls: 'out-dim' });
    }

    let totalPass = 0, totalFail = 0, totalSkip = 0;
    let prevGroup = null;

    for (const test of tests) {
      await new Promise(r => setTimeout(r, 0)); // yield

      if (test.group && test.group !== prevGroup) {
        output.push({ text: '', cls: '' });
        output.push({ text: `▸ group "${test.group}"`, cls: 'out-label' });
        prevGroup = test.group;
      }

      if (test.skipped) {
        output.push({ text: `  ○ test "${test.name}" [skipped: ${test.reason}]`, cls: 'out-skip' });
        totalSkip++;
        continue;
      }

      if (hasBefore) output.push({ text: `    ↳ before_each`, cls: 'out-dim' });

      output.push({ text: `◆ test "${test.name}"`, cls: 'out-label' });

      // Parse expectations from body
      const expects = this._parseExpects(test.body || '');
      const prints  = this._parsePrints(test.body || '');
      const hasSendMethod = (test.body || '').match(/send\s+(GET|POST|PUT|PATCH|DELETE)/);
      const hasSendTemplate = (test.body || '').match(/send\s+(?:req|request|template)\s+[A-Za-z_][A-Za-z0-9_]*/);
      const hasSkipIf = (test.body || '').match(/skip_if\s+status\s+==\s+404/);
      const hasTimer = (test.body || '').match(/time_start\s+(\w+)/);
      const hasIf = (test.body || '').match(/if\s+status/);

      // Simulate the request
      let passCount = 0, failCount = 0;
      const ms = Math.floor(Math.random() * 200) + 40;

      if (hasSendMethod || hasSendTemplate) {
        const method = hasSendMethod ? hasSendMethod[1] : 'TEMPLATE';
        const pathMatch = (test.body || '').match(/send\s+(?:GET|POST|PUT|PATCH|DELETE)\s+(?:req\s+)?["\/]([^"\s{]*)/);
        const path = pathMatch ? ('/' + pathMatch[1]).replace('//', '/') : '/template';

        // Simulate skip_if
        if (hasSkipIf) {
          output.push({ text: `    → ${method} ${path}`, cls: 'out-dim' });
          output.push({ text: `  ○ skipped (status == 404) because "post not found"`, cls: 'out-skip' });
          if (hasAfter) output.push({ text: `    ↳ after_each`, cls: 'out-dim' });
          totalSkip++;
          continue;
        }

        // Print conditional output
        if (hasIf) {
          prints.forEach(p => output.push({ text: `    → ${p}`, cls: 'out-dim' }));
        }

        // Simulate timer
        if (hasTimer) {
          const timerName = hasTimer[1];
          const timerMs = Math.floor(Math.random() * 400) + 100;
          output.push({ text: `    ↺ timer "${timerName}" started`, cls: 'out-dim' });
          output.push({ text: `    ↺ timer "${timerName}" stopped: ${timerMs}ms`, cls: 'out-dim' });
        }

        // Show assertions
        for (const exp of expects) {
          const ok = this._evalExpect(exp);
          if (ok) {
            output.push({ text: `    ✓ ${exp}`, cls: 'out-pass' });
            passCount++;
          } else {
            output.push({ text: `    ✗ ${exp}`, cls: 'out-fail' });
            failCount++;
          }
        }
      } else {
        // No send — just print statements or setup
        prints.forEach(p => output.push({ text: `    → ${p}`, cls: 'out-dim' }));
      }

      if (failCount > 0) {
        output.push({ text: `  FAIL (${passCount}/${passCount + failCount} passed) [${ms}ms]`, cls: 'out-fail' });
        totalFail++;
      } else {
        output.push({ text: `  OK (${passCount}/${passCount || 1} passed) [${ms}ms]`, cls: 'out-pass' });
        totalPass++;
      }

      if (hasAfter) output.push({ text: `    ↳ after_each`, cls: 'out-dim' });
      output.push({ text: '', cls: '' });
    }

    if (hasAfterAll) {
      output.push({ text: '  ✦ after_all', cls: 'out-dim' });
    }

    const total = totalPass + totalFail + totalSkip;
    output.push({ text: '─'.repeat(48), cls: 'out-dim' });
    output.push({
      text: `${total} tests  •  ${totalPass} passed  •  ${totalFail} failed  •  ${totalSkip} skipped`,
      cls: totalFail > 0 ? 'out-fail' : 'out-pass',
      isSummary: true,
      allPass: totalFail === 0,
    });

    return output;
  }

  static _parseExpects(body) {
    const lines = body.split('\n');
    const expects = [];
    for (const line of lines) {
      const m = line.trim().match(/^(?:expect|assert)\s+(.+)/);
      if (m) expects.push(m[1].trim());
    }
    return expects;
  }

  static _parsePrints(body) {
    const lines = body.split('\n');
    const prints = [];
    for (const line of lines) {
      const m = line.trim().match(/^print\s+"?([^"]+)"?/);
      if (m) prints.push(m[1]);
    }
    return prints;
  }

  static _evalExpect(exp) {
    // Simulate pass for most common patterns
    if (exp.match(/^status\s+\d+$/)) return true;
    if (exp.match(/^status\s+in\s+\[/)) return true;
    if (exp.match(/^status\s+(==|!=|<=|>=|<|>)\s+/)) return true;
    if (exp.match(/json\.\S+\s+exists$/)) return true;
    if (exp.match(/json\.\S+\s+==\s+/)) return true;
    if (exp.match(/json\.\S+\s+>\s+0/)) return true;
    if (exp.match(/time\w*\s+</)) return true;
    if (exp.match(/json\.\S+\s+contains/)) return Math.random() > 0.15;
    if (exp.match(/json\.\S+\s+ends_with/)) return Math.random() > 0.25;
    if (exp.match(/json\.\S+\s+starts_with/)) return Math.random() > 0.2;
    if (exp.match(/json\.\S+\s+regex/)) return Math.random() > 0.2;
    return Math.random() > 0.05; // 95% pass rate for unknown expects
  }
}

class PlaygroundPage {
  constructor() {
    this.editor = BAD.$.id('bad-editor');
    this.outputBody = BAD.$.id('output-body');
    this.lineNumbers = BAD.$.id('editor-lines');
    this.runBtn = BAD.$.id('run-btn');
    this.clearBtn = BAD.$.id('clear-btn');
    this.statusBar = BAD.$.id('status-bar');
    this._running = false;
    this._currentExample = 'basic';
    this.runtimeEndpoints = this._buildRuntimeEndpoints();
    this._init();
  }

  _buildRuntimeEndpoints() {
    const configured = String(window.BAD_PLAYGROUND_API || '').trim();
    const meta = document.querySelector('meta[name="bad-playground-api"]');
    const fromMeta = String(meta?.content || '').trim();
    const sameOrigin = location.protocol.startsWith('http') ? location.origin : '';

    const bases = [configured, fromMeta, sameOrigin, 'http://127.0.0.1:3000', 'http://localhost:3000']
      .filter(Boolean)
      .map(v => v.replace(/\/+$/, ''));

    return Array.from(new Set(bases.map(base => `${base}/playground/run`)));
  }

  async _runRemote(code) {
    let lastError = null;

    for (const endpoint of this.runtimeEndpoints) {
      try {
        const controller = new AbortController();
        const timer = setTimeout(() => controller.abort(), 30000);

        const response = await fetch(endpoint, {
          method: 'POST',
          headers: { 'Content-Type': 'application/json' },
          body: JSON.stringify({ code }),
          signal: controller.signal,
        });

        clearTimeout(timer);

        const payload = await response.json();
        if (!response.ok) {
          throw new Error(payload?.error || `Runtime server returned ${response.status}`);
        }

        if (payload && typeof payload === 'object') {
          payload.endpoint = endpoint;
          return payload;
        }

        throw new Error('Runtime server returned invalid response payload');
      } catch (err) {
        lastError = err;
      }
    }

    throw new Error(
      `Could not reach BAD runtime server. Start it with: cd server && npm start${lastError ? ` (${lastError.message})` : ''}`
    );
  }

  _resultToOutputLines(result) {
    const lines = [];

    const stdoutLines = String(result.stdout || '').split(/\r?\n/);
    stdoutLines.forEach((line) => {
      if (line.trim() === '') {
        lines.push({ text: '', cls: '' });
        return;
      }
      const cls = /PASS|OK\b/.test(line)
        ? 'out-pass'
        : /FAIL|ERROR/i.test(line)
          ? 'out-fail'
          : /test\s+"/i.test(line)
            ? 'out-label'
            : 'out-dim';
      lines.push({ text: line, cls });
    });

    const stderrText = String(result.stderr || '').trim();
    if (stderrText) {
      lines.push({ text: '', cls: '' });
      lines.push({ text: '[stderr]', cls: 'out-error' });
      stderrText.split(/\r?\n/).forEach((line) => {
        lines.push({ text: line, cls: 'out-fail' });
      });
    }

    lines.push({ text: '─'.repeat(48), cls: 'out-dim' });

    const timedOut = Boolean(result.timedOut);
    const allPass = Number(result.exitCode || 0) === 0 && !timedOut;
    const runtimeLabel = result.runtime?.target
      ? `${result.runtime.target} ${result.runtime.versionTag || ''}`.trim()
      : 'runtime';

    lines.push({
      text: timedOut
        ? `timeout after ${result.durationMs || 0}ms • ${runtimeLabel}`
        : `exit ${result.exitCode} • ${result.durationMs || 0}ms • ${runtimeLabel}`,
      cls: allPass ? 'out-pass' : 'out-fail',
      isSummary: true,
      allPass,
    });

    return lines;
  }

  _init() {
    this._loadExample('basic');
    this._initEditor();
    this._initButtons();
    this._initExamples();
    this._updateLineNumbers();
  }

  _initEditor() {
    if (!this.editor) return;

    BAD.$.on(this.editor, 'input', () => {
      this._updateLineNumbers();
      this._syncScroll();
    });

    BAD.$.on(this.editor, 'scroll', () => this._syncScroll());

    // Tab key support
    BAD.$.on(this.editor, 'keydown', (e) => {
      if (e.key === 'Tab') {
        e.preventDefault();
        const start = this.editor.selectionStart;
        const end = this.editor.selectionEnd;
        this.editor.value = this.editor.value.slice(0, start) + '    ' + this.editor.value.slice(end);
        this.editor.selectionStart = this.editor.selectionEnd = start + 4;
        this._updateLineNumbers();
      }
      // Ctrl/Cmd+Enter to run
      if ((e.ctrlKey || e.metaKey) && e.key === 'Enter') {
        e.preventDefault();
        this._run();
      }
    });
  }

  _updateLineNumbers() {
    if (!this.lineNumbers || !this.editor) return;
    const lines = this.editor.value.split('\n');
    const activeLine = this.editor.value.slice(0, this.editor.selectionStart).split('\n').length;
    this.lineNumbers.innerHTML = lines.map((_, i) =>
      `<span class="${i + 1 === activeLine ? 'active-line' : ''}">${i + 1}</span>`
    ).join('');
  }

  _syncScroll() {
    if (!this.lineNumbers || !this.editor) return;
    this.lineNumbers.scrollTop = this.editor.scrollTop;
  }

  _initButtons() {
    if (this.runBtn) BAD.$.on(this.runBtn, 'click', () => this._run());
    if (this.clearBtn) BAD.$.on(this.clearBtn, 'click', () => this._clearOutput());
    const copyBtn = BAD.$.id('copy-code-btn');
    if (copyBtn) BAD.$.on(copyBtn, 'click', async () => {
      await BAD.$.copy(this.editor?.value || '');
      BAD.Toast.success('Code copied!');
    });
    const resetBtn = BAD.$.id('reset-btn');
    if (resetBtn) BAD.$.on(resetBtn, 'click', () => {
      this._loadExample(this._currentExample);
      BAD.Toast.info('Editor reset to example.');
    });
  }

  _initExamples() {
    BAD.$.qsa('.example-chip').forEach(chip => {
      BAD.$.on(chip, 'click', () => {
        BAD.$.qsa('.example-chip').forEach(c => BAD.$.remove(c, 'active'));
        BAD.$.add(chip, 'active');
        this._loadExample(chip.dataset.example);
        this._clearOutput();
      });
    });
  }

  _loadExample(name) {
    this._currentExample = name;
    const code = BADInterpreter.examples[name] || BADInterpreter.examples.basic;
    if (this.editor) {
      this.editor.value = code;
      this._updateLineNumbers();
    }
  }

  _clearOutput() {
    if (!this.outputBody) return;
    this.outputBody.classList.add('empty');
    this.outputBody.innerHTML = `
      <div class="output-placeholder">
        <div class="big-icon">◈</div>
        <p>Run your <code>.bad</code> file to see output here.<br>
        <span style="color:var(--text-3)">Press Ctrl+Enter or click Run (real runtime)</span></p>
      </div>`;
    this._setStatus('idle');
  }

  async _run() {
    if (this._running) return;
    this._running = true;

    const code = this.editor?.value?.trim();
    if (!code) {
      BAD.Toast.error('Write some bad code first!');
      this._running = false;
      return;
    }

    this._setStatus('running');
    if (this.outputBody) {
      this.outputBody.classList.remove('empty');
      this.outputBody.innerHTML = `
        <div style="display:flex;align-items:center;gap:8px;color:var(--text-2);padding:var(--sp-3)">
          <span class="spinner"></span>
          <span style="font-family:var(--font-mono);font-size:0.82rem">Running with BAD runtime...</span>
        </div>`;
    }

    try {
      const result = await this._runRemote(code);
      const outputLines = this._resultToOutputLines(result);
      this._renderOutput(outputLines);
    } catch (e) {
      BAD.Err.report(e, 'playground:run');
      if (this.outputBody) {
        this.outputBody.classList.remove('empty');
        this.outputBody.innerHTML = `<div class="out-error">Runtime error: ${e.message}</div>`;
      }
      this._setStatus('fail');
    }

    this._running = false;
  }

  _renderOutput(lines) {
    if (!this.outputBody) return;
    this.outputBody.classList.remove('empty');

    let pass = 0, fail = 0;
    const frag = document.createDocumentFragment();

    for (const line of lines) {
      if (line.isSummary) {
        if (typeof line.allPass === 'boolean') {
          pass = line.allPass ? 1 : 0;
          fail = line.allPass ? 0 : 1;
        } else {
          pass = parseInt(line.text.match(/(\d+) passed/)?.[1] || 0);
          fail = parseInt(line.text.match(/(\d+) failed/)?.[1] || 0);
        }
      }
      const div = document.createElement('div');
      div.className = `out-line ${line.cls || ''}`;
      div.textContent = line.text || '\u00A0';
      frag.appendChild(div);
    }

    this.outputBody.innerHTML = '';
    this.outputBody.appendChild(frag);

    const hasFail = fail > 0;
    this._setStatus(hasFail ? 'fail' : 'pass', pass, fail);
    this.outputBody.scrollTop = 0;
  }

  _setStatus(state, pass = 0, fail = 0) {
    const bar = BAD.$.id('status-bar');
    if (!bar) return;

    const states = {
      idle: `<div class="status-bar__item"><span class="dot"></span> idle</div>
             <div class="status-bar__item">Press Ctrl+Enter to run</div>`,
      running: `<div class="status-bar__item running"><span class="dot"></span> running...</div>`,
      pass: `<div class="status-bar__item pass"><span class="dot"></span> ${pass} passed</div>
             <div class="status-bar__item">all tests passed</div>`,
      fail: `<div class="status-bar__item fail"><span class="dot"></span> ${fail} failed</div>
             <div class="status-bar__item pass">${pass} passed</div>`,
    };
    bar.innerHTML = states[state] || states.idle;
  }
}

// Bootstrap
document.addEventListener('DOMContentLoaded', () => {
  new PlaygroundPage();
});
