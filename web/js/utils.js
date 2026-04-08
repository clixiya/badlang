/**
 * BAD Language Website - Utility Library
 * Shorthand DOM helpers, error handling, animations, and common utilities
 */

const $ = {
  // ─── DOM Selection ──────────────────────────────────────────
  id: (id) => document.getElementById(id),
  qs: (sel, ctx = document) => ctx.querySelector(sel),
  qsa: (sel, ctx = document) => [...ctx.querySelectorAll(sel)],

  // ─── DOM Creation ───────────────────────────────────────────
  el: (tag, attrs = {}, ...children) => {
    const el = document.createElement(tag);
    for (const [k, v] of Object.entries(attrs)) {
      if (k === 'class') el.className = v;
      else if (k === 'html') el.innerHTML = v;
      else if (k === 'text') el.textContent = v;
      else if (k.startsWith('on')) el.addEventListener(k.slice(2), v);
      else el.setAttribute(k, v);
    }
    children.flat().forEach(c => {
      el.append(typeof c === 'string' ? document.createTextNode(c) : c);
    });
    return el;
  },

  // ─── Class Helpers ──────────────────────────────────────────
  add: (el, ...cls) => el?.classList.add(...cls),
  remove: (el, ...cls) => el?.classList.remove(...cls),
  toggle: (el, cls, force) => el?.classList.toggle(cls, force),
  has: (el, cls) => el?.classList.contains(cls),

  // ─── Style Helpers ──────────────────────────────────────────
  css: (el, styles) => Object.assign(el.style, styles),
  show: (el) => el && (el.style.display = ''),
  hide: (el) => el && (el.style.display = 'none'),

  // ─── Event Helpers ──────────────────────────────────────────
  on: (el, event, fn, opts) => el?.addEventListener(event, fn, opts),
  off: (el, event, fn) => el?.removeEventListener(event, fn),
  once: (el, event, fn) => el?.addEventListener(event, fn, { once: true }),
  delegate: (parent, sel, event, fn) => {
    $.on(parent, event, (e) => {
      const target = e.target.closest(sel);
      if (target) fn.call(target, e, target);
    });
  },

  // ─── Scroll Utilities ───────────────────────────────────────
  scrollTo: (el, offset = 80) => {
    if (!el) return;
    const top = el.getBoundingClientRect().top + window.scrollY - offset;
    window.scrollTo({ top, behavior: 'smooth' });
  },
  scrollTop: () => window.scrollY,
  inView: (el, threshold = 0.15) => {
    if (!el) return false;
    const rect = el.getBoundingClientRect();
    return rect.top < window.innerHeight * (1 - threshold) && rect.bottom > 0;
  },

  // ─── Storage ────────────────────────────────────────────────
  store: {
    get: (key, fallback = null) => {
      try { return JSON.parse(localStorage.getItem(key)) ?? fallback; }
      catch { return fallback; }
    },
    set: (key, val) => {
      try { localStorage.setItem(key, JSON.stringify(val)); return true; }
      catch { return false; }
    },
    remove: (key) => localStorage.removeItem(key),
    clear: () => localStorage.clear(),
  },

  // ─── URL / Routing ──────────────────────────────────────────
  params: () => Object.fromEntries(new URLSearchParams(location.search)),
  hash: () => location.hash.slice(1),
  setHash: (h) => history.pushState(null, '', `#${h}`),

  // ─── Async Helpers ──────────────────────────────────────────
  wait: (ms) => new Promise(r => setTimeout(r, ms)),
  debounce: (fn, ms = 200) => {
    let t;
    return (...args) => { clearTimeout(t); t = setTimeout(() => fn(...args), ms); };
  },
  throttle: (fn, ms = 100) => {
    let last = 0;
    return (...args) => {
      const now = Date.now();
      if (now - last > ms) { last = now; fn(...args); }
    };
  },

  // ─── Clipboard ──────────────────────────────────────────────
  copy: async (text) => {
    try {
      await navigator.clipboard.writeText(text);
      return true;
    } catch {
      // Fallback
      const el = document.createElement('textarea');
      el.value = text;
      el.style.cssText = 'position:fixed;opacity:0;top:0;left:0';
      document.body.appendChild(el);
      el.select();
      document.execCommand('copy');
      el.remove();
      return true;
    }
  },

  // ─── Animation ──────────────────────────────────────────────
  animate: (el, keyframes, opts = {}) => {
    if (!el || !el.animate) return Promise.resolve();
    return el.animate(keyframes, { duration: 300, easing: 'ease', fill: 'forwards', ...opts }).finished;
  },
  fadeIn: (el, ms = 300) => $.animate(el, [{ opacity: 0 }, { opacity: 1 }], { duration: ms }),
  fadeOut: (el, ms = 300) => $.animate(el, [{ opacity: 1 }, { opacity: 0 }], { duration: ms }),
  slideDown: (el, ms = 300) => $.animate(el, [
    { opacity: 0, transform: 'translateY(-10px)' },
    { opacity: 1, transform: 'translateY(0)' }
  ], { duration: ms }),

  // ─── Intersection Observer (reveal on scroll) ───────────────
  observe: (els, callback, opts = {}) => {
    const observer = new IntersectionObserver((entries) => {
      entries.forEach(entry => {
        if (entry.isIntersecting) callback(entry.target, entry);
      });
    }, { threshold: 0.15, ...opts });
    (Array.isArray(els) ? els : [els]).forEach(el => el && observer.observe(el));
    return observer;
  },
  revealAll: (selector = '[data-reveal]', rootClass = 'revealed') => {
    $.observe($.qsa(selector), (el) => {
      $.add(el, rootClass);
      if (el.dataset.delay) $.css(el, { transitionDelay: el.dataset.delay });
    });
  },
};

// ─── Error Handling ───────────────────────────────────────────
const Err = {
  _handlers: [],

  // Register a global error handler
  on: (fn) => Err._handlers.push(fn),

  // Report an error
  report: (err, context = '') => {
    console.error(`[BAD] ${context}:`, err);
    Err._handlers.forEach(h => h(err, context));
  },

  // Wrap async function with error catching
  wrap: (fn, context = fn.name || 'fn') => async (...args) => {
    try { return await fn(...args); }
    catch (e) { Err.report(e, context); }
  },

  // Try/catch shorthand
  try: (fn, fallback = null) => {
    try { return fn(); }
    catch (e) { Err.report(e); return fallback; }
  },

  // Show a toast notification on error
  toast: (msg, type = 'error') => Toast.show(msg, type),
};

// ─── Toast Notification ───────────────────────────────────────
const Toast = {
  _container: null,

  _getContainer() {
    if (!this._container) {
      this._container = $.el('div', { class: 'toast-container', 'aria-live': 'polite' });
      document.body.appendChild(this._container);
    }
    return this._container;
  },

  show(msg, type = 'info', duration = 3000) {
    const container = this._getContainer();
    const toast = $.el('div', { class: `toast toast--${type}` });
    const icons = { success: '✓', error: '✕', info: 'ℹ', warning: '⚠' };
    toast.innerHTML = `<span class="toast__icon">${icons[type] || icons.info}</span><span class="toast__msg">${msg}</span>`;
    container.appendChild(toast);
    requestAnimationFrame(() => $.add(toast, 'toast--visible'));
    setTimeout(() => {
      $.remove(toast, 'toast--visible');
      setTimeout(() => toast.remove(), 400);
    }, duration);
    return toast;
  },

  success: (msg, d) => Toast.show(msg, 'success', d),
  error: (msg, d) => Toast.show(msg, 'error', d),
  info: (msg, d) => Toast.show(msg, 'info', d),
};

// ─── Code Block Copy Buttons ──────────────────────────────────
const CodeCopy = {
  init() {
    $.qsa('pre code, pre.code-block').forEach(block => {
      const pre = block.tagName === 'PRE' ? block : block.parentElement;
      if (pre.querySelector('.code-copy-btn')) return;
      const btn = $.el('button', {
        class: 'code-copy-btn',
        'aria-label': 'Copy code',
        title: 'Copy to clipboard',
        text: 'copy',
      });
      $.on(btn, 'click', async () => {
        const text = (block.tagName === 'CODE' ? block : block.querySelector('code') || block).innerText;
        const ok = await $.copy(text);
        if (ok) {
          btn.textContent = 'copied!';
          $.add(btn, 'code-copy-btn--copied');
          setTimeout(() => {
            btn.textContent = 'copy';
            $.remove(btn, 'code-copy-btn--copied');
          }, 2000);
        }
      });
      pre.style.position = 'relative';
      pre.appendChild(btn);
    });
  },
};

// ─── Syntax Highlighter (lightweight for BAD language) ────────
const Highlight = {
  keywords: ['test','send','expect','assert','let','print','import','export',
    'request','req','group','before_all','before_each','after_each','after_all',
    'skip','skip_if','fail_if','only','because','with','if','and','or','not',
    'else','else_if','contains','starts_with','ends_with','regex','in','retry',
    'sleep','stop','stop_all','bearer','env','args','time_start','time_stop',
    'time','base_url','timeout','base','wait','body','payload','header','headers',
    'export','use'],
  methods: ['GET','POST','PUT','PATCH','DELETE'],
  builtins: ['status','json','exists','time_ms','now_ms','true','false'],

  escape: (s) => s.replace(/&/g,'&amp;').replace(/</g,'&lt;').replace(/>/g,'&gt;'),

  tokenize(code) {
    const escaped = this.escape(code);
    return escaped
      .replace(/#[^\n]*/g, m => `<span class="hl-comment">${m}</span>`)
      .replace(/"([^"]*)"/g, (_, c) => `<span class="hl-string">"${c}"</span>`)
      .replace(/\b(\d+)\b/g, '<span class="hl-number">$1</span>')
      .replace(new RegExp(`\\b(${this.methods.join('|')})\\b`, 'g'),
        '<span class="hl-method">$1</span>')
      .replace(new RegExp(`\\b(${this.keywords.join('|')})\\b`, 'g'),
        '<span class="hl-keyword">$1</span>')
      .replace(new RegExp(`\\b(${this.builtins.join('|')})\\b`, 'g'),
        '<span class="hl-builtin">$1</span>')
      .replace(/json\.([\w.]+)/g, '<span class="hl-path">json.$1</span>');
  },

  init(selector = 'code.language-bad, code.lang-bad') {
    $.qsa(selector).forEach(el => {
      el.innerHTML = this.tokenize(el.textContent);
      $.add(el, 'highlighted');
    });
  },
};

// ─── Nav Active Tracking ──────────────────────────────────────
const NavTracker = {
  init(navSel = '.nav__links a', offset = 100) {
    const links = $.qsa(navSel);
    const sections = links
      .map(a => $.id(a.getAttribute('href')?.slice(1)))
      .filter(Boolean);

    const update = $.throttle(() => {
      let active = sections[0];
      sections.forEach(s => {
        if (window.scrollY + offset >= s.offsetTop) active = s;
      });
      links.forEach(a => {
        $.toggle(a, 'active', a.getAttribute('href') === `#${active?.id}`);
      });
    }, 50);

    $.on(window, 'scroll', update, { passive: true });
    update();
  },
};

// ─── Export all utilities ─────────────────────────────────────
window.BAD = { $, Err, Toast, CodeCopy, Highlight, NavTracker };
