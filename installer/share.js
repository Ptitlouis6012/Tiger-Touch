/* "Share this page": each button opens the site's own share page with this
   page's address in the reader's language and its title; "Copy the link" copies
   it; "More…" is the phone's own share sheet, shown where there is one. Nothing
   is sent anywhere until the reader picks a site. */
(function () {
  const bar = document.querySelector(".share");
  if (!bar) return;
  const canon = document.querySelector('link[rel="canonical"]');
  const url = () => (canon ? canon.href : location.href.split("#")[0]);
  const title = () => document.title;
  const enc = encodeURIComponent;
  const TARGETS = {
    whatsapp: () => "https://wa.me/?text=" + enc(title() + " " + url()),
    telegram: () => "https://t.me/share/url?url=" + enc(url()) + "&text=" + enc(title()),
    x: () => "https://x.com/intent/post?text=" + enc(title()) + "&url=" + enc(url()),
    bluesky: () => "https://bsky.app/intent/compose?text=" + enc(title() + " " + url()),
    facebook: () => "https://www.facebook.com/sharer/sharer.php?u=" + enc(url()),
    reddit: () => "https://www.reddit.com/submit?url=" + enc(url()) + "&title=" + enc(title()),
    linkedin: () => "https://www.linkedin.com/sharing/share-offsite/?url=" + enc(url()),
    mail: () => "mailto:?subject=" + enc(title()) + "&body=" + enc(url()),
  };
  /* The addresses are written when the page opens and again when the language
     changes (lang.js), so a link always carries the page as it is read. */
  const fill = () => bar.querySelectorAll("a[data-share]").forEach(a => {
    const f = TARGETS[a.dataset.share];
    if (f) a.href = f();
  });
  fill();
  document.addEventListener("tt-lang", fill);
  const word = (k, en) => (window.ttWord && window.ttWord(k)) || en;
  const copy = bar.querySelector("[data-share-copy]");
  if (copy) {
    copy.addEventListener("click", () => {
      navigator.clipboard.writeText(url()).then(() => {
        const label = copy.querySelector("span");
        label.textContent = word("share_copied", "Link copied");
        setTimeout(() => { label.textContent = word("share_copy", "Copy the link"); }, 1600);
      }).catch(() => {});
    });
  }
  const more = bar.querySelector("[data-share-native]");
  if (more && navigator.share) {
    more.hidden = false;
    more.addEventListener("click", () => navigator.share({ title: title(), url: url() }).catch(() => {}));
  }
})();
