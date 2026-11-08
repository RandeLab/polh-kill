// Embedded pages: captive-portal setup and OTA upload. Kept small and
// self-contained (no external fonts/scripts: the hotspot has no internet).
#pragma once
#include <Arduino.h>

static const char PAGE_STYLE[] PROGMEM = R"css(
:root{--bg:#0B0014;--panel:#16032A;--text:#FFEAF4;--muted:#E3A0C0;--pink:#FF6BB5;--cyan:#00F5FF;--green:#39FF14;--input:#22064A}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:16px/1.45 -apple-system,"Segoe UI",Roboto,sans-serif;
background-image:radial-gradient(circle at 12% -5%,rgba(255,107,181,.22),transparent 45%),radial-gradient(circle at 88% 105%,rgba(0,245,255,.16),transparent 45%);min-height:100vh}
main{max-width:520px;margin:0 auto;padding:20px 16px 40px}h1{font-size:1.6rem;margin:.2em 0;color:var(--pink);text-shadow:0 0 12px rgba(255,107,181,.7)}
.sub{color:var(--muted);margin:0 0 18px}.panel{background:var(--panel);border:2px solid var(--pink);border-radius:6px;padding:16px;margin:14px 0;box-shadow:0 0 15px rgba(255,107,181,.45)}
label{display:block;margin:12px 0 4px;color:var(--muted);font-size:.9rem}input,select{width:100%;padding:10px;border-radius:6px;border:1px solid var(--pink);background:var(--input);color:var(--text);font-size:1rem}
button{margin-top:16px;width:100%;padding:12px;border:2px solid var(--cyan);background:transparent;color:var(--cyan);border-radius:6px;font-size:1rem;font-weight:700;cursor:pointer}
button:hover{background:rgba(0,245,255,.12)}small{color:var(--muted)}a{color:var(--cyan)}.ok{color:var(--green)}
.nets{display:flex;flex-wrap:wrap;gap:6px;margin-top:6px}.nets span{border:1px solid var(--muted);border-radius:12px;padding:3px 10px;font-size:.85rem;cursor:pointer}
progress{width:100%;height:18px;margin-top:12px;accent-color:var(--pink)}
)css";

static const char PAGE_SETUP[] PROGMEM = R"html(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>polh-kill setup</title><style>%STYLE%</style></head>
<body><main>
<h1>🐭 polh-kill setup</h1>
<p class="sub">Ze trap needs to know your WiFi. Give it, and it will go away from this hotspot, and so on, and so on.</p>
<form method="post" action="/api/setup" class="panel">
<label for="ssid">WiFi network</label><input id="ssid" name="ssid" value="%SSID%" required autocomplete="off">
<div class="nets" id="nets"><small>scanning…</small></div>
<label for="pass">WiFi password</label><input id="pass" name="pass" type="password" placeholder="%PASS_HINT%">
<label for="token">Telegram bot token (from @BotFather)</label><input id="token" name="token" placeholder="%TOKEN_HINT%" autocomplete="off">
<label for="chat">Telegram chat id (optional)</label><input id="chat" name="chat" value="%CHAT%" placeholder="empty = first /start pairs">
<label for="name">Device name</label><input id="name" name="name" value="%NAME%">
<label for="admin">Admin password (optional, protects OTA + reset)</label><input id="admin" name="admin" type="password" placeholder="%ADMIN_HINT%">
<label for="ghtoken">GitHub token (optional, publishes captures to GitHub Pages)</label><input id="ghtoken" name="gh_token" type="password" placeholder="%GH_HINT%" autocomplete="off">
<label for="ghrepo">GitHub repo</label><input id="ghrepo" name="gh_repo" value="%GH_REPO%">
<button type="submit">💾 Save and reboot</button>
<small>Empty password / token fields keep ze saved values.</small>
</form>
<p class="sub">Full interface: <a href="/">learning lab &amp; live view</a> · <a href="/update">⚡ OTA update</a></p>
</main><script>
function scan(){fetch('/api/wifi/scan').then(r=>r.json()).then(l=>{const n=document.getElementById('nets');
if(!l.length){setTimeout(scan,1500);return}n.innerHTML='';l.forEach(w=>{const s=document.createElement('span');
s.textContent=(w.open?'🔓 ':'')+w.ssid+' '+w.rssi;s.onclick=()=>{document.getElementById('ssid').value=w.ssid;document.getElementById('pass').focus()};n.appendChild(s)})}).catch(()=>setTimeout(scan,2000))}
fetch('/api/wifi/scan?start=1').then(()=>setTimeout(scan,2500));
</script></body></html>)html";

static const char PAGE_SAVED[] PROGMEM = R"html(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>polh-kill saved</title><style>%STYLE%</style></head>
<body><main><h1>✅ Saved</h1><div class="panel"><p>Ze trap reboots now and joins <b>%SSID%</b>.</p>
<p>Then find it at <a href="http://polh-kill.local">http://polh-kill.local</a> or look at your router for its IP.
If ze WiFi does not work, the hotspot <b>polh-kill-AP</b> comes back after ~20 s.</p></div></main></body></html>)html";

static const char PAGE_UPDATE[] PROGMEM = R"html(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>polh-kill OTA</title><style>%STYLE%</style></head>
<body><main><h1>⚡ polh-kill OTA update</h1><p class="sub">Firmware %VERSION%. Feed me a new .bin, I will digest it.</p>
<form id="f" class="panel">
<label for="type">What</label><select id="type"><option value="fw">Firmware (firmware.bin)</option><option value="fs">Web UI (littlefs.bin)</option></select>
<label for="file">File</label><input id="file" type="file" accept=".bin" required>
<button type="submit">⚡ Upload</button><progress id="p" max="100" value="0"></progress><p id="msg"></p>
<small>Ze learned dataset lives in its own partition and survives both kinds of update.</small>
</form><p class="sub"><a href="/">← back</a></p></main>
<script>
document.getElementById('f').onsubmit=e=>{e.preventDefault();const file=document.getElementById('file').files[0];if(!file)return;
const t=document.getElementById('type').value,x=new XMLHttpRequest(),fd=new FormData(),m=document.getElementById('msg');fd.append('update',file);
x.upload.onprogress=ev=>{if(ev.lengthComputable)document.getElementById('p').value=Math.round(ev.loaded/ev.total*100)};
x.onload=()=>{m.innerHTML=x.status==200?'<span class="ok">✅ Done. Rebooting, reload in 10 s.</span>':'❌ '+x.responseText;
if(x.status==200)setTimeout(()=>location.href='/',10000)};x.onerror=()=>m.textContent='❌ upload failed';
x.open('POST','/update?type='+t);x.send(fd);m.textContent='uploading…'};
</script></body></html>)html";

static const char PAGE_NO_UI[] PROGMEM = R"html(<!doctype html><html lang="en"><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>polh-kill</title><style>%STYLE%</style></head>
<body><main><h1>🐭 polh-kill</h1><div class="panel"><p>Ze firmware runs, but ze web UI is not on ze flash yet.</p>
<p>Run <code>pio run -t uploadfs</code> or upload <code>littlefs.bin</code> on the <a href="/update">⚡ OTA page</a>.</p>
<p><a href="/setup">WiFi / Telegram setup</a> · <a href="/api/status">/api/status</a></p></div></main></body></html>)html";
