/* The site's language, the same on every page: the menu in the header, the
   choice kept in this browser, the text swapped in place.

   Words come from window.I18N (i18n.js, the whole site's) and, on a brand page,
   from window.I18N_PAGE (that brand's own text, written into the page by
   scripts/make-brand-pages.py); a missing word falls back to English. The HTML
   as written is the English page: what a search engine or a reader without
   JavaScript sees. */
(function () {
  const LANGS = Object.keys(window.I18N);
  const PAGE = window.I18N_PAGE || {};
  const root = document.documentElement;
  /* {name} in a sentence is the page's brand. */
  const brand = document.body.dataset.brand || "";

  /* Map what the browser reports onto what is shipped. European and Brazilian
     Portuguese are separate dictionaries, so `pt-PT` must not fall onto `pt`. */
  function pickLang() {
    try {
      const saved = localStorage.getItem("tigertouch-lang");
      if (saved && LANGS.includes(saved)) return saved;
    } catch (e) {}
    for (const raw of (navigator.languages || [navigator.language || "en"])) {
      const tag = raw.toLowerCase();
      if (LANGS.includes(tag)) return tag;
      const base = tag.split("-")[0];
      if (base === "pt") return tag.startsWith("pt-pt") ? "pt-pt" : "pt";
      if (LANGS.includes(base)) return base;
    }
    return "en";
  }

  function word(code, key) {
    const v = [PAGE[code], window.I18N[code], PAGE.en, window.I18N.en]
      .map(d => d && d[key]).find(x => x != null);
    return v == null ? null : v.split("{name}").join(brand);
  }

  function applyLang(code) {
    root.lang = code;
    const title = word(code, document.body.dataset.title || "title");
    if (title) document.title = title;
    document.querySelectorAll("[data-i18n]").forEach(el => {
      const v = word(code, el.dataset.i18n);
      if (v != null) el.textContent = v;
    });
    /* Only keys the dictionary marks as markup are inserted as HTML. */
    document.querySelectorAll("[data-i18n-html]").forEach(el => {
      const v = word(code, el.dataset.i18nHtml);
      if (v != null) el.innerHTML = v;
    });
    document.querySelectorAll("[data-i18n-aria]").forEach(el => {
      const v = word(code, el.dataset.i18nAria);
      if (v != null) el.setAttribute("aria-label", v);
    });
    document.querySelectorAll("[data-i18n-alt]").forEach(el => {
      const v = word(code, el.dataset.i18nAlt);
      if (v != null) el.alt = v;
    });
  }

  /* On a phone the comparison is stacked: each cell carries its column's name. */
  function labelComparison() {
    const heads = [...document.querySelectorAll(".cmp thead th")].map(th => th.textContent.trim());
    document.querySelectorAll(".cmp tbody tr").forEach(tr =>
      [...tr.children].forEach((td, i) => { if (i) td.dataset.label = heads[i]; }));
  }

  const box = document.getElementById("langbox");
  const lbtn = document.getElementById("langbtn");
  const menu = document.getElementById("langmenu");
  const now = document.getElementById("langnow");
  const TICK = '<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="3" '
             + 'stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">'
             + '<path d="M4.5 12.5 9.5 17.5 19.5 6.5"/></svg>';
  menu.innerHTML = LANGS.map(c =>
    '<li role="option" tabindex="0" data-code="' + c + '" lang="' + c + '" aria-selected="false">'
    + "<span>" + window.I18N[c].lang_name + "</span>" + TICK + "</li>").join("");

  function setOpen(open) {
    box.dataset.open = open ? "1" : "0";
    lbtn.setAttribute("aria-expanded", open ? "true" : "false");
  }
  /* Opened from the keyboard, the focus goes to the language in use; the
     arrows move through the list, Tab or Escape close it. Closed, the list is
     hidden and holds no focus stop (site.css). */
  const items = () => [...menu.querySelectorAll("li")];
  function focusItem(i) {
    const list = items();
    list[(i + list.length) % list.length].focus();
  }
  function focusCurrent() {
    const at = items().findIndex(li => li.getAttribute("aria-selected") === "true");
    requestAnimationFrame(() => focusItem(Math.max(0, at)));
  }
  function choose(code, remember) {
    applyLang(code);
    labelComparison();
    if (window.ttBrandify) window.ttBrandify();
    now.textContent = window.I18N[code].lang_name;
    menu.querySelectorAll("li").forEach(li =>
      li.setAttribute("aria-selected", li.dataset.code === code ? "true" : "false"));
    if (remember) { try { localStorage.setItem("tigertouch-lang", code); } catch (e) {} }
    setOpen(false);
    document.dispatchEvent(new CustomEvent("tt-lang", { detail: code }));
  }
  /* What other scripts read: the language shown and a word in it. */
  window.ttLang = () => root.lang || "en";
  window.ttWord = key => word(window.ttLang(), key);

  lbtn.addEventListener("click", e => {
    const open = box.dataset.open !== "1";
    setOpen(open);
    if (open && e.detail === 0) focusCurrent();  /* a key, not a pointer: move into the list */
  });
  lbtn.addEventListener("keydown", e => {
    if (e.key === "ArrowDown" || e.key === "ArrowUp") {
      e.preventDefault();
      setOpen(true);
      focusCurrent();
    }
  });
  menu.addEventListener("click", e => {
    const li = e.target.closest("li");
    if (li) choose(li.dataset.code, true);
  });
  menu.addEventListener("keydown", e => {
    const li = e.target.closest("li");
    const at = items().indexOf(li);
    if (li && (e.key === "Enter" || e.key === " ")) { e.preventDefault(); choose(li.dataset.code, true); lbtn.focus(); }
    else if (e.key === "ArrowDown") { e.preventDefault(); focusItem(at + 1); }
    else if (e.key === "ArrowUp") { e.preventDefault(); focusItem(at - 1); }
    else if (e.key === "Home") { e.preventDefault(); focusItem(0); }
    else if (e.key === "End") { e.preventDefault(); focusItem(-1); }
    else if (e.key === "Tab") { setOpen(false); lbtn.focus(); }
  });
  document.addEventListener("click", e => { if (!box.contains(e.target)) setOpen(false); });
  document.addEventListener("keydown", e => {
    if (e.key === "Escape" && box.dataset.open === "1") { setOpen(false); lbtn.focus(); }
  });
  /* A choice made on the page is remembered; the browser's own language is not
     written down, so a later change of browser language is followed. */
  choose(pickLang(), false);
})();
