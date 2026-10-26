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
  ".operation-progress-line progress{display:block;flex:1;min-width:0;width:auto;height:13px;margin:0;appearance:none;-webkit-appearance:none;border:1px solid var(--bar-bd);border-radius:0;background:var(--bar-bg);overflow:hidden}"
  ".operation-progress-line progress::-webkit-progress-bar{background:var(--bar-bg)}"
  ".operation-progress-line progress::-webkit-progress-value{background:var(--bar-fill)}"
  ".operation-progress-line progress::-moz-progress-bar{background:var(--bar-fill)}"
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
  // Paginotto e-ink editorial/typewriter skin. Keep selectors aligned with
  // the existing markup so this remains a global, markup-preserving change.
  ":root{--bg:#f1f0eb;--card:#faf9f4;--line:#262624;--line-soft:#c9c7bd;--text:#1d1d1b;--muted:#696862;--link:#a9402d;--ok:#2d5f49;--okbg:#e4eee8;--warn:#8a5a00;--warnbg:#f5ead2;--danger:#a9402d;--pill-bg:#f1eee6;--pill-fg:#514d45;--pre-bg:#f5f3eb;--stat-bg:#faf9f4;--bar-bg:#deddd5;--bar-bd:#262624;--bar-fill:#b7462f;--ban-ok-bd:#9bb8a7;--ban-warn-bd:#c5a76a;--btn-bg:#222220;--btn-fg:#fff;--btn-sec-bg:transparent;--btn-sec-fg:#1d1d1b;--btn-sec-bd:#a9aaa2;--inp-bd:#8f918b;--inp-bg:#faf9f4}"
  "body{background:var(--bg);color:var(--text);font:15px/1.5 'Courier New',Courier,monospace;letter-spacing:-.015em}"
  ".wrap{max-width:820px;padding:28px 24px 42px}.wrap.wide{max-width:1020px}"
  ".top{gap:24px;margin-bottom:0;padding:20px 0 18px;border-bottom:2px solid var(--line)}"
  ".brand{font:700 36px/.95 Georgia,'Times New Roman',serif;letter-spacing:.02em;color:var(--text);text-transform:uppercase}"
  ".top h1,.card h2,.book-title{font-family:Georgia,'Times New Roman',serif;letter-spacing:-.02em}"
  ".top h1{font-size:27px;text-transform:uppercase}.top a,.link{color:var(--link)}"
  ".top-side{gap:10px}.muted,.meta,.hint{font-family:'Courier New',Courier,monospace;color:var(--muted);font-size:12px}"
  ".nav-bar{margin:0 0 18px;padding:13px 0;border-bottom:1px solid var(--line-soft)}"
  ".nav-pages{gap:13px 26px}.nav a{color:var(--text);font:600 12px 'Courier New',Courier,monospace;text-transform:uppercase;letter-spacing:.04em}.nav a.active{color:var(--link);text-decoration:underline;text-decoration-thickness:2px;text-underline-offset:6px}"
  ".card{background:var(--card);border:1px solid var(--line);border-radius:0;padding:19px 20px;margin-bottom:16px;box-shadow:none}"
  ".book-list{gap:0}.book-list li.book-item{padding:17px 0;border:0;border-top:1px solid var(--line-soft);border-radius:0;background:transparent}.book-list li.book-item:first-child{padding-top:17px;border-top:0}"
  ".book-title{color:var(--text);font-weight:700;font-size:20px}.book-actions{border-top:1px solid var(--line-soft)}"
  ".tree-root,.tree-folder,.stat,.review-summary,.wifi-scan-item{border-radius:0;background:var(--stat-bg)}"
  ".pill{border:1px solid var(--line-soft);border-radius:0;background:transparent;padding:5px 8px}"
  ".pre{border-radius:0;background:var(--pre-bg)}"
  ".banner-ok,.banner-warn{border-radius:0;padding:12px 14px}.banner-ok{border-left:4px solid var(--ok)}.banner-warn{border-left:4px solid var(--warn)}"
  ".bar{height:13px;border-radius:0;background:var(--bar-bg);border:1px solid var(--bar-bd)}"
  ".status{border-radius:0}.review-check{border-radius:0}.review-head{border-bottom:1px solid var(--line-soft)}"
  "button,.btn{border-radius:0;background:var(--btn-bg);color:var(--btn-fg);font:600 12px 'Courier New',Courier,monospace;text-transform:uppercase;letter-spacing:.025em}"
  ".btn.secondary{background:transparent;color:var(--btn-sec-fg);border:1px solid var(--btn-sec-bd)}"
  "input:not([type=hidden]):not([type=checkbox]):not([type=radio]):not([type=range]),select,textarea{border-radius:0;background:var(--inp-bg);font-family:'Courier New',Courier,monospace}"
  ".operation-overlay{background:rgba(29,29,27,.42)}.operation-dialog{border-radius:0;box-shadow:3px 3px 0 rgba(29,29,27,.18)}"
  /* Screensaver editor: page-specific layout, global editorial skin. */
  ".ss-wrap{display:grid;gap:12px}.ss-card{border:1px solid var(--line-soft);border-radius:0;padding:10px 11px;background:var(--stat-bg)}"
  ".ss-grid{display:grid;gap:10px;grid-template-columns:repeat(2,minmax(0,1fr))}.ss-grid .full{grid-column:1/-1}.ss-grid > div{min-width:0}"
  ".ss-adv{border:1px solid var(--line);border-radius:0;padding:8px 10px;background:var(--card)}.ss-adv summary{cursor:pointer;font-weight:600;list-style:none}.ss-adv summary::-webkit-details-marker{display:none}.ss-adv summary:after{content:'\\25BE';float:right;color:var(--muted)}.ss-adv[open] summary:after{content:'\\25B4'}.ss-adv-body{margin-top:10px}"
  ".ss-label-row{display:flex;justify-content:space-between;align-items:center;gap:8px;margin:0 0 6px}.ss-value{font-size:12px;color:var(--muted);white-space:nowrap}.ss-preview-wrap{display:flex;flex-direction:column;gap:8px;min-height:240px}.ss-preview-stage{flex:1;display:flex;align-items:center;justify-content:center;min-height:140px}"
  ".ss-preview-stage canvas{width:100%;max-width:520px;border:1px solid var(--line);border-radius:0;background:#fff;image-rendering:pixelated;touch-action:none}.ss-meta,.ss-status{font-size:12px;color:var(--muted)}"
  ".ss-slots{display:grid;grid-template-columns:repeat(auto-fill,minmax(120px,1fr));gap:10px}.ss-slot{border:1px solid var(--line-soft);border-radius:0;padding:8px;background:var(--stat-bg);display:flex;flex-direction:column;gap:6px;align-items:center}.ss-slot-actions{display:flex;gap:8px;align-items:center;justify-content:center;width:100%;margin-top:4px}"
  ".btn-icon{display:inline-flex;align-items:center;justify-content:center;width:42px;height:42px;padding:0;margin:0;border:1px solid var(--line);border-radius:0;background:var(--card);color:inherit;text-decoration:none;cursor:pointer;transition:background .2s ease}.ss-reset{align-self:flex-start}.btn-icon svg{width:18px;height:18px;stroke:currentColor;fill:none;stroke-width:2;stroke-linecap:round;stroke-linejoin:round}.btn-icon:hover{background:var(--line-soft)}.btn-icon.danger{color:var(--danger);border-color:var(--danger)}"
  ".ss-slot img{width:100%;height:auto;border:1px solid var(--line);border-radius:0;background:#fff;image-rendering:pixelated}.ss-slot .ss-slot-empty{width:100%;aspect-ratio:250/122;display:flex;align-items:center;justify-content:center;border:1px dashed var(--line);border-radius:0;color:var(--muted);font-size:12px}"
  /* Read / Edit & Jump: keep the same paper, ink and square controls. */
  ".bv-find{display:flex;flex-wrap:wrap;gap:8px;align-items:center;margin-top:8px}.bv-find input[type=search]{flex:1;min-width:160px}.bv-find-actions{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;align-items:stretch;width:100%}.bv-find-stack{display:grid;gap:8px;min-width:0}.bv-find-actions button{width:100%;white-space:nowrap}.bv-search-panel{margin-top:14px}.bv-top-actions{display:flex;align-items:flex-start;justify-content:space-between;gap:10px}.bv-page-jump .bv-page-form{display:grid;grid-template-columns:repeat(2,minmax(0,1fr));gap:8px;align-items:center}.bv-page-jump .bv-page-form input{max-width:none}.bv-page-jump .bv-page-form button{width:100%}.bv-page-jump .bv-page-form .bv-page-hint{grid-column:1/-1}.bv-find .small{min-height:42px;padding:10px 14px;font-size:14px}"
  ".bv-status{margin-top:8px;font-size:13px;color:var(--muted)}.bv-text{margin-top:14px;max-height:60vh;overflow:auto;padding:12px;border:1px solid var(--line);border-radius:0;background:var(--pre-bg);white-space:pre-wrap;line-height:1.5;font:14px/1.5 'Courier New',Courier,monospace}.bv-editor{width:100%;min-height:60vh;max-height:none;margin-top:14px;resize:vertical;overflow:auto;overscroll-behavior:contain;font:14px/1.5 'Courier New',Courier,monospace}.bv-edit-actions{display:flex;flex-wrap:wrap;gap:8px;margin-top:10px}.find-hit{background:#e5d98a;color:var(--text);padding:0 1px;border-radius:0}.find-cur{background:#c66a4f;color:#fff}"
  ".tree-mark,.tree-book-mark{width:18px;height:1em;display:inline-block;position:relative;flex:none;color:var(--link)}.tree-library-mark:before{content:'';position:absolute;left:2px;top:.18em;width:13px;height:.62em;border:1px solid currentColor;background:linear-gradient(to bottom,transparent 30%,currentColor 31%,currentColor 39%,transparent 40%,transparent 64%,currentColor 65%,currentColor 73%,transparent 74%)}.tree-folder-mark:before{content:'';position:absolute;left:1px;top:.28em;width:15px;height:.58em;border:1px solid currentColor}.tree-folder-mark:after{content:'';position:absolute;left:2px;top:.16em;width:6px;height:.18em;border:1px solid currentColor;border-bottom:0}.tree-book-mark:before{content:'';position:absolute;left:3px;top:.16em;width:11px;height:.68em;border:1px solid currentColor;background:linear-gradient(90deg,transparent 44%,currentColor 45%,currentColor 55%,transparent 56%)}"
  "@media(max-width:640px){.bv-search-panel{position:sticky;top:0;z-index:3;padding:8px 0 10px;background:var(--card);border-bottom:1px solid var(--line-soft)}.bv-search-row{display:grid;grid-template-columns:1fr}.bv-search-row input[type=search]{min-width:0}.bv-find-actions{grid-template-columns:repeat(2,minmax(0,1fr))}.bv-editor{height:calc(100dvh - 210px);min-height:280px;font-size:16px;line-height:1.5}.bv-edit-actions{position:sticky;bottom:0;z-index:2;padding:10px 0;background:var(--card);border-top:1px solid var(--line-soft)}}"
  "@media(max-width:360px){.bv-find-actions{grid-template-columns:1fr}}@media(max-width:560px){.ss-grid{grid-template-columns:1fr}}"
  "@media(max-width:640px){.wrap{padding:16px 12px 28px}.top{padding-top:12px}.nav-pages{gap:13px 18px}}"
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
        "<link rel='stylesheet' href='/style.css?v=11'>";
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
