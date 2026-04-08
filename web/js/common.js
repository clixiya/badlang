/**
 * BAD Language Website — Shared/Common JS
 * Navigation, footer, mobile menu, scroll effects
 */

class SiteNav {
  constructor() {
    this.nav = BAD.$.qs('.nav');
    this.hamburger = BAD.$.qs('.nav__hamburger');
    this.mobile = BAD.$.qs('.nav__mobile');
    this.links = BAD.$.qsa('.nav__links a, .nav__mobile a');
    this._open = false;
    this.init();
  }

  init() {
    // Scroll-based nav style
    BAD.$.on(window, 'scroll', BAD.$.throttle(() => this._onScroll(), 50), { passive: true });
    this._onScroll();

    // Mobile hamburger
    if (this.hamburger) {
      BAD.$.on(this.hamburger, 'click', () => this._toggleMobile());
    }

    // Close mobile on link click
    this.links.forEach(a => {
      BAD.$.on(a, 'click', () => { if (this._open) this._closeMobile(); });
    });

    // Close on outside click
    BAD.$.on(document, 'click', (e) => {
      if (this._open && !e.target.closest('.nav') && !e.target.closest('.nav__mobile')) {
        this._closeMobile();
      }
    });

    // Highlight current page link
    this._highlightCurrentPage();
  }

  _onScroll() {
    BAD.$.toggle(this.nav, 'scrolled', window.scrollY > 20);
  }

  _toggleMobile() {
    this._open ? this._closeMobile() : this._openMobile();
  }

  _openMobile() {
    this._open = true;
    BAD.$.add(this.hamburger, 'open');
    BAD.$.add(this.mobile, 'open');
  }

  _closeMobile() {
    this._open = false;
    BAD.$.remove(this.hamburger, 'open');
    BAD.$.remove(this.mobile, 'open');
  }

  _highlightCurrentPage() {
    const path = location.pathname.split('/').pop() || 'index.html';
    this.links.forEach(a => {
      const href = a.getAttribute('href')?.split('/').pop() || '';
      if (href === path || (path === 'index.html' && href === '')) {
        BAD.$.add(a, 'active');
      }
    });
  }
}

class ScrollReveal {
  constructor() {
    this.init();
  }
  init() {
    BAD.$.revealAll('[data-reveal]');
  }
}

// ─── Shared footer HTML builder ───────────────────────────
function buildFooter(container) {
  if (!container) return;
  container.innerHTML = `
    <div class="container">
      <div class="footer__grid">
        <div>
          <div class="footer__brand">bad<span style="color:var(--accent)">.</span></div>
          <div class="footer__tagline">A CLI-first API testing DSL. Write readable tests with expressive syntax.</div>
        </div>
        <div class="footer__col">
          <h4>Pages</h4>
          <ul>
            <li><a href="index.html">Home</a></li>
            <li><a href="playground.html">Playground</a></li>
            <li><a href="docs.html">Documentation</a></li>
          </ul>
        </div>
        <div class="footer__col">
          <h4>Resources</h4>
          <ul>
            <li><a href="docs.html#install">Install</a></li>
            <li><a href="docs.html#syntax">Syntax Guide</a></li>
            <li><a href="docs.html#cli">CLI Reference</a></li>
            <li><a href="playground.html">Try Online</a></li>
          </ul>
        </div>
        <div class="footer__col">
          <h4>Language</h4>
          <ul>
            <li><a href="docs.html#keywords">Keywords</a></li>
            <li><a href="docs.html#examples">Examples</a></li>
            <li><a href="docs.html#config">Config File</a></li>
            <li><a href="docs.html#history">History</a></li>
          </ul>
        </div>
      </div>
      <div class="footer__bottom">
        <span>bad — API Testing DSL · Written in C</span>
        <span>built with ♥ by the bad team</span>
      </div>
    </div>
  `;
}

// ─── Bootstrap shared behavior ────────────────────────────
document.addEventListener('DOMContentLoaded', () => {
  // Initialize nav
  new SiteNav();

  // Initialize scroll reveal
  new ScrollReveal();

  // Build footer
  buildFooter(BAD.$.qs('.footer'));

  // Initialize code copy buttons
  BAD.CodeCopy.init();

  // Initialize syntax highlighting
  BAD.Highlight.init();
});
