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
