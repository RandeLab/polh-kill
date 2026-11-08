/* polh-kill web UI — vanilla JS, no CDN (must work on the offline hotspot) */
'use strict';

const BUILTIN = ['snap', 'bump', 'footstep', 'mouse', 'other', 'armed-trap'];
let CLASSES = BUILTIN.slice();  // replaced by /api/classes (built-in + custom)
let EMOJI = { snap: '💥', bump: '🤜', footstep: '👣', mouse: '🐭', other: '❓', 'armed-trap': '🪤' };
let classInfo = [];
// caught in the bar (soft vs. hard target damps the ring-down differently),
// then animals around the trap, then household noise
const EMOJI_PICKS = [
  // animals
  '🐁', '🐀', '🐹', '🐿️', '🦔', '🐈', '🐕', '🐦', '🐓', '🦉', '🐍', '🦎', '🐸', '🪳', '🕷️', '🦟', '🪰', '🐜', '🐌', '🦗', '🐾',
  // bait and food
  '🧀', '🥜', '🌰', '🍫', '🍞', '🥓', '🌭', '🥒', '🥕', '🍎', '🍌', '🍇', '🌽', '🍯',
  // hands and objects
  '👆', '✋', '🦶', '✏️', '🖌️', '🪵', '🔨', '🪛', '🥄', '🧽', '🧹', '🧺', '📦', '🪣', '🔑', '📱', '🎾', '⚽', '🧲',
  // house and surroundings
  '🚪', '🪟', '🛏️', '🚿', '🧊', '🔥', '💨', '🌧️', '⛈️', '🌀', '🚗', '🚲', '🚂', '✈️', '🔊', '🎵', '⚡', '🫳',
  // people
  '🧍', '🚶', '🏃', '👶', '🧑‍🔧'];
const MOCK = location.protocol === 'file:' || new URLSearchParams(location.search).has('mock');

// ---------------- i18n (English default, optional German) ----------------
const DE = {
  tagline: 'Mausefallen-Detektor · RandeLab', tabLive: '📡 Live', tabLab: '🧪 Lernlabor', tabData: '📚 Datensatz',
  tabSettings: '⚙️ Einstellungen', noiseFloorMg: 'Grundrauschen (mg)', thresholdMg: 'Auslöseschwelle (mg)',
  eventsBoot: 'Ereignisse seit Start', snapsTotal: 'Schnapper total', samples: 'gelabelte Beispiele', uptime: 'Laufzeit',
  liveTitle: 'Live-Vibration', liveSub: 'dynamische Beschleunigung ohne Schwerkraft, letzte 10 s', threshold: 'Schwelle',
  noiseFloor: 'Grundrauschen', simTitle: 'Simulator', simSub: 'kein echter Sensor: künstliche Vibration auslösen',
  recentTitle: 'Letzte Ereignisse', recentSub: 'labeln, und das Modell lernt', labStep1: 'Was wirst du tun?',
  labHint: 'Klasse wählen, Test scharf schalten, dann genau das mit der Falle machen (zuschnappen lassen, auf den Tisch klopfen, vorbeilaufen …). Die nächste Vibration wird ein Trainingsbeispiel.',
  armTest: '🎯 Test scharf schalten', series: 'Serienmodus: nach jedem Behalten wieder scharf', cancel: 'abbrechen',
  labStep2: 'Aufgezeichnet. War es das?', keepAs: 'behalten als …', discard: '🗑 verwerfen', features: 'Merkmale',
  labStep3: 'Was das Modell weiss', goal: 'Ziel: mindestens 10 Beispiele pro Klasse. Schnapper aus verschiedenen Positionen = besser.',
  qualityTitle: 'Modellqualität', qualitySub: 'Leave-one-out-Test auf dem Gerät', refresh: '↻ aktualisieren',
  datasetTitle: 'Datensatz', exportCsv: '⬇ CSV exportieren', wipe: 'alles löschen', detTitle: 'Erkennung',
  kSigma: 'Auslösung: Rauschen + k·σ', minG: 'minimale Auslösung (g)', snapG: 'Schnapp-Spitze bei leerem Modell (g)',
  alertP: 'Alarm ab Schnapp-Wahrscheinlichkeit', unsureP: 'nach Label fragen ab', armedLbl: 'scharf (Telegram-Alarme)',
  notifyUnsure: 'bei Unsicherheit auf Telegram fragen', daily: 'Tagebuch jeden Tag um 20:00', voiceLbl: '🎙 Sprachnachrichten: der Philosoph liest Commits vor', tgTest: 'Testnachricht senden',
  tgUnpair: 'alle entkoppeln', tgCode: 'neuer Beitrittscode', sysTitle: 'System', wifiSetup: '📶 WLAN / Bot einrichten', ota: '⚡ OTA-Update',
  portal: 'Hotspot öffnen', reboot: 'neu starten', noEvents: 'Noch nichts passiert. Absolut nichts.',
  waiting: 'Warte auf', doItNow: 'jetzt machen!', kept: 'gespeichert', discarded: 'verworfen', saved: 'gespeichert',
  interview: '▶ Interview anhören', ghSub: 'jede Aufnahme, veröffentlicht', ghEvery: 'pushen alle (Minuten)', ghPush: 'jetzt pushen', ghOpen: 'Seite öffnen', newClass: '＋ neue Klasse', addClass: 'Klasse anlegen', newClassOpt: '＋ neue Klasse …', createKeep: '＋ anlegen & behalten',
  classRules: 'Name: a-z, 0-9, - und _ (max. 16). Löschen geht, solange die Klasse keine Beispiele hat.',
  confirmWipe: 'Wirklich den ganzen Datensatz löschen?', confirmReboot: 'Neu starten?', confirmPortal: 'Hotspot öffnen?',
};
let lang = localGet('polh-lang') || 'en';
const EN_CACHE = {};
function t(key, fallback) { return lang === 'de' && DE[key] ? DE[key] : (EN_CACHE[key] || fallback || key); }
function applyLang() {
  document.documentElement.lang = lang;
  document.querySelectorAll('[data-i18n]').forEach(el => {
    const k = el.dataset.i18n;
    if (!(k in EN_CACHE)) EN_CACHE[k] = el.textContent;
    el.textContent = t(k);
  });
  $('#btnLang').textContent = lang === 'de' ? 'EN' : 'DE';
}

const QUOTES = [
  'And so on, and so on.',
  'Ze mouse does not know it wants ze cheese. Ze cheese knows.',
  'A trap which never snaps is pure ideology.',
  'What is a vibration, really? *sniff*',
  'Ze real catastrophe is not ze snap. It is ze silence after.',
  'I would prefer not to be a false positive.',
];

// ---------------- helpers ----------------
function $(s) { return document.querySelector(s); }
function el(tag, attrs = {}, html = '') { const e = document.createElement(tag); Object.assign(e, attrs); if (html) e.innerHTML = html; return e; }
function localGet(k) { try { return localStorage.getItem(k); } catch (e) { return null; } }
function localSet(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* private mode */ } }
function fmtDur(ms) {
  const s = Math.floor(ms / 1000), d = Math.floor(s / 86400), h = Math.floor(s % 86400 / 3600), m = Math.floor(s % 3600 / 60);
  return d ? `${d}d ${h}h` : h ? `${h}h ${m}m` : `${m}m ${s % 60}s`;
}
function pct(p) { return Math.round(p * 100) + ' %'; }
function toast(msg) { const tt = $('#toast'); tt.textContent = msg; tt.classList.add('show'); clearTimeout(toast.t); toast.t = setTimeout(() => tt.classList.remove('show'), 2500); }

const api = {
  async get(path) {
    if (MOCK) return window.polhMock.get(path);
    const r = await fetch(path);
    if (!r.ok) throw new Error(path + ' ' + r.status);
    return r.json();
  },
  async post(path, params = {}) {
    if (MOCK) return window.polhMock.post(path, params);
    const r = await fetch(path, { method: 'POST', body: new URLSearchParams(params) });
    const j = await r.json().catch(() => ({}));
    if (!r.ok || j.ok === false) throw new Error(j.error || r.status);
    return j;
  },
};

// ---------------- tabs ----------------
function showTab() {
  const name = (location.hash || '#live').slice(1);
  document.querySelectorAll('section.tab').forEach(s => s.classList.toggle('active', s.id === 'tab-' + name));
  document.querySelectorAll('nav a').forEach(a => a.classList.toggle('active', a.dataset.tab === name));
  if (name === 'lab') loadModel();
  if (name === 'data') loadDataset();
  if (name === 'settings') loadConfig();
  requestAnimationFrame(() => drawLive(true));
}

// ---------------- canvas ----------------
function prepCanvas(cv) {
  const dpr = window.devicePixelRatio || 1, w = cv.clientWidth, h = cv.getAttribute('height') | 0;
  if (cv.width !== Math.round(w * dpr)) { cv.width = Math.round(w * dpr); cv.height = Math.round(h * dpr); cv.style.height = h + 'px'; }
  const ctx = cv.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  return { ctx, w, h };
}
// getComputedStyle on every frame is expensive on phones: colours are cached until ze theme changes
const cssCache = {};
function css(v) { return cssCache[v] ??= getComputedStyle(document.body).getPropertyValue(v).trim(); }
function cssReset() { for (const k in cssCache) delete cssCache[k]; }

// ze trap sends 50 points per second in batches every 100 ms. Drawing on a timer made ze plot
// jump in steps; now it is drawn every animation frame and scrolls smoothly between batches.
const LIVE_N = 500, LIVE_MS = 20;  // points shown, ms per point
const live = { v: new Array(LIVE_N).fill(0), thr: 0, base: 0, dirty: true, at: 0, batch: 5 };
function drawLive(force) {
  if (!live.dirty && !force) return;
  live.dirty = false;
  const cv = $('#cvLive');
  if (!cv.offsetParent) return;
  const { ctx, w, h } = prepCanvas(cv);
  // log scale: 1 mg (sensor noise) up to a few g (snap) in one view
  const top = Math.log10(Math.max(live.thr * 3, ...live.v, 100)), bot = 0;
  const y = v => h - 4 - ((Math.log10(Math.max(v, 1)) - bot) / (top - bot)) * (h - 18);
  ctx.strokeStyle = css('--grid'); ctx.lineWidth = 1; ctx.font = '11px sans-serif'; ctx.fillStyle = css('--muted');
  for (let d = 1; d <= 1e4 && Math.log10(d) <= top; d *= 10) { ctx.beginPath(); ctx.moveTo(0, y(d)); ctx.lineTo(w, y(d)); ctx.stroke(); ctx.fillText(d >= 1000 ? d / 1000 + ' g' : d + ' mg', 4, y(d) - 2); }
  const line = (val, col, dash) => { ctx.setLineDash(dash); ctx.strokeStyle = col; ctx.beginPath(); ctx.moveTo(0, y(val)); ctx.lineTo(w, y(val)); ctx.stroke(); ctx.setLineDash([]); };
  line(live.base, css('--muted'), [2, 4]);
  line(live.thr, css('--red'), [6, 4]);
  ctx.strokeStyle = css('--mag'); ctx.lineWidth = 1.5; ctx.beginPath();
  // scroll by ze time since ze last batch arrived, at most one batch, so it never runs ahead
  const shift = Math.min(live.batch, (performance.now() - live.at) / LIVE_MS);
  const span = LIVE_N - 1 - live.batch, last = live.v.length - 1;
  let started = false;
  live.v.forEach((v, i) => {
    const x = (i - (last - span) - shift) / span * w;
    if (x < -10) return;  // scrolled out on ze left
    if (started) ctx.lineTo(x, y(v)); else { ctx.moveTo(x, y(v)); started = true; }
  });
  ctx.stroke();
}
(function liveLoop() {
  if (!document.hidden) drawLive(true);
  requestAnimationFrame(liveLoop);
})();

function drawWindow(cv, win) {
  cv.classList.remove('hide');
  const { ctx, w, h } = prepCanvas(cv);
  const n = win.n, mag = new Array(n);
  let max = 1;
  for (let i = 0; i < n; i++) {
    mag[i] = Math.hypot(win.x[i], win.y[i], win.z[i]);
    max = Math.max(max, mag[i]);
  }
  const mid = h / 2, sc = (h / 2 - 8) / max, X = i => i / (n - 1) * w;
  ctx.strokeStyle = css('--grid'); ctx.beginPath(); ctx.moveTo(0, mid); ctx.lineTo(w, mid); ctx.stroke();
  const tx = X(win.trig); ctx.setLineDash([4, 4]); ctx.strokeStyle = css('--muted'); ctx.beginPath(); ctx.moveTo(tx, 0); ctx.lineTo(tx, h); ctx.stroke(); ctx.setLineDash([]);
  const thr = win.threshold_g * 1000;
  ctx.strokeStyle = css('--red'); ctx.setLineDash([6, 4]); ctx.beginPath(); ctx.moveTo(0, mid - thr * sc); ctx.lineTo(w, mid - thr * sc); ctx.stroke(); ctx.setLineDash([]);
  const trace = (arr, col, lw) => { ctx.strokeStyle = col; ctx.lineWidth = lw; ctx.beginPath(); arr.forEach((v, i) => i ? ctx.lineTo(X(i), mid - v * sc) : ctx.moveTo(X(i), mid - v * sc)); ctx.stroke(); };
  trace(win.x, css('--x'), 1); trace(win.y, css('--y'), 1); trace(win.z, css('--z'), 1); trace(mag, css('--mag'), 1.5);
  ctx.fillStyle = css('--muted'); ctx.font = '11px sans-serif';
  ctx.fillText(`±${(max / 1000).toFixed(2)} g · ${(n / win.rate * 1000).toFixed(0)} ms @ ${win.rate} Hz`, 4, 12);
}

// ---------------- status ----------------
let status = null;
// ze comrades of ze trap: owner first, the others can be removed here or with /users in Telegram
function renderTgUsers(tg) {
  const box = $('#tgUsers');
  const users = tg.users || [];
  const sig = JSON.stringify(users) + tg.join_code + tg.bot;
  if (box.dataset.sig === sig) return;  // status refreshes often, do not rebuild ze buttons each time
  box.dataset.sig = sig;
  box.innerHTML = '';
  for (const u of users) {
    const row = document.createElement('div');
    row.className = 'row';
    const name = document.createElement('span');
    name.textContent = (u.owner ? '👑 ' : '• ') + u.name + (u.owner ? ' (owner)' : '');
    row.append(name);
    if (!u.owner) {
      const b = document.createElement('button');
      b.className = 'small';
      b.textContent = '🚪 remove';
      b.onclick = async () => {
        if (!confirm(`Remove ${u.name} from ze trap?`)) return;
        try { await api.post('/api/telegram/remove', { chat: u.chat }); loadStatus(); } catch (e) { toast('❌ ' + e.message); }
      };
      row.append(b);
    }
    box.append(row);
  }
  const link = tg.bot && tg.join_code ? `https://t.me/${tg.bot}?start=${tg.join_code}` : '';
  $('#tgJoin').innerHTML = !tg.configured || !tg.join_code ? '' :
    `🔑 Join code <code>${tg.join_code}</code>: a new comrade sends <code>/start ${tg.join_code}</code> to ze bot` +
    (link ? ` or opens <a href="${link}" target="_blank" rel="noopener">${link}</a>` : '') +
    '. Without ze code they knock, and ze owner gets a ✅/🚫 button.';
}

async function loadStatus() {
  try {
    status = await api.get('/api/status');
  } catch (e) {
    $('#pillConn').className = 'pill bad'; $('#pillConn').textContent = 'offline';
    return;
  }
  const s = status;
  $('#ver').textContent = 'v' + s.version;
  const net = s.net;
  $('#pillConn').className = 'pill ' + (net.mode === 'online' ? 'ok' : net.mode === 'portal' ? 'warn' : '');
  $('#pillConn').textContent = net.mode === 'online' ? `📶 ${net.ssid} ${net.rssi} dBm` : net.mode === 'portal' ? '📶 hotspot' : net.mode;
  $('#pillSensor').className = 'pill ' + (s.sensor.ok ? 'ok' : 'bad');
  $('#pillSensor').textContent = (s.sensor.simulated ? '🧪 ' : '🧲 ') + (s.sensor.ok ? `${s.sensor.name} ${s.sensor.rate} Hz` : 'no sensor');
  $('#pillArmed').className = 'pill ' + (s.armed ? 'ok' : 'warn');
  $('#pillArmed').textContent = s.armed ? '🎯 armed' : '😴 disarmed';
  $('#sNoise').textContent = (s.sensor.baseline_g * 1000).toFixed(1);
  $('#sThr').textContent = (s.sensor.threshold_g * 1000).toFixed(0);
  $('#sEvents').textContent = s.stats.events;
  $('#sSnaps').textContent = s.stats.snaps_total;
  $('#sSamples').textContent = s.lab.samples;
  $('#sUptime').textContent = fmtDur(s.uptime_ms);
  $('#simPanel').classList.toggle('hide', !s.sensor.simulated);
  $('#labSim').classList.toggle('hide', !s.sensor.simulated);
  const tg = s.telegram;
  $('#tgInfo').textContent = !tg.configured ? 'No bot token yet → WiFi / bot setup.' :
    `Bot @${tg.bot || '?'} · ${tg.paired ? (tg.users || []).length + '/' + (tg.max_users || 10) + ' comrades' : 'not paired: send /start to the bot, ze first one is ze owner'}` +
    `${tg.pending ? ' · ' + tg.pending + ' knocking (approve in Telegram)' : ''}${tg.last_error ? ' · last error: ' + tg.last_error : ''}`;
  renderTgUsers(tg);
  const gh = s.github || {};
  $('#lnkPages').href = gh.pages_url || '#';
  $('#ghInfo').textContent = !gh.configured ? 'No GitHub token yet → WiFi / bot setup (fine-grained token, Contents read+write on ' + (gh.repo || 'the repo') + ').' :
    `${gh.repo} @ ${gh.branch} · last push ${gh.last_ok_ago_ms ? fmtDur(gh.last_ok_ago_ms) + ' ago' : 'never'}` +
    `${gh.backlog > 0 ? ' · ' + gh.backlog + ' waiting' : ''}${gh.last_error ? ' · ❌ ' + gh.last_error : gh.last_message ? ' · “' + gh.last_message + '”' : ''}`;
  $('#sysInfo').textContent = `IP ${net.ip} · heap ${(s.heap / 1024).toFixed(0)} kB · PSRAM ${(s.psram / 1048576).toFixed(1)} MB · captures ${s.fs.captures} (${s.fs.labelled} labelled, ${s.fs.raw} waveforms, ${s.fs.raw_kb}/${s.fs.raw_budget_kb} kB) · dataset ${(s.fs.used / 1024).toFixed(0)} / ${(s.fs.total / 1024).toFixed(0)} kB · gravity [${s.sensor.gravity.map(v => v.toFixed(2)).join(', ')}] g`;
  if (!wsOpen) { live.thr = s.sensor.threshold_g * 1000; live.base = s.sensor.baseline_g * 1000; }
  updateLab(s.lab.armed_label, s.lab.remaining_ms);
}

// ---------------- recent events ----------------
let recent = [];
async function loadEvents() {
  try { recent = await api.get('/api/events'); } catch (e) { return; }
  renderEvents();
}
function renderEvents() {
  const box = $('#recentList');
  box.innerHTML = '';
  if (!recent.length) { box.append(el('span', { className: 'muted' }, t('noEvents', 'Nothing happened yet. Absolutely nothing.'))); return; }
  recent.forEach(e => {
    const isSnap = e.snap_p >= (cfg ? cfg.alert_p : 0.7);
    const card = el('div', { className: 'event' + (isSnap ? ' snap' : '') + (e.candidate ? ' lab' : '') });
    const when = e.ts ? new Date(e.ts * 1000).toLocaleTimeString() : fmtDur(e.ago_ms) + ' ago';
    card.append(el('div', { className: 'head' },
      `<span class="cls">${EMOJI[e.cls]} ${e.cls}</span><span class="mono">#${e.id}</span><span class="muted">${when}</span>` +
      `<span class="mono">peak ${e.peak_g.toFixed(2)} g</span><span class="mono">snap ${pct(e.snap_p)}</span><span class="muted">${e.method}</span>` +
      (e.stored_label ? `<span class="pill ok">✓ ${e.stored_label}</span>` : '')));
    const row = el('div', { className: 'row', style: 'margin-top:6px' });
    CLASSES.forEach(c => {
      const b = el('button', { className: 'small', textContent: `${EMOJI[c]} ${c}` });
      if (e.stored_label === c) b.disabled = true;
      b.onclick = async () => { await api.post('/api/lab/label', { event: e.id, label: c }); toast(`#${e.id} → ${c}`); loadEvents(); loadStatus(); };
      row.append(b);
    });
    const cv = el('canvas', { className: 'hide', height: 160 });
    const show = el('button', { className: 'small', textContent: '📈' });
    show.onclick = async () => { if (!cv.classList.contains('hide')) { cv.classList.add('hide'); return; } drawWindow(cv, await api.get('/api/window?event=' + e.id)); };
    row.append(show);
    card.append(row, cv);
    box.append(card);
  });
}

// ---------------- learning lab ----------------
let labLabel = localGet('polh-lab-label') || 'snap';
let candidate = null;
async function loadClasses() {
  try { classInfo = await api.get('/api/classes'); } catch (e) { return; }
  CLASSES = classInfo.map(c => c.name);
  EMOJI = Object.fromEntries(classInfo.map(c => [c.name, c.emoji]));
  if (!CLASSES.includes(labLabel)) labLabel = 'snap';
  renderLabChips(); renderEvents();
}
// creates a class from a name + emoji input pair; returns ze name or null
async function createClass(nameInput, emojiInput) {
  const name = nameInput.value.trim().toLowerCase().replace(/\s+/g, '-'), emoji = emojiInput.value.trim() || '🏷️';
  if (!/^[a-z0-9_-]{1,16}$/.test(name)) { toast('❌ ' + t('classRules', 'name: a-z, 0-9, - and _ (max 16)')); nameInput.focus(); return null; }
  if (!CLASSES.includes(name)) {
    try { await api.post('/api/classes/add', { name, emoji }); } catch (e) { toast('❌ ' + e.message); return null; }
    toast(`${emoji} ${name} ✅`);
  }
  nameInput.value = ''; emojiInput.value = '';
  await loadClasses();
  return name;
}
async function addClass() {
  const name = await createClass($('#newName'), $('#newEmoji'));
  if (!name) return;
  labLabel = name; localSet('polh-lab-label', name);
  renderLabChips();
}
function emojiPicker(box, input) {
  box.innerHTML = '';
  EMOJI_PICKS.forEach(e => { const b = el('button', { className: 'chip', textContent: e }); b.onclick = () => { input.value = e; }; box.append(b); });
}
function renderLabChips() {
  const box = $('#labChips'); box.innerHTML = '';
  CLASSES.forEach(c => {
    const ch = el('button', { className: 'chip' + (c === labLabel ? ' sel' : ''), textContent: `${EMOJI[c]} ${c}` });
    ch.onclick = () => { labLabel = c; localSet('polh-lab-label', c); renderLabChips(); };
    box.append(ch);
  });
  const plus = el('button', { className: 'chip', textContent: t('newClass', '＋ new class') });
  plus.onclick = () => { $('#classForm').classList.toggle('hide'); $('#newName').focus(); };
  box.append(plus);
  const cl = $('#customList'); cl.innerHTML = '';
  classInfo.filter(c => !c.builtin).forEach(c => {
    const b = el('button', { className: 'chip', textContent: `${c.emoji} ${c.name} (${c.n}) ✕`, title: 'delete' });
    b.onclick = async () => {
      if (!confirm(`${c.emoji} ${c.name} ✕ ?`)) return;
      try { await api.post('/api/classes/delete', { name: c.name }); loadClasses(); } catch (e) { toast('❌ ' + e.message); }
    };
    cl.append(b);
  });
  const sel = $('#selKeepAs'), keep = sel.value; sel.innerHTML = '';
  CLASSES.forEach(c => sel.append(el('option', { value: c, textContent: `${EMOJI[c]} ${c}` })));
  sel.append(el('option', { value: '__new', textContent: t('newClassOpt', '＋ new class …') }));
  if (keep && keep !== '__new' && CLASSES.includes(keep)) sel.value = keep;
}
function updateLab(armedLabel, remainingMs) {
  const armed = !!armedLabel && remainingMs > 0;
  $('#labIdle').classList.toggle('hide', armed);
  $('#labArmed').classList.toggle('hide', !armed);
  if (armed) {
    $('#labArmedText').textContent = `${EMOJI[armedLabel]} ${t('waiting', 'Waiting for')} “${armedLabel}” — ${t('doItNow', 'do it now!')}`;
    $('#labCountdown').textContent = Math.ceil(remainingMs / 1000);
  }
}
async function armTest() {
  await api.post('/api/lab/arm', { label: labLabel });
  $('#candPanel').classList.add('hide');
  updateLab(labLabel, 60000);
}
async function showCandidate(e) {
  candidate = e;
  $('#candPanel').classList.remove('hide');
  $('#btnKeep').textContent = `✅ keep as ${EMOJI[e.test_label]} ${e.test_label}`;
  $('#selKeepAs').value = e.test_label;
  $('#candPred').textContent = `#${e.id} · peak ${e.peak_g.toFixed(2)} g · model says ${e.cls} (snap ${pct(e.snap_p)}, ${e.method})`;
  const ft = $('#candFeat'); ft.innerHTML = '';
  Object.entries(e.features).forEach(([k, v]) => ft.append(el('tr', {}, `<td>${k}</td><td class="num">${v.toFixed(3)}</td>`)));
  try { drawWindow($('#cvCand'), await api.get('/api/window?event=' + e.id)); } catch (err) { /* evicted */ }
}
async function keepCandidate(label) {
  if (!candidate) return;
  await api.post('/api/lab/label', { event: candidate.id, label });
  toast(`${EMOJI[label]} ${t('kept', 'kept')} (#${candidate.id})`);
  afterCandidate();
}
async function discardCandidate() {
  if (!candidate) return;
  await api.post('/api/lab/discard', { event: candidate.id }).catch(() => {});
  toast(t('discarded', 'discarded'));
  afterCandidate();
}
function afterCandidate() {
  $('#keepNewForm').classList.add('hide');
  candidate = null;
  $('#candPanel').classList.add('hide');
  loadModel(); loadStatus(); loadEvents();
  if ($('#chkSeries').checked) armTest();
}

async function loadModel() {
  let m;
  try { m = await api.get('/api/model'); } catch (e) { return; }
  const tb = $('#classTable');
  tb.innerHTML = '<tr><th>class</th><th class="num">n</th><th style="width:40%"></th><th class="num">peak (g)</th><th class="num">duration</th></tr>';
  const maxN = Math.max(10, ...m.classes.map(c => c.n));
  m.classes.forEach(c => {
    const peak = c.n ? Math.pow(10, c.mean[0]).toFixed(2) : '–';
    const dur = c.n ? c.mean[3].toFixed(0) + ' ms' : '–';
    tb.append(el('tr', {}, `<td>${c.emoji} ${c.name}</td><td class="num">${c.n}</td><td><div class="bar"><i style="width:${c.n / maxN * 100}%"></i></div></td><td class="num">${peak}</td><td class="num">${dur}</td>`));
  });
  $('#modelSummary').textContent = `${m.total} samples`;
  loadClasses();
  const l = m.loo;
  const prec = l.snap_tp + l.snap_fp ? l.snap_tp / (l.snap_tp + l.snap_fp) : 0;
  const rec = l.snap_tp + l.snap_fn ? l.snap_tp / (l.snap_tp + l.snap_fn) : 0;
  $('#looSummary').textContent = l.total ? `accuracy ${pct(l.correct / l.total)} · snap recall ${pct(rec)} · snap precision ${pct(prec)}` : 'no data yet';
  const cm = $('#cmTable');
  cm.innerHTML = '<tr><th>true ↓ / predicted →</th>' + CLASSES.map(c => `<th>${EMOJI[c]}</th>`).join('') + '</tr>';
  l.confusion.forEach((row, i) => cm.append(el('tr', {}, `<th>${EMOJI[CLASSES[i]]} ${CLASSES[i]}</th>` + row.map((v, j) => `<td class="${i === j && v ? 'diag' : ''}">${v || '·'}</td>`).join(''))));
}

// ---------------- dataset ----------------
async function loadDataset() {
  let list;
  try { list = await api.get('/api/samples'); } catch (e) { return; }
  $('#dsInfo').textContent = `${list.length} captures, ${list.filter(s => s.label).length} labelled`;
  const tb = $('#dsTable');
  tb.innerHTML = '<tr><th>#</th><th>time</th><th>label</th><th>model said</th><th class="num">peak (g)</th><th></th></tr>';
  list.slice().reverse().forEach(s => {
    const tr = el('tr');
    const sel = el('select');
    if (!s.label) sel.append(el('option', { value: '', textContent: '— unlabelled', selected: true, disabled: true }));
    CLASSES.forEach(c => sel.append(el('option', { value: c, textContent: `${EMOJI[c]} ${c}`, selected: c === s.label })));
    sel.onchange = async () => { await api.post('/api/samples/relabel', { id: s.id, label: sel.value }); toast(`#${s.id} → ${sel.value}`); };
    tr.append(el('td', { className: 'mono', textContent: s.id }), el('td', { textContent: s.ts ? new Date(s.ts * 1000).toLocaleString() : '–' }));
    const tdSel = el('td'); tdSel.append(sel); tr.append(tdSel);
    tr.append(el('td', { textContent: s.pred ? `${EMOJI[s.pred] || '❔'} ${s.pred}${s.snap_p >= 0 ? ' · ' + pct(s.snap_p) : ''}` : '' }));
    tr.append(el('td', { className: 'num', textContent: s.peak_g.toFixed(2) + (s.published ? ' 🐙' : '') }));
    const act = el('td');
    if (s.raw) { const b = el('button', { className: 'small', textContent: '📈' }); b.onclick = async () => drawWindow($('#cvSample'), await api.get('/api/window?sample=' + s.id)); act.append(b); }
    const del = el('button', { className: 'small danger', textContent: '✕' });
    del.onclick = async () => { await api.post('/api/samples/delete', { id: s.id }); loadDataset(); loadStatus(); };
    act.append(' ', del); tr.append(act);
    tb.append(tr);
  });
}

// ---------------- settings ----------------
let cfg = null;
async function loadConfig() {
  try { cfg = await api.get('/api/config'); } catch (e) { return; }
  document.querySelectorAll('[data-cfg]').forEach(inp => {
    const k = inp.dataset.cfg;
    if (inp.type === 'checkbox') inp.checked = !!cfg[k];
    else { inp.value = cfg[k]; inp.nextElementSibling.textContent = (+cfg[k]).toFixed(2); }
  });
}
function bindConfig() {
  let timer;
  document.querySelectorAll('[data-cfg]').forEach(inp => {
    inp.addEventListener('input', () => { if (inp.type === 'range') inp.nextElementSibling.textContent = (+inp.value).toFixed(2); });
    inp.addEventListener('change', () => {
      clearTimeout(timer);
      timer = setTimeout(async () => {
        const p = {};
        document.querySelectorAll('[data-cfg]').forEach(i => p[i.dataset.cfg] = i.type === 'checkbox' ? (i.checked ? 1 : 0) : i.value);
        await api.post('/api/config', p);
        cfg = await api.get('/api/config');
        toast('💾 ' + t('saved', 'saved'));
        loadStatus();
      }, 300);
    });
  });
}

// ---------------- websocket ----------------
let wsOpen = false;
function onMessage(m) {
  if (m.t === 'live') {
    live.v.push(...m.v); live.v.splice(0, live.v.length - LIVE_N);
    live.thr = m.thr; live.base = m.base; live.dirty = true;
    live.batch = Math.max(1, Math.min(10, m.v.length)); live.at = performance.now();
  } else if (m.t === 'event') {
    recent = [m.e, ...recent.filter(x => x.id !== m.e.id)].slice(0, 6); renderEvents();
    if (m.e.candidate) showCandidate(m.e);
    loadStatus();
  } else if (m.t === 'lab') {
    updateLab(m.armed_label, m.remaining_ms);
  }
}
function connectWs() {
  if (MOCK) { window.polhMock.subscribe(onMessage); wsOpen = true; return; }
  const ws = new WebSocket(`ws://${location.host}/ws`);
  ws.onopen = () => { wsOpen = true; };
  ws.onmessage = ev => { try { onMessage(JSON.parse(ev.data)); } catch (e) { /* ignore */ } };
  ws.onclose = () => { wsOpen = false; setTimeout(connectWs, 2000); };
}

// ---------------- wiring ----------------
function init() {
  if (localGet('polh-theme') === 'randen') document.body.classList.add('theme-randen');
  $('#btnTheme').onclick = () => { document.body.classList.toggle('theme-randen'); localSet('polh-theme', document.body.classList.contains('theme-randen') ? 'randen' : 'pink'); cssReset(); drawLive(true); };
  $('#btnLang').onclick = () => { lang = lang === 'de' ? 'en' : 'de'; localSet('polh-lang', lang); applyLang(); renderEvents(); };
  applyLang();
  $('#quote').textContent = QUOTES[Math.floor(Math.random() * QUOTES.length)];
  const aud = $('#audInterview'), btnI = $('#btnInterview');
  const labelI = () => { btnI.textContent = aud.paused ? t('interview', '▶ listen to ze interview') : '⏸ pause'; };
  btnI.onclick = () => { aud.paused ? aud.play().catch(() => toast('❌ audio')) : aud.pause(); };
  aud.onplay = aud.onpause = aud.onended = labelI;
  aud.addEventListener('play', () => $('#transcript').classList.remove('hide'), { once: true });
  window.polhTranscript(aud, $('#transcript'));
  window.addEventListener('hashchange', showTab);
  window.addEventListener('resize', () => drawLive(true));

  ['#simChips', '#labSimChips'].forEach(sel => BUILTIN.forEach(c => {
    const b = el('button', { className: 'chip', textContent: `${EMOJI[c]} ${c}` });
    b.onclick = () => api.post('/api/sim/inject', { label: c });
    $(sel).append(b);
  }));
  renderLabChips();
  emojiPicker($('#emojiPicks'), $('#newEmoji'));
  $('#btnAddClass').onclick = addClass;
  $('#newName').addEventListener('keydown', ev => { if (ev.key === 'Enter') addClass(); });
  loadClasses();
  $('#btnArm').onclick = armTest;
  $('#btnCancel').onclick = async () => { await api.post('/api/lab/cancel'); updateLab(null, 0); };
  $('#btnKeep').onclick = () => candidate && keepCandidate(candidate.test_label);
  // "keep as": ze last option creates a new class right here, then keeps ze recording in it
  const keepNew = show => { $('#keepNewForm').classList.toggle('hide', !show); if (show) $('#keepNewName').focus(); };
  $('#selKeepAs').onchange = () => keepNew($('#selKeepAs').value === '__new');
  $('#btnKeepAs').onclick = () => $('#selKeepAs').value === '__new' ? keepNew(true) : keepCandidate($('#selKeepAs').value);
  const createAndKeep = async () => {
    const name = await createClass($('#keepNewName'), $('#keepNewEmoji'));
    if (!name) return;
    keepNew(false);
    keepCandidate(name);
  };
  $('#btnKeepNew').onclick = createAndKeep;
  $('#keepNewName').addEventListener('keydown', ev => { if (ev.key === 'Enter') createAndKeep(); });
  emojiPicker($('#keepNewPicks'), $('#keepNewEmoji'));
  $('#btnDiscard').onclick = discardCandidate;
  $('#btnLoo').onclick = loadModel;
  $('#btnDsRefresh').onclick = loadDataset;
  $('#btnWipe').onclick = async () => { if (confirm(t('confirmWipe', 'Really wipe the whole dataset?'))) { await api.post('/api/model/wipe'); loadDataset(); loadStatus(); } };
  $('#btnGhPush').onclick = async () => { try { await api.post('/api/github/push'); toast('🐙 pushing…'); } catch (e) { toast('❌ ' + e.message); } };
  $('#btnTgTest').onclick = async () => { try { await api.post('/api/telegram/test'); toast('✈️ sent'); } catch (e) { toast('❌ ' + e.message); } };
  $('#btnTgUnpair').onclick = async () => { if (!confirm('Unpair everybody? Ze next /start becomes ze new owner.')) return; await api.post('/api/telegram/unpair'); loadStatus(); };
  $('#btnTgCode').onclick = async () => { try { await api.post('/api/telegram/newcode'); toast('🔑 new code, ze old one is dead'); loadStatus(); } catch (e) { toast('❌ ' + e.message); } };
  $('#btnPortal').onclick = async () => { if (confirm(t('confirmPortal', 'Open the hotspot?'))) await api.post('/api/portal'); };
  $('#btnReboot').onclick = async () => { if (confirm(t('confirmReboot', 'Reboot?'))) await api.post('/api/reboot'); };
  bindConfig();

  showTab();
  loadConfig();
  loadStatus();
  loadEvents();
  connectWs();
  setInterval(loadStatus, 3000);
}
init();
