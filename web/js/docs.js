/**
 * BAD Language Website — Documentation Page
 * Class: DocsPage — Search, active section tracking, smooth scrolling
 */

class DocsPage {
  constructor() {
    this.links = BAD.$.qsa('.docs-sidebar__link[data-target]');
    this.sections = BAD.$.qsa('.doc-section[id]');
    this.searchInput = BAD.$.id('docs-search');
    this._init();
  }

  _init() {
    this._initSidebarLinks();
    this._initScrollSpy();
    this._initSearch();
    this._initAnchors();
    this._handleHashOnLoad();
  }

  // ─── Sidebar Navigation ───────────────────────
  _initSidebarLinks() {
    this.links.forEach(link => {
      BAD.$.on(link, 'click', (e) => {
        e.preventDefault();
        const target = BAD.$.id(link.dataset.target);
        if (target) {
          BAD.$.scrollTo(target, 80);
          BAD.$.setHash(link.dataset.target);
        }
      });
    });
  }

  _setActiveLink(id) {
    this.links.forEach(l => {
      BAD.$.toggle(l, 'active', l.dataset.target === id);
    });
  }

  // ─── Scroll Spy ───────────────────────────────
  _initScrollSpy() {
    const update = BAD.$.throttle(() => {
      let current = this.sections[0]?.id;
      this.sections.forEach(section => {
        if (window.scrollY + 120 >= section.offsetTop) {
          current = section.id;
        }
      });
      this._setActiveLink(current);
    }, 80);

    BAD.$.on(window, 'scroll', update, { passive: true });
    update();
  }

  // ─── Search ───────────────────────────────────
  _initSearch() {
    if (!this.searchInput) return;

    const allLinks = BAD.$.qsa('.docs-sidebar__link');
    const sectionTitles = BAD.$.qsa('.docs-sidebar__section');

    BAD.$.on(this.searchInput, 'input', BAD.$.debounce(() => {
      const q = this.searchInput.value.toLowerCase().trim();

      if (!q) {
        allLinks.forEach(l => BAD.$.css(l, { display: '' }));
        sectionTitles.forEach(s => BAD.$.css(s, { display: '' }));
        return;
      }

      allLinks.forEach(link => {
        const text = link.textContent.toLowerCase();
        BAD.$.css(link, { display: text.includes(q) ? '' : 'none' });
      });

      // Hide section titles that have no visible links
      sectionTitles.forEach(section => {
        const nextLinks = [];
        let el = section.nextElementSibling;
        while (el && !el.classList.contains('docs-sidebar__section')) {
          if (el.classList.contains('docs-sidebar__link')) nextLinks.push(el);
          el = el.nextElementSibling;
        }
        const hasVisible = nextLinks.some(l => l.style.display !== 'none');
        BAD.$.css(section, { display: hasVisible ? '' : 'none' });
      });
    }, 200));
  }

  // ─── Anchor Hover Copy ────────────────────────
  _initAnchors() {
    BAD.$.qsa('.doc-section h2, .doc-section h3').forEach(heading => {
      const id = heading.id || heading.closest('[id]')?.id;
      if (!id) return;
      const anchor = BAD.$.qs('.anchor', heading);
      if (!anchor) return;
      BAD.$.on(anchor, 'click', async (e) => {
        e.preventDefault();
        const url = `${location.origin}${location.pathname}#${id}`;
        await BAD.$.copy(url);
        BAD.Toast.success('Link copied!');
      });
    });
  }

  // ─── Handle #hash on page load ────────────────
  _handleHashOnLoad() {
    const hash = BAD.$.hash();
    if (hash) {
      setTimeout(() => {
        const el = BAD.$.id(hash);
        if (el) BAD.$.scrollTo(el, 80);
      }, 200);
    }
  }
}

// Bootstrap
document.addEventListener('DOMContentLoaded', () => {
  new DocsPage();
});
