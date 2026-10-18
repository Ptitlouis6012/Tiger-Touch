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
      const day = v.released ? new Date(v.released) : null;
      const when = day && !isNaN(day)
        ? ", released " + day.toLocaleDateString("en-GB", { day: "numeric", month: "long", year: "numeric" })
        : "";
      const notes = v.notes_url || "https://github.com/TigerTag-Project/Tiger-Touch/releases/latest";
      els.forEach(el => {
        el.textContent = "";
        el.append("Latest version ");
        const b = document.createElement("b");
        b.textContent = "v" + v.version;
        el.append(b, when + " · ");
        const a = document.createElement("a");
        a.href = notes;
        a.textContent = "What's new";
        el.append(a);
        el.hidden = false;
      });
    })
    .catch(() => {});
})();
