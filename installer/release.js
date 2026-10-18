/* The latest release, read when the page opens: version.json is the manifest
   every Tiger Touch reads, published with each release beside these pages. No
   page holds a version of its own, so a release changes nothing to edit. */
(function () {
  const els = document.querySelectorAll("[data-latest]");
  if (!els.length) return;
  /* The site's root, from this script's own address: the same from any page. */
  const root = new URL(".", document.currentScript ? document.currentScript.src : location.href);
  fetch(new URL("version.json", root), { cache: "no-cache" })
    .then(r => (r.ok ? r.json() : Promise.reject(r.status)))
    .then(v => {
      if (!v || !v.version) return;
      const draw = () => {
        /* In the page's language (lang.js), English when it is not there. */
        const w = k => (window.ttWord && window.ttWord(k)) || { rel_latest: "Latest version",
          rel_released: "released {date}", f_releases: "What's new" }[k];
        const day = v.released ? new Date(v.released) : null;
        let when = "";
        if (day && !isNaN(day)) {
          let date;
          try {
            date = day.toLocaleDateString(document.documentElement.lang === "en" ? "en-GB" : document.documentElement.lang,
              { day: "numeric", month: "long", year: "numeric" });
          } catch (e) { date = day.toISOString().slice(0, 10); }
          when = ", " + w("rel_released").split("{date}").join(date);
        }
        const notes = v.notes_url || "https://github.com/TigerTag-Project/Tiger-Touch/releases/latest";
        els.forEach(el => {
          el.textContent = "";
          el.append(w("rel_latest") + " ");
          const b = document.createElement("b");
          b.textContent = "v" + v.version;
          el.append(b, when + " · ");
          const a = document.createElement("a");
          a.href = notes;
          a.textContent = w("f_releases");
          el.append(a);
          el.hidden = false;
        });
      };
      draw();
      document.addEventListener("tt-lang", draw);
    })
    .catch(() => {});
})();
