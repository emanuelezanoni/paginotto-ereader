#include "src/web/files.h"

#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#include "src/config.h"
#include "src/diagnostics/log.h"
#include "src/state.h"
#include "src/pure/hashing.h"
#include "src/pure/paths.h"
#include "src/storage/book_metadata.h"
#include "src/storage/fs_util.h"
#include "src/storage/library.h"
#include "src/storage/preferences_store.h"
#include "src/ui/text.h"            // pageOffsetForPage
#include "src/web/chrome.h"

// ============================================================================
//  /               — redirect to book/folder browser
//  /books          — book/folder browser
//  /download-book  — GET: stream book .txt as attachment (?id=N)
//  /download-url   — POST: stream a remote file through the device (?url=...)
//  /del            — POST: delete book
//  /mkdir          — POST: create folder
//  /rmdir          — POST: delete (empty) folder
//  /move           — POST: move book to a different folder
//  /jumppage       — POST: set the page that opens next on device
// ============================================================================

static void handleDownloadBook() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }
  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  String path = String(g_library.books[id].path);
  File f = FS.open(path, "r");
  if (!f) {
    server.send(404, "text/plain; charset=utf-8", "Not found");
    return;
  }

  String filename = lastPathComponent(path);
  server.setContentLength(f.size());
  server.sendHeader("Content-Disposition",
                    "attachment; filename=\"" + filename + "\"");
  server.send(200, "text/plain; charset=utf-8", "");

  WiFiClient client = server.client();
  uint8_t buf[512];
  while (f.available()) {
    size_t n = f.read(buf, sizeof(buf));
    if (n > 0) client.write(buf, n);
  }
  f.close();
}

// The remote EPUB is deliberately never written to LittleFS. It is fetched
// by the already-connected station interface and forwarded to the browser in
// small chunks, which also avoids the browser's cross-origin CORS restriction.
static constexpr uint32_t kRemoteBookMaxBytes = 64UL * 1024UL * 1024UL;
static constexpr uint16_t kRemoteBookTimeoutMs = 5000U;
// The proxy is synchronous, so a stalled upstream must have a bounded wait.
// Interrupted transfers are resumed by the browser with HTTP Range requests.
static constexpr uint32_t kRemoteBookStallMs = 30000UL;
// Keep the single-threaded WebServer recoverable when the browser socket
// stops consuming data. The browser resumes from the last received offset.
static constexpr uint32_t kRemoteBookWriteStallMs = 2000UL;

enum class RemoteBrowserWriteResult {
  complete,
  closed,
  timeout,
};

static void traceRemoteBookState(const char* event, uint32_t offset,
                                 int expected, uint32_t resumeOffset,
                                 uint32_t startedMs) {
  char detail[112];
  snprintf(detail, sizeof(detail),
           "remote_epub.%s offset=%lu expected=%ld resume=%lu fsfree=%lu elapsed=%lu",
           event, (unsigned long)offset, (long)expected,
           (unsigned long)resumeOffset, (unsigned long)fsFreeBytesSafe(),
           (unsigned long)(millis() - startedMs));
  // Runtime-only: do not write progress samples to NVS.
  Diagnostics::trace(detail);
}

static RemoteBrowserWriteResult writeRemoteBookChunk(
    WiFiClient& browser, const uint8_t* buffer, size_t length,
    uint32_t offset, uint32_t startedMs, size_t& written) {
  written = 0;
  uint32_t lastProgress = millis();
  while (written < length) {
    size_t n = browser.write(buffer + written, length - written);
    if (n > 0) {
      written += n;
      lastProgress = millis();
      if (written < length) {
        char detail[112];
        snprintf(detail, sizeof(detail),
                 "remote_epub.browser_short_write offset=%lu written=%lu/%lu elapsed=%lu",
                 (unsigned long)(offset + written), (unsigned long)written,
                 (unsigned long)length,
                 (unsigned long)(millis() - startedMs));
        Diagnostics::trace(detail);
      }
      continue;
    }

    if (millis() - lastProgress >= kRemoteBookWriteStallMs) {
      return browser.connected() ? RemoteBrowserWriteResult::timeout
                                 : RemoteBrowserWriteResult::closed;
    }
    delay(1);
    yield();
  }
  return RemoteBrowserWriteResult::complete;
}

static void configureRemoteBookClient(WiFiClientSecure& client) {
  // This endpoint accepts arbitrary user-supplied download hosts. The
  // certificate bundle path was crashing inside HTTPClient::GET() on the
  // target, before any response bytes were received. Keep the proxy's TLS
  // connection bounded and avoid loading the large CA bundle here. The EPUB
  // is still streamed directly to the browser and never stored on-device.
  client.setInsecure();
  client.setTimeout(kRemoteBookTimeoutMs);
}

static void releaseRemoteBookClient(WiFiClient* transport,
                                    WiFiClientSecure* secureClient) {
  if (transport) transport->stop();
  delete secureClient;
}

static bool parseRemoteUrl(const String& url, bool& secure, String& host,
                           uint16_t& port, String& path) {
  secure = url.startsWith("https://");
  const size_t authorityStart = secure ? 8 : 7;
  int pathStart = url.indexOf('/', authorityStart);
  String authority = pathStart < 0
                   ? url.substring(authorityStart)
                   : url.substring(authorityStart, pathStart);
  if (authority.length() == 0) return false;

  port = secure ? 443 : 80;
  int portSeparator = authority.lastIndexOf(':');
  if (portSeparator > 0) {
    int parsedPort = authority.substring(portSeparator + 1).toInt();
    if (parsedPort <= 0 || parsedPort > 65535) return false;
    port = (uint16_t)parsedPort;
    authority = authority.substring(0, portSeparator);
  }
  host = authority;
  path = pathStart < 0 ? "/" : url.substring(pathStart);
  return host.length() > 0;
}

static bool readRemoteLine(Stream& source, String& line) {
  line = "";
  uint32_t started = millis();
  while (millis() - started <= kRemoteBookStallMs) {
    while (source.available()) {
      char c = (char)source.read();
      if (c == '\n') return true;
      if (c != '\r') line += c;
      if (line.length() > 2048) return false;
    }
    delay(2);
    yield();
  }
  return false;
}

static void handleRemoteBookDownload() {
  const uint32_t remoteStartedMs = millis();
  Diagnostics::checkpoint("remote_epub.begin");
  if (WiFi.status() != WL_CONNECTED) {
    Diagnostics::warning("remote_epub.no_station");
    server.send(503, "text/plain; charset=utf-8",
                "Online EPUB import requires an Internet Wi-Fi connection; hotspot-only mode is unavailable");
    return;
  }

  if (!server.hasArg("url")) {
    Diagnostics::warning("remote_epub.missing_url");
    server.send(400, "text/plain; charset=utf-8", "Missing download URL");
    return;
  }

  String url = server.arg("url");
  if (!(url.startsWith("http://") || url.startsWith("https://"))) {
    Diagnostics::warning("remote_epub.bad_scheme");
    server.send(400, "text/plain; charset=utf-8", "Only HTTP(S) URLs are supported");
    return;
  }

  uint32_t resumeOffset = 0;
  if (server.hasArg("offset")) {
    String offsetText = server.arg("offset");
    if (offsetText.length() == 0) {
      server.send(400, "text/plain; charset=utf-8", "Invalid resume offset");
      return;
    }
    resumeOffset = strtoul(offsetText.c_str(), nullptr, 10);
  }
  traceRemoteBookState("request", 0, -1, resumeOffset, remoteStartedMs);

  bool secure = false;
  String host;
  String path;
  uint16_t port = 0;
  if (!parseRemoteUrl(url, secure, host, port, path)) {
    Diagnostics::warning("remote_epub.bad_url");
    server.send(400, "text/plain; charset=utf-8", "Invalid download URL");
    return;
  }

  WiFiClient plainClient;
  // WiFiClientSecure uses significant stack during TLS setup. The main loop
  // task has a small stack, so keep the TLS object off-stack like OTA does.
  WiFiClientSecure* secureClient = nullptr;
  WiFiClient* transport = &plainClient;
  if (secure) {
    secureClient = new WiFiClientSecure();
    if (!secureClient) {
      Diagnostics::error("remote_epub.tls_alloc_failed");
      server.send(503, "text/plain; charset=utf-8", "Not enough memory for secure download");
      return;
    }
    configureRemoteBookClient(*secureClient);
    transport = secureClient;
  }

  transport->setTimeout(kRemoteBookTimeoutMs);
  if (!transport->connect(host.c_str(), port)) {
    Diagnostics::error("remote_epub.connect_failed");
    releaseRemoteBookClient(transport, secureClient);
    server.send(502, "text/plain; charset=utf-8", "Could not connect to remote download");
    return;
  }
  transport->print("GET ");
  transport->print(path);
  transport->print(" HTTP/1.1\r\nHost: ");
  transport->print(host);
  transport->print("\r\nConnection: close\r\nUser-Agent: Paginotto\r\nAccept-Encoding: identity\r\n");
  if (resumeOffset > 0) {
    transport->print("Range: bytes=");
    transport->print(resumeOffset);
    transport->print("-\r\n");
  }
  transport->print("\r\n");

  String statusLine;
  if (!readRemoteLine(*transport, statusLine)) {
    Diagnostics::error("remote_epub.status_timeout");
    releaseRemoteBookClient(transport, secureClient);
    server.send(502, "text/plain; charset=utf-8", "Remote response timeout");
    return;
  }
  int firstSpace = statusLine.indexOf(' ');
  int secondSpace = firstSpace < 0 ? -1 : statusLine.indexOf(' ', firstSpace + 1);
  int code = (firstSpace >= 0)
           ? statusLine.substring(firstSpace + 1, secondSpace < 0 ? statusLine.length() : secondSpace).toInt()
           : 0;
  if ((resumeOffset == 0 && code != 200) ||
      (resumeOffset > 0 && code != 206)) {
    Diagnostics::errorCode("remote_epub.http_failed", code);
    releaseRemoteBookClient(transport, secureClient);
    server.send(resumeOffset > 0 && code == 200 ? 409 : 502,
                "text/plain; charset=utf-8",
                resumeOffset > 0 && code == 200
                  ? "Remote server does not support resumable downloads"
                  : "Remote download failed (HTTP " + String(code) + ")");
    return;
  }

  int remaining = -1;
  int remoteTotal = -1;
  int remoteRangeStart = -1;
  String contentType = "application/octet-stream";
  String headerLine;
  while (true) {
    if (!readRemoteLine(*transport, headerLine)) {
      Diagnostics::error("remote_epub.headers_timeout");
      releaseRemoteBookClient(transport, secureClient);
      server.send(502, "text/plain; charset=utf-8", "Remote response headers timeout");
      return;
    }
    if (headerLine.length() == 0) break;
    String lowerHeader = headerLine;
    lowerHeader.toLowerCase();
    if (lowerHeader.startsWith("content-length:")) {
      remaining = headerLine.substring(headerLine.indexOf(':') + 1).toInt();
    } else if (lowerHeader.startsWith("content-range:")) {
      String range = headerLine.substring(headerLine.indexOf(':') + 1);
      range.trim();
      int space = range.indexOf(' ');
      int dash = range.indexOf('-', space + 1);
      int slash = range.lastIndexOf('/');
      if (space >= 0 && dash > space && slash > dash) {
        remoteRangeStart = range.substring(space + 1, dash).toInt();
        remoteTotal = range.substring(slash + 1).toInt();
      }
    } else if (lowerHeader.startsWith("content-type:")) {
      contentType = headerLine.substring(headerLine.indexOf(':') + 1);
      contentType.trim();
    } else if (lowerHeader.startsWith("transfer-encoding:")
               && lowerHeader.indexOf("chunked") >= 0) {
      Diagnostics::error("remote_epub.chunked_unsupported");
      releaseRemoteBookClient(transport, secureClient);
      server.send(502, "text/plain; charset=utf-8", "Chunked remote downloads are unsupported");
      return;
    }
  }

  if (resumeOffset > 0) {
    if (remoteRangeStart != (int)resumeOffset || remoteTotal < 0 ||
        resumeOffset >= (uint32_t)remoteTotal || remaining < 0) {
      Diagnostics::error("remote_epub.bad_range");
      releaseRemoteBookClient(transport, secureClient);
      server.send(502, "text/plain; charset=utf-8", "Invalid remote resume response");
      return;
    }
  } else {
    remoteTotal = remaining;
  }

  const int expectedLength = remaining;
  Diagnostics::checkpoint("remote_epub.response");
  if (remaining > (int)kRemoteBookMaxBytes) {
    Diagnostics::warning("remote_epub.too_large");
    releaseRemoteBookClient(transport, secureClient);
    server.send(413, "text/plain; charset=utf-8", "Remote file is too large");
    return;
  }

  if (remaining >= 0) server.setContentLength((size_t)remaining);
  else server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  if (remoteTotal >= 0) {
    server.sendHeader("X-Remote-Total-Length", String(remoteTotal));
  }
  // A truncated response must be observable by fetch. Otherwise the browser
  // keeps waiting for the advertised Content-Length and never starts resume.
  server.sendHeader("Connection", "close");
  traceRemoteBookState("headers", 0, remaining, resumeOffset, remoteStartedMs);
  server.send(200, contentType, "");

  WiFiClient browser = server.client();
  // Never let a slow remote server block the loop task for the full HTTP
  // timeout. The proxy reads only bytes already available and yields between
  // attempts so the Wi-Fi stack and watchdog continue to be serviced.
  transport->setTimeout(1000);
  browser.setTimeout(1000);
  static uint8_t buffer[4096];
  uint32_t total = 0;
  uint32_t lastData = millis();
  uint32_t nextProgressTrace = 1024UL * 1024UL;

  // Do not use connected() as the loop condition: on ESP TLS clients it can
  // briefly report false while the socket is idle, even though more data may
  // still arrive. available() plus the stall deadline is the reliable test.
  while (remaining != 0) {
    size_t available = transport->available();
    if (available == 0) {
      if (millis() - lastData > kRemoteBookStallMs) {
        Diagnostics::errorCode("remote_epub.stall_timeout", (int)total);
        break;
      }
      delay(2);
      continue;
    }

    size_t wanted = available < sizeof(buffer) ? available : sizeof(buffer);
    if (remaining >= 0 && wanted > (size_t)remaining) wanted = (size_t)remaining;
    int n = transport->read(buffer, wanted);
    if (n <= 0) {
      Diagnostics::errorCode("remote_epub.remote_read_failed", (int)total);
      break;
    }
    size_t written = 0;
    RemoteBrowserWriteResult writeResult = writeRemoteBookChunk(
        browser, buffer, (size_t)n, total, remoteStartedMs, written);
    if (writeResult != RemoteBrowserWriteResult::complete) {
      if (writeResult == RemoteBrowserWriteResult::closed) {
        Diagnostics::errorCode("remote_epub.browser_closed", (int)total);
      } else {
        Diagnostics::errorCode("remote_epub.browser_write_timeout", (int)total);
      }
      break;
    }
    total += (uint32_t)n;
    if (remaining >= 0) remaining -= n;
    if (total > kRemoteBookMaxBytes) break;
    if (total >= nextProgressTrace) {
      traceRemoteBookState("progress", total, expectedLength,
                           resumeOffset, remoteStartedMs);
      nextProgressTrace += 1024UL * 1024UL;
    }
    lastData = millis();
    yield();
  }

  releaseRemoteBookClient(transport, secureClient);
  const bool complete = expectedLength < 0 || total == (uint32_t)expectedLength;
  if (!complete) {
    Diagnostics::errorCode("remote_epub.incomplete", (int)total);
    // Do not leave a Content-Length response open after an upstream failure:
    // the browser must observe EOF/error so its resume loop can issue a new
    // request instead of waiting forever for bytes that cannot arrive.
    browser.stop();
  }
  traceRemoteBookState("end", total, expectedLength, resumeOffset, remoteStartedMs);
  Diagnostics::checkpoint("remote_epub.end");
}

static void handleRoot() {
  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

// The filesystem check is necessary, but the catalog check is deliberate too:
// it prevents deleting a directory when the catalog still associates a book
// with it (or with one of its legacy nested paths).
static bool catalogContainsFolderBook(const String& folder) {
  String prefix = folder + "/";
  for (int i = 0; i < g_library.bookCount; i++) {
    String bookFolder = String(g_library.books[i].folder);
    if (bookFolder == folder || bookFolder.startsWith(prefix)) return true;
  }
  return false;
}

// EPUB conversion deliberately lives entirely in the browser.  The ESP32
// continues to receive only the compact UTF-8 text stream handled by /upload.
// Modern offline browsers provide raw-DEFLATE through DecompressionStream, so
// this avoids adding a ZIP/XML parser (and its ROM cost) to firmware.
static const char kEpubUploadScript[] = R"EPUBJS(
<script>
(()=>{
const form=document.getElementById('bookUpload'),input=document.getElementById('bookFile'),urlInput=document.getElementById('bookUrl'),urlButton=document.getElementById('bookUrlButton'),uploadStatus=document.getElementById('bookUploadStatus'),downloadStatus=document.getElementById('bookDownloadStatus'),downloadProgress=document.getElementById('bookDownloadProgress'),uploadProgress=document.getElementById('bookUploadProgress'),button=document.getElementById('bookUploadButton'),operationOverlay=document.getElementById('bookOperationOverlay'),operationTitle=document.getElementById('bookOperationTitle'),operationMessage=document.getElementById('bookOperationMessage'),operationStatus=document.getElementById('bookOperationStatus'),operationPercent=document.getElementById('bookOperationPercent'),operationCancel=document.getElementById('bookOperationCancel');
let converted=null,choice=0,keepaliveTimer=null,operationController=null,activeXhr=null,operationCancelled=false;
const setControlsDisabled=disabled=>{input.disabled=disabled;urlInput.disabled=disabled;urlButton.disabled=disabled;button.disabled=disabled;};
const beginOperation=(title,message)=>{operationCancelled=false;downloadProgress.hidden=true;uploadProgress.hidden=true;operationStatus.textContent=message;operationPercent.textContent='';operationTitle.textContent=title;operationMessage.textContent='';operationOverlay.hidden=false;setControlsDisabled(true);};
const endOperation=()=>{operationOverlay.hidden=true;operationController=null;activeXhr=null;setControlsDisabled(false);};
const clearSelectedFile=()=>{try{input.value='';}catch(_){} };
const formatBytes=n=>{if(n<1024)return n+' B';if(n<1024*1024)return (n/1024).toFixed(1)+' KB';return (n/(1024*1024)).toFixed(1)+' MB';};
const refreshOperationView=()=>{const progress=downloadProgress.hidden?uploadProgress:downloadProgress;const source=downloadProgress.hidden?uploadStatus.textContent:downloadStatus.textContent;if(!progress.hidden){if(source)operationStatus.textContent=source;if(progress.hasAttribute('value')&&progress.max>0)operationPercent.textContent=Math.round(progress.value*100/progress.max)+'%';else operationPercent.textContent='';}};
setInterval(refreshOperationView,100);
const getStorageInfo=async signal=>{let lastError=null;for(let attempt=0;attempt<3;attempt++){try{const response=await fetch('/storage-info',{cache:'no-store',signal});if(!response.ok)throw Error('storage check failed (HTTP '+response.status+')');return await response.json();}catch(error){lastError=error;if(attempt<2)await new Promise(resolve=>setTimeout(resolve,250));}}throw lastError||Error('storage check failed');};
const cancelOperation=()=>{operationCancelled=true;choice++;if(operationController)operationController.abort();if(activeXhr)activeXhr.abort();stopKeepalive();converted=null;clearSelectedFile();operationMessage.textContent='Operation cancelled.';endOperation();uploadStatus.textContent='Operation cancelled.';downloadStatus.textContent='Operation cancelled.';};
const startKeepalive=()=>{if(keepaliveTimer!==null)return;const ping=()=>{fetch('/keepalive',{cache:'no-store'}).catch(()=>{});};ping();keepaliveTimer=setInterval(ping,30000);};
const stopKeepalive=()=>{if(keepaliveTimer!==null){clearInterval(keepaliveTimer);keepaliveTimer=null;}};
const fail=(m,target=uploadStatus)=>{stopKeepalive();converted=null;clearSelectedFile();endOperation();target.textContent='EPUB conversion failed: '+m;button.disabled=true;};
const showSelected=file=>{try{const dt=new DataTransfer();dt.items.add(file);input.files=dt.files;}catch(_){/* visual status remains the fallback */}};
const u16=(a,o)=>a[o]|a[o+1]<<8, u32=(a,o)=>(u16(a,o)|u16(a,o+2)<<16)>>>0;
const name=(a,o,n)=>new TextDecoder('utf-8').decode(a.subarray(o,o+n));
const badXml=d=>d.getElementsByTagName('parsererror').length!==0;
const xml=(s,label)=>{const d=new DOMParser().parseFromString(s,'application/xml');if(badXml(d))throw Error('invalid '+label);return d;};
const path=(base,href)=>{try{const u=new URL(href,'https://epub.invalid/'+base);if(u.origin!=='https://epub.invalid')throw Error();return decodeURIComponent(u.pathname.replace(/^\//,''));}catch(_){throw Error('bad EPUB path');}};
async function zip(file){
 if(file.size>64*1024*1024)throw Error('file exceeds 64 MiB browser conversion limit');
 if(!('DecompressionStream'in window))throw Error('browser lacks offline EPUB decompression support');
 const a=new Uint8Array(await file.arrayBuffer());let end=-1;
 for(let i=a.length-22;i>=Math.max(0,a.length-65557);i--)if(u32(a,i)===0x06054b50){end=i;break;}
 if(end<0)throw Error('ZIP end record missing');
 if(u16(a,end+4)||u16(a,end+6))throw Error('multi-disk EPUB unsupported');
 const count=u16(a,end+10),start=u32(a,end+16),size=u32(a,end+12);if(start+size>a.length)throw Error('bad ZIP directory');
 const entries=new Map();let p=start;
 for(let i=0;i<count;i++){
  if(p+46>a.length||u32(a,p)!==0x02014b50)throw Error('bad ZIP entry');
  const flags=u16(a,p+8),method=u16(a,p+10),packed=u32(a,p+20),plain=u32(a,p+24),nl=u16(a,p+28),xl=u16(a,p+30),cl=u16(a,p+32),off=u32(a,p+42),n=name(a,p+46,nl);
  if(flags&1)throw Error('encrypted EPUB unsupported');if(off+30>a.length||u32(a,off)!==0x04034b50)throw Error('bad ZIP data');
  const data=off+30+u16(a,off+26)+u16(a,off+28);if(data+packed>a.length)throw Error('truncated ZIP entry');
  entries.set(n,{method,packed,plain,data});p+=46+nl+xl+cl;
 }
 return async n=>{const e=entries.get(n);if(!e)throw Error('missing '+n);if(e.plain>64*1024*1024)throw Error('EPUB entry exceeds 64 MiB limit');let out;
  if(e.method===0)out=a.slice(e.data,e.data+e.packed);else if(e.method===8)out=new Uint8Array(await new Response(new Blob([a.slice(e.data,e.data+e.packed)]).stream().pipeThrough(new DecompressionStream('deflate-raw'))).arrayBuffer());else throw Error('unsupported ZIP compression');
  if(out.length!==e.plain)throw Error('corrupt ZIP entry');return new TextDecoder('utf-8',{fatal:true}).decode(out);
 };
}
function chapter(source){
 let d=new DOMParser().parseFromString(source,'application/xhtml+xml');if(badXml(d))d=new DOMParser().parseFromString(source,'text/html');
 const root=d.body||d.documentElement,parts=[],block=new Set(['address','article','aside','blockquote','br','div','dl','dt','dd','figcaption','figure','footer','h1','h2','h3','h4','h5','h6','header','hr','li','main','nav','ol','p','pre','section','table','td','th','tr','ul']);
 const walk=n=>{if(n.nodeType===3){parts.push(n.nodeValue);return;}if(n.nodeType!==1)return;const tag=n.localName.toLowerCase();if(['head','script','style','noscript','template','svg','math','audio','video','canvas','iframe','object'].includes(tag))return;if(block.has(tag))parts.push('\n');for(const c of n.childNodes)walk(c);if(block.has(tag))parts.push('\n');};
 walk(root);return parts.join('').replace(/\u00a0/g,' ').replace(/[ \t\f\v]+/g,' ').replace(/ *\n */g,'\n').replace(/\n{3,}/g,'\n\n').trim();
}
async function convert(file){
 const get=await zip(file),container=xml(await get('META-INF/container.xml'),'container.xml'),root=container.getElementsByTagNameNS('*','rootfile')[0];if(!root)throw Error('package document missing');
 const opfPath=root.getAttribute('full-path');if(!opfPath)throw Error('package path missing');const opf=xml(await get(opfPath),'package document'),base=opfPath.replace(/[^/]*$/,'');
 const manifest=new Map();for(const item of opf.getElementsByTagNameNS('*','item'))manifest.set(item.getAttribute('id'),item);
 const sections=[];for(const ref of opf.getElementsByTagNameNS('*','itemref')){const item=manifest.get(ref.getAttribute('idref'));if(!item)throw Error('spine item missing from manifest');const href=item.getAttribute('href');if(!href)throw Error('spine item path missing');const text=chapter(await get(path(base,href)));if(text)sections.push(text);}
 if(!sections.length)throw Error('no readable XHTML/HTML spine content');const text=sections.join('\n\n').replace(/\n{3,}/g,'\n\n');if(!text)throw Error('empty EPUB');
 const filename=(file.name.replace(/\.epub$/i,'')||'book')+'.txt';return new File([text],filename,{type:'text/plain;charset=utf-8'});
}
const prepare=async(file,selected,fromOnline)=>{startKeepalive();if(fromOnline)downloadStatus.textContent='Converting EPUB…';else uploadStatus.textContent='Converting EPUB…';uploadProgress.hidden=false;uploadProgress.removeAttribute('value');try{const result=await convert(file);if(selected!==choice)return;converted=result;showSelected(converted);uploadProgress.hidden=true;uploadStatus.textContent='Selected file: '+converted.name+' ('+converted.size.toLocaleString()+' bytes)';if(fromOnline)downloadStatus.textContent='Download complete. EPUB converted and ready to upload.';stopKeepalive();endOperation();}catch(e){uploadProgress.hidden=true;if(selected===choice&&!operationCancelled)fail(e&&e.message?e.message:'invalid EPUB',fromOnline?downloadStatus:uploadStatus);}};
input.addEventListener('change',async()=>{const file=input.files[0],selected=++choice;converted=null;uploadProgress.hidden=true;if(!file){stopKeepalive();uploadStatus.textContent='';return;}if(!/\.epub$/i.test(file.name)){stopKeepalive();uploadStatus.textContent='Selected file: '+file.name;return;}beginOperation('Converting EPUB…','The interface is locked while the EPUB is converted.');await prepare(file,selected,false);});
 const readDownload=async(response,offset,chunks,totalHint)=>{const advertised=Number(response.headers.get('X-Remote-Total-Length'))||totalHint||Number(response.headers.get('Content-Length'))||0,reader=response.body&&response.body.getReader();let loaded=offset;if(!reader){try{const blob=await response.blob();if(blob.size)chunks.push(blob);loaded+=blob.size;return {loaded,total:advertised};}catch(e){e.loaded=loaded;e.total=advertised;throw e;}}downloadProgress.hidden=false;downloadProgress.max=advertised||1;downloadProgress.value=offset;try{for(;;){const part=await reader.read();if(part.done)break;chunks.push(part.value);loaded+=part.value.byteLength;if(advertised){downloadProgress.value=loaded;downloadStatus.textContent='Downloading EPUB… '+Math.round(loaded*100/advertised)+'%';}else downloadStatus.textContent='Downloading EPUB… '+loaded.toLocaleString()+' bytes';}}catch(e){e.loaded=loaded;e.total=advertised;throw e;}return {loaded,total:advertised};};
 const fetchRemoteWithResume=async raw=>{let chunks=[],offset=0,total=0,lastError=null;for(let attempt=1;attempt<=3;attempt++){try{downloadStatus.textContent=offset===0?(attempt===1?'Connecting to remote download…':'Retrying remote download… ('+attempt+'/3)'):'Resuming remote download… '+Math.round(offset*100/(total||offset+1))+'%';const response=await fetch('/download-url',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'url='+encodeURIComponent(raw)+'&offset='+offset,signal:operationController&&operationController.signal});if(!response.ok){const message=await response.text(),error=Error(message||('download failed (HTTP '+response.status+')'));error.httpStatus=response.status;throw error;}const state=await readDownload(response,offset,chunks,total);total=state.total||total;if(!total||state.loaded===total)return new Blob(chunks,{type:response.headers.get('Content-Type')||'application/epub+zip'});if(state.loaded>offset){offset=state.loaded;continue;}throw Error('remote download made no progress');}catch(e){lastError=e;if(operationCancelled)throw Error('operation cancelled');if(e&&e.loaded>offset){offset=e.loaded;total=e.total||total;}if(e&&e.httpStatus===409&&offset>0){chunks=[];offset=0;total=0;continue;}if(attempt<3){downloadStatus.textContent=offset>0?'Download interrupted. Resuming…':'Download interrupted. Retrying…';await new Promise(resolve=>setTimeout(resolve,1000*Math.min(attempt,3)));}}}throw lastError||Error('download failed');};
 // Do not start keepalive here: /download-url is currently synchronous and
 // those extra requests queue behind the stream, consuming sockets/RAM.
 urlButton.addEventListener('click',async()=>{const raw=urlInput.value.trim(),selected=++choice;converted=null;clearSelectedFile();downloadProgress.hidden=true;if(!/^https?:\/\//i.test(raw)){fail('enter a valid HTTP(S) EPUB URL',downloadStatus);return;}beginOperation('Downloading EPUB…','The interface is locked while the remote book is downloaded.');operationController=new AbortController();try{const blob=await fetchRemoteWithResume(raw);const url=new URL(raw),part=decodeURIComponent(url.pathname.split('/').pop()||'book.epub'),filename=/\.epub$/i.test(part)?part:part+'.epub';await prepare(new File([blob],filename,{type:'application/epub+zip'}),selected,true);}catch(e){if(!operationCancelled)fail(e&&e.message?e.message:'download failed',downloadStatus);}finally{if(!operationCancelled)endOperation();}});
 form.addEventListener('submit',async e=>{if(!converted)return;e.preventDefault();const file=converted;beginOperation('Checking device storage…','Checking whether the book fits on the device.');operationController=new AbortController();try{const storage=await getStorageInfo(operationController.signal);const free=Number(storage.freeBytes)||0;const required=file.size+8192;if(free<required){endOperation();uploadStatus.textContent='Upload not started: book '+formatBytes(file.size)+', device has only '+formatBytes(free)+' free.';return;}}catch(error){if(operationCancelled)return;endOperation();uploadStatus.textContent='Upload not started: cannot check device storage after 3 attempts.';return;}operationController=null;beginOperation('Uploading book…','The interface is locked while the converted book is uploaded.');startKeepalive();uploadProgress.hidden=false;uploadProgress.max=file.size;uploadProgress.value=0;uploadStatus.textContent='Uploading '+file.name+'… 0%';const fd=new FormData();fd.append('file',file,file.name);const xhr=new XMLHttpRequest();activeXhr=xhr;xhr.open('POST','/upload');xhr.upload.onprogress=ev=>{if(ev.lengthComputable){uploadProgress.value=ev.loaded;uploadStatus.textContent='Uploading '+file.name+'… '+Math.round(ev.loaded*100/ev.total)+'%';}};xhr.onload=()=>{stopKeepalive();if(xhr.status<200||xhr.status>=300){converted=null;clearSelectedFile();endOperation();const detail=(xhr.responseText||'').trim();uploadStatus.textContent=detail?'Upload failed: '+detail:'Upload failed: HTTP '+xhr.status;return;}document.open();document.write(xhr.responseText);document.close();};xhr.onerror=()=>{stopKeepalive();converted=null;clearSelectedFile();endOperation();uploadStatus.textContent='Upload failed: network error';};xhr.onabort=()=>{stopKeepalive();if(!operationCancelled){converted=null;clearSelectedFile();endOperation();uploadStatus.textContent='Upload cancelled';}};xhr.send(fd);});
 operationCancel.addEventListener('click',cancelOperation);
})();
</script>
)EPUBJS";

static void handleFiles() {
  String out = webPageStart(
    D_WEB_BOOKS_HEADING,
    D_WEB_BOOKS_SUBTITLE,
    webNav("books")
  );

  out += storageCardHtml();

  out += "<div class='card'><p class='muted'>" D_WEB_BOOKS_PATHS_DESC "</p></div>";

  out += "<div class='card'><h2>" D_WEB_ONLINE_BOOKS_HEADING "</h2>"
         "<p class='muted'>" D_WEB_ONLINE_BOOKS_DESC "</p>";
  String onlineLibraryUrl = ONLINE_BOOK_LIBRARY_URL;
  if (onlineLibraryUrl.length() > 0) {
    out += "<div class='actions'><a class='btn secondary' href='" + htmlEscape(onlineLibraryUrl) + "' target='_blank' rel='noopener'>" D_WEB_OPEN_ONLINE_LIBRARY "</a></div>";
  } else {
    out += "<p class='muted'>" D_WEB_ONLINE_LIBRARY_MISSING "</p>";
  }
  out += "<label for='bookUrl'>" D_WEB_EPUB_URL_LABEL "</label>"
         "<input id='bookUrl' type='url' inputmode='url' size='80' style='width:100%' placeholder='" D_WEB_EPUB_URL_PLACEHOLDER "' autocomplete='url' spellcheck='false'>"
         "<p id='bookDownloadStatus' class='muted' aria-live='polite' hidden></p>"
         "<div class='actions'><button id='bookUrlButton' type='button'";
  if (WiFi.status() != WL_CONNECTED) {
    out += " disabled";
  }
  out += ">" D_WEB_IMPORT_EPUB_BUTTON "</button></div>";
  if (WiFi.status() != WL_CONNECTED) {
    out += "<p class='muted'>" D_WEB_ONLINE_BOOKS_WIFI_REQUIRED "</p>";
  }
  out += "</div>";

  out +=
    "<div class='card'><h2>" D_WEB_UPLOAD_BOOK_HEADING "</h2>"
    "<p class='muted'>" D_WEB_UPLOAD_BOOK_DESC "</p>"
    "<form id='bookUpload' method='POST' action='/upload' enctype='multipart/form-data' accept-charset='UTF-8' style='margin-top:14px'>"
    "<input id='bookFile' type='file' name='file' accept='.txt,text/plain,.epub,application/epub+zip'>"
    "<p id='bookUploadStatus' class='muted' aria-live='polite' hidden></p>"
    "<div class='actions'><button id='bookUploadButton' type='submit'>" D_WEB_UPLOAD_BOOK_BUTTON "</button></div>"
    "</form></div>";

  out += "<div id='bookOperationOverlay' class='operation-overlay' hidden aria-live='polite'>"
         "<div class='operation-dialog'><h2 id='bookOperationTitle'>Working…</h2>"
         "<p id='bookOperationMessage' hidden></p>"
         "<div class='operation-progress-line'>"
         "<progress id='bookDownloadProgress' max='1' value='0' hidden></progress>"
         "<progress id='bookUploadProgress' max='1' value='0' hidden></progress>"
         "<strong id='bookOperationPercent'></strong>"
         "</div>"
         "<p id='bookOperationStatus' class='operation-status' aria-live='polite'></p>"
         "<button id='bookOperationCancel' type='button' class='btn danger'>Cancel</button></div></div>";

  out += kEpubUploadScript;

  // This is an index, not a second action list: each book entry links to the
  // corresponding card below. It gives the library a stable mental model:
  // Library -> folder -> book, while keeping all actions in the book card.
  out += "<div class='card'><h2>" D_WEB_FOLDERS_HEADING "</h2>"
         "<div class='tree-create'><h3>" D_WEB_CREATE_FOLDER_HEADING "</h3>"
         "<form method='POST' action='/mkdir' class='stack' accept-charset='UTF-8' style='margin-top:12px'>"
         "<input id='folderName' type='text' name='folder' placeholder='" D_WEB_CREATE_FOLDER_PLACEHOLDER "' maxlength='24' oninput=\"document.getElementById('folderNameCount').textContent=this.value.length\">"
         "<div class='hint'><span id='folderNameCount'>0</span>/24" D_WEB_FOLDER_NAME_COUNTER ".</div>"
         "<div class='actions'><button type='submit'>" D_WEB_CREATE_FOLDER_BUTTON "</button><span class='muted'>" D_WEB_CREATE_FOLDER_HINT "</span></div>"
         "</form></div>"
         "<div class='library-tree'>"
         "<div class='tree-root'><div class='tree-root-header'><div class='tree-label'><span class='tree-mark'>&#9670;</span><span>Library</span></div></div><div class='tree-children'>";
  bool rootHasBooks = false;
  for (int i = 0; i < g_library.bookCount; i++) {
    if (g_library.books[i].folder[0] != '\0') continue;
    rootHasBooks = true;
    out += "<div class='tree-book'><span class='tree-mark'>&#8627;</span><a href='#book-card-" + String(i) + "'>";
    out += htmlEscape(String(g_library.books[i].name));
    out += "</a><span class='muted'>" + String((int)g_library.books[i].size) + D_WEB_BOOK_BYTES_LABEL "</span></div>";
  }
  if (!rootHasBooks) out += "<div class='tree-empty'>No books in the library root.</div>";
  out += "</div></div>";

  for (int folderIndex = 0; folderIndex < g_library.folderCount; folderIndex++) {
    String folder = String(g_library.folders[folderIndex]);
    if (folder.indexOf('/') >= 0) continue;  // legacy nested paths are not recreated
    out += "<div class='tree-folder'><div class='tree-folder-header'><div class='tree-label'><span class='tree-mark'>&#128193;</span><span>";
    out += htmlEscape(folderLeafLabel(folder));
    out += "</span></div><form method='POST' action='/rmdir' style='margin:0'><input type='hidden' name='folder' value='";
    out += htmlEscape(folder);
    out += "'><button type='submit' class='btn danger' onclick=\"return confirm('" D_WEB_CONFIRM_DELETE_FOLDER "')\">" D_WEB_DELETE_BUTTON "</button></form></div><div class='tree-children'>";
    bool folderHasBooks = false;
    String prefix = folder + "/";
    for (int i = 0; i < g_library.bookCount; i++) {
      String bookFolder = String(g_library.books[i].folder);
      if (!(bookFolder == folder || bookFolder.startsWith(prefix))) continue;
      folderHasBooks = true;
      out += "<div class='tree-book'><span class='tree-mark'>&#8627;</span><a href='#book-card-" + String(i) + "'>";
      out += htmlEscape(String(g_library.books[i].name));
      out += "</a><span class='muted'>" + String((int)g_library.books[i].size) + D_WEB_BOOK_BYTES_LABEL "</span></div>";
    }
    if (!folderHasBooks) out += "<div class='tree-empty'>Empty folder</div>";
    out += "</div></div>";
  }
  if (g_library.folderCount == 0 && !rootHasBooks) {
    out += "<p class='muted'>" D_WEB_NO_FOLDERS "</p>";
  }
  out += "</div></div>";

  out += "<div class='card'><h2>" D_WEB_LIBRARY_BOOKS_HEADING "</h2>";
  if (g_library.bookCount   >= MAX_BOOKS)   out += "<p style='color:#b91c1c;font-weight:600'>" D_WEB_LIBRARY_FULL_WARN "</p>";
  if (g_library.folderCount >= MAX_FOLDERS) out += "<p style='color:#b91c1c;font-weight:600'>" D_WEB_FOLDER_LIMIT_WARN "</p>";

  if (g_library.bookCount == 0) {
    out += "<p class='muted'>" D_WEB_NO_BOOKS_UPLOADED "</p>";
  } else {
    out += "<ul class='list book-list'>";
    for (int i = 0; i < g_library.bookCount; i++) {
      String filePath = String(g_library.books[i].path);
      String folderLabel = g_library.books[i].folder[0] ? prettyRelativeLabel(g_library.books[i].folder) : String(D_WEB_BOOK_ROOT);
      int savedPage = savedPageForBookPath(filePath) + 1;
      if (savedPage < 1) savedPage = 1;

      String bookName = String(g_library.books[i].name);
      String currentFolder = String(g_library.books[i].folder);
      const size_t titlePrefixBytes = currentFolder.length() == 0
                                   ? sizeof("/books/") - 1
                                   : sizeof("/books/") - 1 + currentFolder.length() + 1;
      const size_t titleLeafBytes = (sizeof(g_library.books[i].path) - 1 > titlePrefixBytes)
                                  ? sizeof(g_library.books[i].path) - 1 - titlePrefixBytes : 0;
      const size_t titlePathLimit = titleLeafBytes > 4 ? titleLeafBytes - 4 : 1;
      const size_t titleLimit = titlePathLimit < (size_t)MAX_BOOK_TITLE_BYTES
                              ? titlePathLimit : (size_t)MAX_BOOK_TITLE_BYTES;
      out += "<li id='book-card-" + String(i) + "' class='book-item'><div class='book-title-line'><div class='book-main'>";
      out += "<div id='bookTitleView" + String(i) + "' class='book-title-view'><h3 class='book-title'>";
      out += htmlEscape(bookName);
      out += "</h3><button type='button' class='icon-btn' aria-label='Rename book' title='Rename book' onclick=\"editBookTitle(" + String(i) + ")\">&#9998;</button></div>";
      out += "<form id='bookTitleEdit" + String(i) + "' method='POST' action='/rename' class='stack' accept-charset='UTF-8' hidden>";
      out += "<input type='hidden' name='id' value='" + String(i) + "'>";
      out += "<input id='bookName" + String(i) + "' type='text' name='name' value='" + htmlEscape(bookName) + "' maxlength='" + String(titleLimit) + "' required autocomplete='off' autocapitalize='off' spellcheck='false' oninput=\"document.getElementById('bookNameCount" + String(i) + "').textContent=this.value.length\">";
      out += "<div class='hint'><span id='bookNameCount" + String(i) + "'>" + String(bookName.length()) + "</span>/" + String(titleLimit) + D_WEB_BOOK_NAME_COUNTER ".<br>" D_WEB_BOOK_NAME_LIMIT_HINT "</div>";
      out += "<div class='actions small'><button type='submit'>Save</button><button type='button' class='btn secondary' onclick=\"cancelBookTitle(" + String(i) + ")\">Cancel</button></div></form></div>";
      out += "</div><div class='meta'>";
      out += String((int)g_library.books[i].size);
      out += D_WEB_BOOK_BYTES_LABEL " (";
      out += String((float)g_library.books[i].size / 1000.0f, 1);
      out += D_WEB_BOOK_KB_LABEL ")" D_WEB_BOOK_FOLDER_LABEL;
      out += htmlEscape(folderLabel);
      out += D_WEB_BOOK_CURRENT_PAGE;
      out += String(savedPage);
      out += "</div>";

      out += "<div class='book-actions'><div class='book-actions-left'>";
      out += "<a class='btn' href='/read?id=" + String(i) + "'>" D_WEB_READ_AND_FIND_LINK "</a>";
      out += "<form method='POST' action='/move' class='book-folder-form' accept-charset='UTF-8'>";
      out += "<input type='hidden' name='id' value='" + String(i) + "'><select name='folder' aria-label='Move book to folder'>";
      out += "<option value=''" + (currentFolder.length() == 0 ? String(" selected") : String("")) + ">Library</option>";
      for (int folderIndex = 0; folderIndex < g_library.folderCount; folderIndex++) {
        String folder = String(g_library.folders[folderIndex]);
        if (folder.indexOf('/') >= 0) continue;
        out += "<option value='" + htmlEscape(folder) + "'" + (currentFolder == folder ? String(" selected") : String("")) + ">";
        out += htmlEscape(folderLeafLabel(folder));
        out += "</option>";
      }
      out += "</select><button type='submit' class='btn secondary'>Move</button></form></div>";
      out += "<div class='book-actions-right'><a class='btn secondary' href='/download-book?id=" + String(i) + "' download>" D_WEB_DOWNLOAD_BUTTON "</a>";
      out += "<form method='POST' action='/del' style='margin:0'><input type='hidden' name='id' value='" + String(i) + "'>";
      out += "<button type='submit' class='btn danger' onclick=\"return confirm('" D_WEB_CONFIRM_DELETE_FILE "')\">" D_WEB_DELETE_BUTTON "</button></form></div></div></li>";
    }
    out += "</ul>";
  }

  out += "</div>";
  out += "<script>function editBookTitle(i){var v=document.getElementById('bookTitleView'+i),f=document.getElementById('bookTitleEdit'+i),n=document.getElementById('bookName'+i);if(v&&f){v.hidden=true;f.hidden=false;if(n){n.focus();n.select();}}}function cancelBookTitle(i){var v=document.getElementById('bookTitleView'+i),f=document.getElementById('bookTitleEdit'+i);if(v&&f){f.hidden=true;v.hidden=false;}}</script>";

  out += webPageEnd();
  server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate");
  server.send(200, "text/html; charset=utf-8", out);
}

static void handleDelete() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  // Library entry already cleared g_bookview; no book is "current" here.
  String path = String(g_library.books[id].path);
  if (FS.exists(path)) FS.remove(path);
  deleteBookMetadata(path);
  loadBooks();   // refresh catalog so the next read sees the deletion

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleCreateFolder() {
  ensureBooksDir();
  if (!server.hasArg("folder")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_FOLDER);
    return;
  }

  String folder = sanitizeFolderInput(server.arg("folder"));
  if (folder.length() == 0) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }
  if (folder.indexOf('/') >= 0) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }
  if (folder.length() > MAX_FOLDER_NAME_BYTES) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }
  if (g_library.folderCount >= MAX_FOLDERS) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_LIMIT);
    return;
  }

  String fullPath = "/books/" + folder;
  if (!ensureDirRecursive(fullPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_MKDIR_FAILED);
    return;
  }
  loadBooks();   // refresh catalog so the new folder appears

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleDeleteFolder() {
  if (!server.hasArg("folder")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_FOLDER);
    return;
  }

  String folder = sanitizeFolderInput(server.arg("folder"));
  if (folder.length() == 0) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }
  if (folder.indexOf('/') >= 0) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }

  String fullPath = "/books/" + folder;
  if (!FS.exists(fullPath)) {
    server.send(404, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_NOT_FOUND);
    return;
  }
  if (catalogContainsFolderBook(folder) || !isDirEmpty(fullPath)) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_NOT_EMPTY);
    return;
  }
  if (!FS.rmdir(fullPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_DELETE_FAILED);
    return;
  }
  loadBooks();   // refresh catalog so the folder disappears

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleMoveBook() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  String oldPath = String(g_library.books[id].path);
  String folder = sanitizeFolderInput(server.arg("folder"));
  if (folder.indexOf('/') >= 0) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_FOLDER);
    return;
  }
  String destDir = (folder.length() == 0) ? String("/books") : String("/books/") + folder;
  String newPath = destDir + "/" + lastPathComponent(oldPath);
  if (newPath.length() >= sizeof(g_library.books[id].path)) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_NAME);
    return;
  }

  if (!ensureDirRecursive(destDir)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_FOLDER_CREATE_FAILED);
    return;
  }

  if (newPath == oldPath) {
    server.sendHeader("Location", "/books");
    server.send(302, "text/plain", "");
    return;
  }
  if (FS.exists(newPath)) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_DEST_EXISTS);
    return;
  }

  // Library entry already cleared g_bookview; no book is "current" here.
  if (!FS.rename(oldPath, newPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_MOVE_FAILED);
    return;
  }

  // Do not report success unless the filesystem reflects the requested move.
  // This also protects the folder-delete guard from a stale catalog entry.
  if (FS.exists(oldPath) || !FS.exists(newPath)) {
    if (FS.exists(newPath) && !FS.exists(oldPath)) FS.rename(newPath, oldPath);
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_MOVE_FAILED);
    return;
  }

  migrateBookMetadata(oldPath, newPath);   // NVS keys + page-cache file
  loadBooks();   // refresh catalog so the moved book shows in its new folder

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleRenameBook() {
  if (!server.hasArg("id")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID);
    return;
  }
  if (!server.hasArg("name")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_NAME);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  String oldPath = String(g_library.books[id].path);
  String folder = String(g_library.books[id].folder);
  const size_t prefixBytes = (folder.length() == 0 ? sizeof("/books/") - 1
                                                    : sizeof("/books/") - 1 + folder.length() + 1);
  const size_t maxLeafBytes = (sizeof(g_library.books[id].path) - 1 > prefixBytes)
                              ? sizeof(g_library.books[id].path) - 1 - prefixBytes : 0;
  const size_t pathTitleBytes = maxLeafBytes > 4 ? maxLeafBytes - 4 : 0;
  const size_t maxTitleBytes = pathTitleBytes < (size_t)MAX_BOOK_TITLE_BYTES
                             ? pathTitleBytes : (size_t)MAX_BOOK_TITLE_BYTES;
  String requestedName = server.arg("name");
  if (requestedName.length() == 0 || requestedName.length() > maxTitleBytes) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_NAME);
    return;
  }
  String leaf = sanitizeUploadedFilename(requestedName, maxLeafBytes);
  String newPath = (folder.length() == 0 ? String("/books/") : String("/books/") + folder + "/") + leaf;
  if (newPath.length() >= sizeof(g_library.books[id].path)) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_NAME);
    return;
  }

  if (newPath == oldPath) {
    server.sendHeader("Location", "/books");
    server.send(302, "text/plain", "");
    return;
  }
  if (FS.exists(newPath)) {
    server.send(409, "text/plain; charset=utf-8", D_WEB_ERR_DEST_EXISTS);
    return;
  }
  if (!FS.rename(oldPath, newPath)) {
    server.send(500, "text/plain; charset=utf-8", D_WEB_ERR_RENAME_FAILED);
    return;
  }

  migrateBookMetadata(oldPath, newPath);
  loadBooks();
  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

static void handleJumpPageWeb() {
  if (!server.hasArg("id") || !server.hasArg("page")) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_MISSING_ID_PAGE);
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= g_library.bookCount) {
    server.send(400, "text/plain; charset=utf-8", D_WEB_ERR_BAD_ID);
    return;
  }

  int page = server.arg("page").toInt();
  if (page < 1) page = 1;
  int zeroBasedPage = page - 1;

  // Library entry already cleared g_bookview. We write both the page number
  // (display hint) and the byte offset (canonical position used by the
  // reader on next open). Computing the offset requires paginating the
  // book at the current font — file open + page walk. Same cost as the
  // page-offset lookup path.
  String path = String(g_library.books[id].path);
  String key = prefKeyForBook(path);
  PreferencesStore kv(prefs);
  saveSavedPage(kv, key, zeroBasedPage);

  File f = FS.open(path, "r");
  if (f) {
    uint32_t offset = pageOffsetForPage(f, path, zeroBasedPage);
    f.close();
    saveSavedOffset(kv, key, offset);
  }

  server.sendHeader("Location", "/books");
  server.send(302, "text/plain", "");
}

void registerFilesRoutes() {
  server.on("/",             HTTP_GET,  handleRoot);
  server.on("/books",        HTTP_GET,  handleFiles);
  server.on("/download-book", HTTP_GET, handleDownloadBook);
  server.on("/download-url", HTTP_POST, handleRemoteBookDownload);
  server.on("/del",      HTTP_POST, handleDelete);          // POST: prevents accidental deletion via browser prefetch
  server.on("/mkdir",    HTTP_POST, handleCreateFolder);
  server.on("/rmdir",    HTTP_POST, handleDeleteFolder);    // POST: destructive
  server.on("/move",     HTTP_POST, handleMoveBook);
  server.on("/rename",   HTTP_POST, handleRenameBook);
  server.on("/jumppage", HTTP_POST, handleJumpPageWeb);
}
