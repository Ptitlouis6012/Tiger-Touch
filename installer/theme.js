/* Light or dark, the same on every page of the site: the button flips it and
   the choice is kept in this browser. */
/* ---------- light / dark ------------------------------------------------- */
(function () {
  const root = document.documentElement;
  const mq = window.matchMedia("(prefers-color-scheme: light)");
  const shown = () => root.dataset.theme || (mq.matches ? "light" : "dark");
  const paint = () => { root.dataset.shown = shown(); };
  paint();
  mq.addEventListener("change", paint);
  document.getElementById("themebtn").addEventListener("click", () => {
    const next = shown() === "light" ? "dark" : "light";
    root.dataset.theme = next;
    try { localStorage.setItem("tigertouch-theme", next); } catch (e) {}
    paint();
  });
})();

/* "TigerTouch" in two colours, joined, in every title - also after the
   installer page translates them (it calls this again). */
window.ttBrandify = function () {
  document.querySelectorAll("h1, h2, h3").forEach(h => {
    if (h.querySelector(".tt-touch") || !/Tiger Touch/.test(h.textContent)) return;
    const walk = document.createTreeWalker(h, NodeFilter.SHOW_TEXT);
    const nodes = [];
    while (walk.nextNode()) nodes.push(walk.currentNode);
    nodes.forEach(n => {
      const i = n.data.indexOf("Tiger Touch");
      if (i < 0) return;
      const after = n.splitText(i);
      after.data = after.data.slice("Tiger Touch".length);
      const span = document.createElement("span");
      span.className = "tt-touch";
      span.textContent = "Touch";
      n.parentNode.insertBefore(document.createTextNode("Tiger"), after);
      n.parentNode.insertBefore(span, after);
    });
  });
};
window.ttBrandify();
