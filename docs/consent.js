// Google Analytics (G-SM5ED80EJM) with a consent banner for visitors in Europe. Europe is guessed from the browser's
// time zone; there, nothing is loaded from Google until the visitor accepts. The choice is kept in localStorage.
// Pages include this with <script src="consent.js" defer></script> (../consent.js from docs/demo/).
(function () {
  var ID = 'G-SM5ED80EJM', KEY = 'sj-analytics';

  function load() {
    if (window.gtag) return;
    var s = document.createElement('script');
    s.async = true;
    s.src = 'https://www.googletagmanager.com/gtag/js?id=' + ID;
    document.head.appendChild(s);
    window.dataLayer = window.dataLayer || [];
    window.gtag = function () { dataLayer.push(arguments); };
    gtag('js', new Date());
    gtag('config', ID, { anonymize_ip: true });
  }

  function get() { try { return localStorage.getItem(KEY); } catch (e) { return null; } }
  function set(v) { try { localStorage.setItem(KEY, v); } catch (e) {} }

  function inEurope() {
    try {
      var tz = Intl.DateTimeFormat().resolvedOptions().timeZone || '';
      return /^Europe\//.test(tz) || /^Atlantic\/(Reykjavik|Canary|Madeira|Azores|Faroe)$/.test(tz);
    } catch (e) { return true; }    // can't tell: ask
  }

  function banner() {
    var b = document.createElement('div');
    b.setAttribute('role', 'dialog');
    b.setAttribute('aria-label', 'Analytics cookies');
    b.style.cssText = 'position:fixed;left:16px;right:16px;bottom:16px;max-width:560px;margin:0 auto;z-index:9999;' +
      'background:#183626;color:#eaf2e6;border:1px solid #3c6a4c;border-radius:10px;padding:14px 16px;' +
      'font:15px/1.4 system-ui,sans-serif;box-shadow:0 6px 24px rgba(0,0,0,.4)';
    b.innerHTML = 'This site would like to use Google Analytics cookies to count visits. Nothing is shared until you choose.' +
      '<div style="margin-top:10px;display:flex;gap:8px;flex-wrap:wrap">' +
      '<button data-v="yes" style="background:#d8b44a;color:#10241a;border:0;border-radius:6px;padding:8px 14px;font:inherit;font-weight:600;cursor:pointer">Accept</button>' +
      '<button data-v="no" style="background:#3c6a4c;color:#eaf2e6;border:0;border-radius:6px;padding:8px 14px;font:inherit;font-weight:600;cursor:pointer">Decline</button></div>';
    b.addEventListener('click', function (e) {
      var v = e.target.getAttribute && e.target.getAttribute('data-v');
      if (!v) return;
      set(v);
      b.remove();
      if (v === 'yes') load();
      else if (window.gtag) gtag('consent', 'update', { analytics_storage: 'denied' });   // already counted outside Europe: stop for this visit
    });
    document.body.appendChild(b);
  }

  // "Analytics settings" links call this to ask again.
  window.sjAnalyticsSettings = function () {
    try { localStorage.removeItem(KEY); } catch (e) {}
    if (get() === null && !document.querySelector('[aria-label="Analytics cookies"]')) banner();
    return false;
  };

  var c = get();
  if (c === 'yes' || (c === null && !inEurope())) load();
  else if (c === null) {
    if (document.body) banner(); else document.addEventListener('DOMContentLoaded', banner);
  }
})();
