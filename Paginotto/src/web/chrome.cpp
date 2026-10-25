#include "src/web/chrome.h"

#include <string.h>

#include "src/state.h"            // server, FS
#include "src/storage/fs_util.h"  // fs*BytesSafe
#include "src/storage/library.h"  // g_library
#include "src/web/upload.h"       // notifyUploadActivity

// ============================================================================
//  Stylesheet — served as /style.css. Browsers cache for an hour, so visiting
//  multiple pages doesn't re-download. Bumping CSS requires a refresh on the
//  client (or a wait); acceptable for a personal-device UI.
//
//  All colors use the single light palette.
// ============================================================================
static const char kStyleCss[] PROGMEM =
  ":root{--bg:#f3efe7;--card:#fff;--line:#ddd4c7;--line-soft:#ece5d9;--text:#1f2328;--muted:#667085;--link:#3c5a7a;"
  "--ok:#216e39;--okbg:#e7f6ec;--warn:#8a5a00;--warnbg:#fff4d6;--danger:#b42318;"
  "--pill-bg:#f6f2ea;--pill-fg:#6b6358;--pre-bg:#fcfaf7;--stat-bg:#fcfaf7;"
  "--bar-bg:#ece5d9;--bar-bd:#e0d7ca;--bar-fill:#3c5a7a;"
  "--ban-ok-bd:#cfe9d7;--ban-warn-bd:#ecd9a3;"
  "--btn-bg:#1f2328;--btn-fg:#fff;--btn-sec-bg:#eef2f6;--btn-sec-fg:#334e68;--btn-sec-bd:#d8e0e8;"
  "--inp-bd:#c9c2b8;--inp-bg:#fff}"
  "*{box-sizing:border-box}"
  "body{margin:0;background:var(--bg);color:var(--text);font:15px/1.45 system-ui,sans-serif}"
  ".wrap{max-width:820px;margin:0 auto;padding:18px}"
  ".wide{max-width:1020px}"
  ".top{display:flex;width:100%;align-items:flex-start;gap:12px;margin-bottom:10px}"
  ".top-title{min-width:0;flex:1}"
  ".title-exit{margin-left:auto;white-space:nowrap;flex:none}"
  ".top a,.link{color:var(--link);text-decoration:none}"
  ".brand{font-size:13px;font-weight:750;letter-spacing:.08em;text-transform:uppercase;color:var(--link);margin-bottom:2px}"
  ".top a:hover,.link:hover{text-decoration:underline}"
  ".top-side{display:flex;flex:1;flex-wrap:wrap;gap:10px 14px;align-items:center;justify-content:flex-end}"
  ".muted{color:var(--muted);font-size:13px}"
  ".card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:14px 15px;margin:0 0 14px;box-shadow:0 1px 0 rgba(0,0,0,.03)}"
  ".grid{display:grid;gap:12px}"
  ".actions{display:flex;flex-wrap:wrap;gap:10px;align-items:center;margin-top:14px}"
  ".nav{display:flex;align-items:center;width:100%;font-size:14px}"
  ".nav-bar{width:100%;margin:0 0 18px}"
  ".nav-pages{display:flex;flex-wrap:wrap;gap:10px 14px;align-items:center}"
  ".exit-link{margin-left:auto;white-space:nowrap}"
  ".nav a{color:var(--link);text-decoration:none}"
  ".list{list-style:none;padding:0;margin:0}"
  ".list li{padding:11px 0;border-top:1px solid var(--line-soft)}"
  ".list li:first-child{border-top:0;padding-top:0}"
  ".book-list{display:grid;gap:14px}"
  ".book-list li.book-item{padding:16px;border:1px solid var(--line);border-radius:12px;background:var(--stat-bg)}"
  ".book-list li.book-item:first-child{padding-top:16px;border-top:1px solid var(--line)}"
  ".book-item{display:flex;flex-direction:column;min-height:150px}"
  ".book-title-line,.book-title-view{display:flex;align-items:flex-start;gap:8px;justify-content:space-between}"
  ".book-title-view h3{min-width:0;flex:1}"
  ".book-title,.book-title-view h3,.card h2{overflow-wrap:anywhere;word-break:break-word}"
  ".icon-btn{min-width:42px;min-height:42px;padding:10px;background:transparent;color:var(--link);font-size:18px;line-height:20px;border:1px solid var(--line);flex:none}"
  ".book-actions{display:flex;justify-content:space-between;align-items:flex-end;gap:12px;margin-top:auto;padding-top:14px;border-top:1px solid var(--line-soft)}"
  ".book-actions-left,.book-actions-right{display:flex;align-items:center;flex-wrap:wrap;gap:8px;min-width:0}"
  ".book-folder-form{display:flex;align-items:center;flex-wrap:wrap;gap:8px;margin:0}"
  ".book-folder-form select{width:auto;min-width:130px}"
  ".library-tree{display:grid;gap:10px;margin-top:12px}"
  ".tree-root,.tree-folder{border:1px solid var(--line-soft);border-radius:12px;background:var(--stat-bg)}"
  ".tree-root-header,.tree-folder-header{display:flex;align-items:center;justify-content:space-between;gap:10px;padding:11px 12px}"
  ".tree-label{display:flex;align-items:center;gap:8px;min-width:0;font-weight:700}"
  ".tree-label span:last-child{min-width:0;overflow-wrap:anywhere;word-break:break-word}"
  ".tree-mark{color:var(--link);font-weight:400;flex:none}"
  ".tree-children{padding:0 12px 10px 30px}"
  ".tree-book{display:grid;grid-template-columns:18px minmax(0,1fr) minmax(0,8.5em);align-items:start;gap:8px;padding:6px 0;border-top:1px solid var(--line-soft)}"
  ".tree-book-mark{display:inline-block;width:18px;height:1em;flex:none;position:relative;color:var(--link);transform-origin:center}"
  ".tree-book-mark::before{content:'';position:absolute;left:5px;top:.28em;border-top:.34em solid transparent;border-bottom:.34em solid transparent;border-left:.48em solid currentColor}"
  ".tree-book:hover .tree-book-mark,.tree-book:focus-within .tree-book-mark{transform:scale(1.08)}"
  ".tree-book .muted{margin-left:0;text-align:right;overflow-wrap:anywhere;word-break:break-word}"
  ".tree-book a{min-width:0;color:var(--link);overflow-wrap:anywhere;word-break:break-word;text-decoration:none}"
  ".tree-book a:hover{text-decoration:underline}"
  ".tree-empty{padding:6px 0;color:var(--muted);font-size:13px}"
  ".tree-create{padding:0 0 14px;margin:0 0 12px;border-bottom:1px solid var(--line-soft)}"
  ".row{display:flex;justify-content:space-between;gap:12px;align-items:flex-start}"
  ".book-row{display:flex;gap:16px;align-items:flex-start}"
  ".book-main{min-width:0;flex:1}"
  ".book-title{color:#334e68;font-weight:750;letter-spacing:.01em}"
  ".book-tools{display:flex;flex-direction:column;gap:6px;align-items:stretch;flex:none}"
  ".meta{color:var(--muted);font-size:13px}"
  ".pill{display:inline-block;background:var(--pill-bg);color:var(--pill-fg);border-radius:999px;padding:3px 8px;font-size:12px}"
  ".pre{white-space:pre-wrap;line-height:1.45;padding:12px;border:1px solid var(--line);border-radius:10px;background:var(--pre-bg)}"
  ".btn.danger,button.danger,.btn-icon.danger{background:var(--danger);color:#fff}"
  ".banner-ok{background:var(--okbg);color:var(--ok);border:1px solid var(--ban-ok-bd);border-radius:12px;padding:12px 13px;margin-bottom:14px}"
  ".banner-warn{background:var(--warnbg);color:var(--warn);border:1px solid var(--ban-warn-bd);border-radius:12px;padding:12px 13px;margin-bottom:14px}"
  ".stats{display:grid;gap:10px;grid-template-columns:repeat(2,minmax(0,1fr));margin-top:12px}"
  ".stat{padding:11px 12px;border:1px solid var(--line-soft);border-radius:12px;background:var(--stat-bg)}"
  ".stat b{display:block;font-size:17px;line-height:1.2;margin-top:2px;overflow-wrap:anywhere;word-break:break-word}"
  ".bar{height:10px;border-radius:999px;background:var(--bar-bg);overflow:hidden;border:1px solid var(--bar-bd);margin-top:12px}"
  ".bar > span{display:block;height:100%;background:var(--bar-fill)}"
  ".stack{display:grid;gap:8px}"
  ".small{font-size:13px}"
  "label{display:block;font-weight:600;margin:0 0 6px}"
  ".hint{margin:6px 0 0;color:var(--muted);font-size:12px}"
  ".status{padding:10px 12px;border-radius:10px;font-size:14px;margin:10px 0 0}"
  ".status.ok{background:var(--okbg);color:var(--ok)}"
  ".status.idle{background:var(--pill-bg);color:var(--pill-fg)}"
  ".review-card{padding:18px}"
  ".review-head{display:flex;align-items:center;gap:12px;padding-bottom:16px;border-bottom:1px solid var(--line-soft)}"
  ".review-check{display:grid;place-items:center;width:38px;height:38px;border-radius:50%;background:var(--okbg);color:var(--ok);font-size:22px;font-weight:800;flex:none}"
  ".review-summary{display:flex;align-items:center;gap:10px;margin:16px 0;padding:12px;background:var(--stat-bg);border:1px solid var(--line-soft);border-radius:12px;min-width:0}"
  ".review-summary strong{min-width:0;overflow-wrap:anywhere;word-break:break-word}"
  ".review-form{max-width:620px}"
  ".review-actions-top{padding-top:6px;margin-bottom:6px}"
  ".review-actions-bottom{justify-content:flex-end;padding-top:14px;margin-top:16px;border-top:1px solid var(--line-soft)}"
  ".operation-overlay{position:fixed;inset:0;z-index:1000;display:grid;place-items:center;padding:20px;background:rgba(31,35,40,.42)}"
  ".operation-dialog{width:min(560px,100%);padding:24px;background:var(--card);border:1px solid var(--line);border-radius:16px;box-shadow:0 12px 40px rgba(0,0,0,.2);text-align:center}"
  ".operation-dialog h2{margin:0 0 8px}"
  ".operation-dialog p{margin:0 0 20px;color:var(--muted)}"
  ".operation-progress-line{display:flex;align-items:center;gap:14px;min-width:0;margin:8px 0 10px}"
  ".operation-progress-line progress{display:block;flex:1;min-width:0;width:auto;height:12px}"
  ".operation-progress-line strong{min-width:46px;text-align:right;font-size:16px;font-variant-numeric:tabular-nums}"
  ".operation-dialog .operation-status{margin:0 0 22px;color:var(--muted);font-size:14px;min-height:20px}"
  ".wifi-scan-list{display:grid;gap:8px;margin-top:10px}"
  ".wifi-scan-item{display:flex;align-items:center;justify-content:space-between;padding:10px 12px;border:1px solid var(--line-soft);border-radius:10px;background:var(--stat-bg)}"
  "button,.btn{display:inline-flex;align-items:center;justify-content:center;min-height:42px;box-sizing:border-box;border:0;border-radius:10px;background:var(--btn-bg);color:var(--btn-fg);padding:10px 14px;font:600 14px/20px system-ui,sans-serif;text-decoration:none;cursor:pointer}"
  ".btn.secondary{background:var(--btn-sec-bg);color:var(--btn-sec-fg);border:1px solid var(--btn-sec-bd)}"
  "input:not([type=hidden]):not([type=checkbox]):not([type=radio]):not([type=range]),select,textarea{width:100%;min-height:42px;box-sizing:border-box;border:1px solid var(--inp-bd);border-radius:10px;background:var(--inp-bg);color:var(--text);padding:10px;font:inherit}"
  "input[type=checkbox],input[type=radio]{accent-color:var(--link)}"
  ".nav a.active{font-weight:700;color:var(--text);text-decoration:underline;text-underline-offset:3px}"
  "[hidden]{display:none!important}"
  "h1,h2,h3,p{margin:0}"
  "h1,h2,h3{margin-bottom:6px}"
  "p + p{margin-top:10px}"
  "@media(min-width:760px){.stats{grid-template-columns:repeat(4,minmax(0,1fr))}}"
  "@media(min-width:620px){.grid.cols-2{grid-template-columns:1fr 1fr}}"
  ".book-actions-left .btn,.book-actions-right .btn{white-space:nowrap}"
  "@media(max-width:640px){.row,.book-row{flex-direction:column}.book-tools{align-self:stretch;flex-direction:row;flex-wrap:wrap}.book-actions{align-items:stretch;flex-direction:column}.book-actions-left,.book-actions-right{width:100%}.book-actions-left{display:grid;grid-template-columns:minmax(0,1fr);align-items:stretch}.book-actions-right{display:grid;grid-template-columns:1fr 1fr}.book-actions-right form,.book-actions-right .btn{width:100%}.book-folder-form{display:grid;grid-template-columns:minmax(0,1fr) auto;align-items:center}.book-folder-form select{min-width:0}.wrap{padding:14px}}"
  "@media(max-width:640px){input:not([type=hidden]):not([type=checkbox]):not([type=radio]):not([type=range]),select,textarea{font-size:16px!important}}"
  ;

static void handleStyleCss() {
  server.sendHeader("Cache-Control", "public, max-age=3600");
  server.send_P(200, "text/css; charset=utf-8", kStyleCss);
}

void registerChromeRoutes() {
  server.on("/style.css", HTTP_GET, handleStyleCss);
}

// ============================================================================
//  Page chrome
// ============================================================================
String webPageStart(const String& title, const String& subtitle,
                    const String& navHtml, bool wide) {
  notifyUploadActivity();
  const bool hasNav = navHtml.length() > 0;
  String out;
  out.reserve(1400);
  out = "<!doctype html><html><head><meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta name='color-scheme' content='light'>"
        "<link rel='stylesheet' href='/style.css?v=9'>";
  out += "<title>";
  out += title;
  out += "</title></head><body><div class='wrap";
  if (wide) out += " wide";
  out += "'><div class='top'><div class='top-title'><div class='brand'>Paginotto</div><h1>";
  out += title;
  out += "</h1><div class='muted'>";
  out += subtitle;
  out += "</div></div>";
  if (hasNav) {
    out += "<a class='btn secondary small title-exit' href='/exit' onclick=\"return !window.paginottoDirty || confirm('There may be unsaved changes or an active operation. Exit anyway?')\">Exit WebUI</a>";
  }
  out += "</div>";
  if (hasNav) {
    out += "<div class='nav-bar'><div class='nav'>";
    out += navHtml;
    out += "</div></div>";
  }
  return out;
}

String webPageEnd() {
  return String("</div></body></html>");
}

String webNav(const char* active, bool showExit) {
  const char* keys[] = {"books", "screensavers", "settings", "wifi", "diagnostics"};
  const char* labels[] = {
    D_WEB_NAV_BOOKS, D_WEB_NAV_SCREENSAVER, D_WEB_NAV_SETTINGS, "Wi-Fi", "Diagnostics"
  };
  const char* urls[] = {"/books", "/screensavers", "/settings", "/wifi", "/diagnostics"};

  String out = "<span class='nav-pages'>";
  for (int i = 0; i < 5; i++) {
    out += "<a href='";
    out += urls[i];
    out += "'";
    if (active && strcmp(active, keys[i]) == 0) out += " class='active' aria-current='page'";
    out += ">";
    out += labels[i];
    out += "</a>";
  }
  out += "</span>";
  if (showExit) out += "<a class='btn secondary small exit-link' href='/exit' onclick=\"return !window.paginottoDirty || confirm('There may be unsaved changes or an active operation. Exit anyway?')\">Exit WebUI</a>";
  return out;
}

String webBackLink(const char* href, const char* label) {
  return String("<a class='btn secondary small' href='") + href + "'>&#8592; " + label + "</a>";
}

String successPage(const String& title, const String& subtitle,
                   const String& banner, const String& innerHtml,
                   const char* active) {
  String out = webPageStart(title, subtitle, webNav(active));
  out += "<div class='banner-ok'>" + banner + "</div>";
  out += innerHtml;
  out += webPageEnd();
  return out;
}

// ============================================================================
//  Small helpers
// ============================================================================
String htmlEscape(const String& in) {
  String out;
  out.reserve(in.length() + 16);
  for (size_t i = 0; i < in.length(); i++) {
    char c = in[i];
    if      (c == '&')  out += "&amp;";
    else if (c == '<')  out += "&lt;";
    else if (c == '>')  out += "&gt;";
    else if (c == '"')  out += "&quot;";
    else if (c == '\'') out += "&#39;";
    else                out += c;
  }
  return out;
}

String humanBytes(size_t bytes) {
  if (bytes < 1024) return String(bytes) + " B";
  if (bytes < (1024UL * 1024UL)) return String(bytes / 1024.0f, 1) + " KB";
  return String(bytes / 1024.0f / 1024.0f, 2) + " MB";
}

int storageUsedPct() {
  size_t totalBytes = fsTotalBytesSafe();
  size_t usedBytes  = fsUsedBytesSafe();
  if (totalBytes == 0) return 0;
  int pct = (int)((usedBytes * 100UL) / totalBytes);
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
  return pct;
}

String storageCardHtml(const char* title) {
  size_t totalBytes = fsTotalBytesSafe();
  size_t usedBytes  = fsUsedBytesSafe();
  size_t freeBytes  = fsFreeBytesSafe();
  int    pct        = storageUsedPct();

  String out;
  out.reserve(900);
  out += "<div class='card'><h2>";
  out += title;
  out += "</h2><div class='stats'>";
  out += "<div class='stat'><span class='muted'>" D_WEB_STORAGE_BOOKS "</span><b>" + String(g_library.bookCount) + "</b></div>";
  out += "<div class='stat'><span class='muted'>" D_WEB_STORAGE_USED  "</span><b>" + humanBytes(usedBytes)         + "</b></div>";
  out += "<div class='stat'><span class='muted'>" D_WEB_STORAGE_FREE  "</span><b>" + humanBytes(freeBytes)         + "</b></div>";
  out += "<div class='stat'><span class='muted'>" D_WEB_STORAGE_TOTAL "</span><b>" + humanBytes(totalBytes)        + "</b></div>";
  out += "</div><div class='bar'><span style='width:" + String(pct) + "%'></span></div>";
  out += "<div class='muted' style='margin-top:8px'>" + String(pct) + D_WEB_STORAGE_PCT_SUFFIX "</div></div>";
  return out;
}
