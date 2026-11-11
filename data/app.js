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
  noiseFloor: 'Grundrauschen', viewClassic: '📈 Klassisch', viewFancy: '🧊 3D & Multi-Sensor',
  msMagLbl: '|a| · Spitze', msXyzLbl: 'Δx · Δy · Δz (mg)', msGyroLbl: '🌀 Gyro ω (°/s)',
  msTiltLbl: '🧭 Orientierung (R · N · G)', msGravLbl: '🧲 Schwerkraft (g)', msTempLbl: '🌡️ Chip-Temp.',
  fancyHint: '🖱️ 3D-Falle ziehen zum Drehen · Doppelklick = Reset',
  simTitle: 'Simulator', simSub: 'kein echter Sensor: künstliche Vibration auslösen',
  recentTitle: 'Letzte Ereignisse', recentSub: 'labeln, und das Modell lernt', labStep1: 'Was wirst du tun?',
  labHint: 'Klasse wählen, Test scharf schalten, dann genau das mit der Falle machen (zuschnappen lassen, auf den Tisch klopfen, vorbeilaufen …). Die nächste Vibration wird ein Trainingsbeispiel.',
  armTest: '🎯 Test scharf schalten', series: 'Serienmodus: nach jedem Behalten wieder scharf', cancel: 'abbrechen',
  labStep2: 'Aufgezeichnet. War es das?', keepAs: 'behalten als …', discard: '🗑 verwerfen', features: 'Merkmale',
  labStep3: 'Was das Modell weiss', goal: 'Ziel: mindestens 10 Beispiele pro Klasse. Schnapper aus verschiedenen Positionen = besser.',
  qualityTitle: 'Modellqualität', qualitySub: 'Leave-one-out-Test auf dem Gerät', refresh: '↻ aktualisieren',
  datasetTitle: 'Datensatz', exportCsv: '⬇ CSV exportieren', purgeUnlabelled: '🧹 ungelabelte löschen', wipe: 'alles löschen', detTitle: 'Erkennung',
  kSigma: 'Auslösung: Rauschen + k·σ', minG: 'minimale Auslösung (g)', snapG: 'Schnapp-Spitze bei leerem Modell (g)',
  alertP: 'Alarm ab Schnapp-Wahrscheinlichkeit', unsureP: 'nach Label fragen ab', armedLbl: 'scharf (Telegram-Alarme)',
  notifyUnsure: 'bei Unsicherheit auf Telegram fragen', daily: 'Tagebuch jeden Tag um 20:00', voiceLbl: '🎙 Sprachnachrichten: der Philosoph liest Commits vor', tgTest: 'Testnachricht senden',
  tgUnpair: 'alle entkoppeln', tgCode: 'neuer Beitrittscode', sysTitle: 'System', wifiSetup: '📶 WLAN / Bot einrichten', ota: '⚡ OTA-Update',
  portal: 'Hotspot öffnen', reboot: 'neu starten', noEvents: 'Noch nichts passiert. Absolut nichts.',
  waiting: 'Warte auf', doItNow: 'jetzt machen!', kept: 'gespeichert', discarded: 'verworfen', saved: 'gespeichert',
  interview: '▶ Interview anhören', ghSub: 'jede Aufnahme, veröffentlicht', ghEvery: 'pushen alle (Minuten)', ghPush: 'jetzt pushen', ghOpen: 'Seite öffnen', newClass: '＋ neue Klasse', addClass: 'Klasse anlegen', newClassOpt: '＋ neue Klasse …', createKeep: '＋ anlegen & behalten',
  classRules: 'Name: a-z, 0-9, - und _ (max. 16). Löschen geht, solange die Klasse keine Beispiele hat.',
  confirmPurge: 'Wirklich alle ungelabelten Aufnahmen löschen? Gelabelte Trainingsdaten bleiben erhalten.',
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

// ze trap sends 50 points per second in batches every 50 ms. Drawing on a timer made ze plot
// jump in steps; now it is drawn every animation frame and scrolls smoothly between batches.
const LIVE_N = 520, LIVE_SPAN = 480, LIVE_MS = 20, LIVE_CUSHION = 3.5;  // 50 Hz = 20 ms/point, 3.5-pt jitter cushion
const live = {
  v: new Array(LIVE_N).fill(0),
  x: new Array(LIVE_N).fill(0),
  y: new Array(LIVE_N).fill(0),
  z: new Array(LIVE_N).fill(0),
  wx: new Array(LIVE_N).fill(0),
  wy: new Array(LIVE_N).fill(0),
  wz: new Array(LIVE_N).fill(0),
  roll: new Array(LIVE_N).fill(0),
  pitch: new Array(LIVE_N).fill(0),
  yaw: new Array(LIVE_N).fill(0),
  g: [0, 0, 1],
  gSmooth: [0, 0, 1],
  ori: [0, 0, 0],
  oriSmooth: [0, 0, 0],
  w: [0, 0, 0],
  temp: null,
  peakHold: 0,
  thr: 0,
  base: 0,
  dirty: true,
  lag: 0,
  lastFrame: 0,
  top: 2.3,
  xyzRange: 50,
  gyroRange: 10,
  oriRange: 30,
  init: false,
  view: localGet('polh-live-view') || 'fancy',
  orbit: { yaw: -0.62, pitch: 0.42, dragging: false, lastX: 0, lastY: 0 },
  rings: [],
  barSnap: 0.0,       // 0.0 = armed (open), 1.0 = snapped (closed)
  barSnapTarget: 0.0, // target state
  lastSnapEventId: 0,
};

function setLiveView(v) {
  live.view = v === 'classic' ? 'classic' : 'fancy';
  localSet('polh-live-view', live.view);
  $('#liveClassic').classList.toggle('hide', live.view !== 'classic');
  $('#liveFancy').classList.toggle('hide', live.view !== 'fancy');
  $('#btnViewClassic').classList.toggle('active', live.view === 'classic');
  $('#btnViewFancy').classList.toggle('active', live.view === 'fancy');
  live.dirty = true;
  requestAnimationFrame(() => drawLive(true));
}

function interpAt(arr, idx, frac) {
  const v0 = arr[idx] || 0;
  return idx + 1 < arr.length ? v0 + frac * ((arr[idx + 1] || 0) - v0) : v0;
}

function drawLive(force) {
  const activeCv = live.view === 'fancy' ? $('#cvFancy') : $('#cvLive');
  if (!activeCv || !activeCv.offsetParent) return;
  const now = performance.now();
  const dt = live.lastFrame ? Math.min(100, now - live.lastFrame) : 16;
  live.lastFrame = now;
  if (live.lag > 0.001) {
    // proportional playout cushion: slows down gently if a WiFi packet is late so ze line never freezes
    const rate = live.lag >= LIVE_CUSHION
      ? Math.min(2.2, 1 + (live.lag - LIVE_CUSHION) * 0.14)
      : Math.max(0.15, live.lag / LIVE_CUSHION);
    live.lag = Math.max(0, live.lag - (dt / LIVE_MS) * rate);
    live.dirty = true;
  }
  if (live.rings.length) live.dirty = true;
  if (!live.dirty && !force) return;
  live.dirty = false;

  const head = (live.v.length - 1) - live.lag;
  const left = head - LIVE_SPAN;
  const iStart = Math.max(0, Math.floor(left));
  const iEnd = Math.min(live.v.length - 1, Math.floor(head));
  const frac = head - iEnd;

  if (live.view === 'fancy') {
    drawFancy(activeCv, head, left, iStart, iEnd, frac, dt);
    return;
  }

  const { ctx, w, h } = prepCanvas(activeCv);
  let peak = Math.max(live.thr * 3, 100);
  for (let i = iStart; i <= iEnd; i++) if (live.v[i] > peak) peak = live.v[i];
  // log scale: 1 mg (sensor noise) up to a few g (snap) in one view, smoothed vertically
  const targetTop = Math.log10(peak);
  live.top = targetTop >= live.top ? targetTop : live.top + (targetTop - live.top) * 0.15;
  const top = live.top, bot = 0;
  const y = v => h - 4 - ((Math.log10(Math.max(v, 1)) - bot) / (top - bot)) * (h - 18);
  ctx.strokeStyle = css('--grid'); ctx.lineWidth = 1; ctx.font = '11px sans-serif'; ctx.fillStyle = css('--muted');
  for (let d = 1; d <= 1e4 && Math.log10(d) <= top; d *= 10) { ctx.beginPath(); ctx.moveTo(0, y(d)); ctx.lineTo(w, y(d)); ctx.stroke(); ctx.fillText(d >= 1000 ? d / 1000 + ' g' : d + ' mg', 4, y(d) - 2); }
  const line = (val, col, dash) => { ctx.setLineDash(dash); ctx.strokeStyle = col; ctx.beginPath(); ctx.moveTo(0, y(val)); ctx.lineTo(w, y(val)); ctx.stroke(); ctx.setLineDash([]); };
  line(live.base, css('--muted'), [2, 4]);
  line(live.thr, css('--red'), [6, 4]);
  ctx.strokeStyle = css('--mag'); ctx.lineWidth = 1.5; ctx.beginPath();
  let started = false;
  for (let i = iStart; i <= iEnd; i++) {
    const x = (i - left) / LIVE_SPAN * w;
    if (started) ctx.lineTo(x, y(live.v[i])); else { ctx.moveTo(x, y(live.v[i])); started = true; }
  }
  if (started && iEnd + 1 < live.v.length) {
    ctx.lineTo(w, y(interpAt(live.v, iEnd, frac)));
  }
  ctx.stroke();
}

function drawFancy(cv, head, left, iStart, iEnd, frac, dt) {
  const { ctx, w, h } = prepCanvas(cv);
  const cX = css('--x'), cY = css('--y'), cZ = css('--z'), cMag = css('--mag');
  const cRed = css('--red'), cMuted = css('--muted'), cGrid = css('--grid'), cText = css('--text'), cCyan = css('--cyan'), cPink = css('--pink');

  // instantaneous interpolated values at playout head
  const curMag = interpAt(live.v, iEnd, frac);
  const curDx = interpAt(live.x, iEnd, frac);
  const curDy = interpAt(live.y, iEnd, frac);
  const curDz = interpAt(live.z, iEnd, frac);
  const curWx = interpAt(live.wx, iEnd, frac);
  const curWy = interpAt(live.wy, iEnd, frac);
  const curWz = interpAt(live.wz, iEnd, frac);
  const curRoll = interpAt(live.roll, iEnd, frac);
  const curPitch = interpAt(live.pitch, iEnd, frac);
  const curYaw = interpAt(live.yaw, iEnd, frac);

  live.peakHold = Math.max(curMag, live.peakHold * Math.pow(0.992, dt / 16));

  // smooth gravity vector + 6-DOF orientation (roll, pitch, yaw in deg)
  for (let k = 0; k < 3; k++) {
    live.gSmooth[k] += (live.g[k] - live.gSmooth[k]) * 0.22;
    let dAng = ( [curRoll, curPitch, curYaw][k] ) - live.oriSmooth[k];
    if (dAng > 180) dAng -= 360;
    if (dAng < -180) dAng += 360;
    live.oriSmooth[k] += dAng * 0.35;
    if (live.oriSmooth[k] > 180) live.oriSmooth[k] -= 360;
    if (live.oriSmooth[k] < -180) live.oriSmooth[k] += 360;
  }
  const rollDeg = live.oriSmooth[0];
  const pitchDeg = live.oriSmooth[1];
  const yawDeg = live.oriSmooth[2];
  const rollRad = rollDeg * Math.PI / 180;
  const pitchRad = pitchDeg * Math.PI / 180;
  const yawRad = yawDeg * Math.PI / 180;

  // total tilt angle away from flat horizontal (0° = flat, 90° = vertical wall, 180° = flipped upside down)
  const cosTilt = Math.max(-1, Math.min(1, Math.cos(rollRad) * Math.cos(pitchRad)));
  const tiltTotalDeg = Math.acos(cosTilt) * 180 / Math.PI;

  // update readout bar every ~80 ms or when forced
  if (!live.lastReadout || performance.now() - live.lastReadout > 80) {
    live.lastReadout = performance.now();
    const fmtSigned = (n, d = 0) => (n >= 0 ? '+' : '') + n.toFixed(d);
    $('#msMag').textContent = `${curMag.toFixed(1)} · max ${live.peakHold.toFixed(0)} mg`;
    $('#msXyz').textContent = `${fmtSigned(curDx)} · ${fmtSigned(curDy)} · ${fmtSigned(curDz)}`;
    $('#msGyro').textContent = `${fmtSigned(curWx, 1)} · ${fmtSigned(curWy, 1)} · ${fmtSigned(curWz, 1)}`;
    $('#msTilt').textContent = `${fmtSigned(rollDeg, 1)}° · ${fmtSigned(pitchDeg, 1)}° · ${fmtSigned(yawDeg, 1)}°`;
    $('#msGrav').textContent = `[${live.g[0].toFixed(2)}, ${live.g[1].toFixed(2)}, ${live.g[2].toFixed(2)}]`;
    $('#msTemp').textContent = live.temp != null ? `${live.temp.toFixed(1)} °C` : '–';
  }

  // Split viewport: desktop side-by-side (left 3D stage, right strip charts); mobile stacked
  const wide = w >= 620;
  const stageW = wide ? Math.round(w * 0.44) : w;
  const stageH = wide ? h : Math.round(h * 0.48);
  const chartX = wide ? stageW + 10 : 0;
  const chartY = wide ? 0 : stageH + 8;
  const chartW = wide ? w - chartX : w;
  const chartH = wide ? h : h - chartY;

  // ---------------- 1. 3D SENSOR & TRAP STAGE ----------------
  ctx.save();
  ctx.beginPath();
  ctx.rect(0, 0, stageW, stageH);
  ctx.clip();

  const cx = stageW * 0.5, cy = stageH * 0.55;
  const scale = Math.min(stageW, stageH) * 0.36;
  const camYaw = live.orbit.yaw, camPitch = live.orbit.pitch;
  const cyaw = Math.cos(camYaw), syaw = Math.sin(camYaw);
  const cpitch = Math.cos(camPitch), spitch = Math.sin(camPitch);

  // 3D world (X right, Y forward, Z up) -> screen projection
  const proj = (x, y, z) => {
    const x1 = x * cyaw - y * syaw;
    const y1 = x * syaw + y * cyaw;
    const z2 = y1 * spitch + z * cpitch;
    const y2 = y1 * cpitch - z * spitch;
    const fov = 3.2;
    const persp = fov / (fov + y2 * 0.45);
    return { x: cx + x1 * scale * persp, y: cy - z2 * scale * persp, z: y2, p: persp };
  };

  // True 1:1 3D Orientation rotation (Roll around X, Pitch around Y, Yaw around Z)
  const cr = Math.cos(rollRad), sr = Math.sin(rollRad);
  const cp = Math.cos(pitchRad), sp = Math.sin(pitchRad);
  const cz = Math.cos(yawRad), sz = Math.sin(yawRad);
  const bodyToWorld = (bx, by, bz) => {
    // 1. Roll around X
    const y1 = by * cr - bz * sr;
    const z1 = by * sr + bz * cr;
    // 2. Pitch around Y
    const x2 = bx * cp + z1 * sp;
    const z2 = -bx * sp + z1 * cp;
    // 3. Yaw around Z
    const x3 = x2 * cz - y1 * sz;
    const y3 = x2 * sz + y1 * cz;
    return [x3, y3, z2];
  };
  const projBody = (bx, by, bz) => {
    const [wx, wy, wz] = bodyToWorld(bx, by, bz);
    return proj(wx, wy, wz);
  };

  // Floor radar + compass rings on z = -0.35 (cached / lightened)
  const floorZ = -0.35;
  ctx.strokeStyle = cGrid;
  ctx.lineWidth = 1;
  ctx.beginPath();
  [0.55, 1.15].forEach(rad => {
    for (let a = 0; a <= 32; a++) {
      const ang = (a / 32) * Math.PI * 2;
      const p = proj(Math.cos(ang) * rad, Math.sin(ang) * rad, floorZ);
      a ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y);
    }
  });
  // floor crosshairs
  [[-1.15, 0, 1.15, 0], [0, -1.15, 0, 1.15]].forEach(([x0, y0, x1, y1]) => {
    const p0 = proj(x0, y0, floorZ), p1 = proj(x1, y1, floorZ);
    ctx.moveTo(p0.x, p0.y); ctx.lineTo(p1.x, p1.y);
  });
  ctx.stroke();

  // Yaw heading pointer on floor
  const yawFloorTip = proj(Math.cos(yawRad) * 1.15, Math.sin(yawRad) * 1.15, floorZ);
  const yawFloorCtr = proj(0, 0, floorZ);
  ctx.strokeStyle = cZ;
  ctx.lineWidth = 1.4;
  ctx.beginPath();
  ctx.moveTo(yawFloorCtr.x, yawFloorCtr.y);
  ctx.lineTo(yawFloorTip.x, yawFloorTip.y);
  ctx.stroke();

  // Shockwave rings on floor during impact
  if (curMag > live.thr && live.thr > 0 && (!live.rings.length || live.rings[live.rings.length - 1].r > 0.3)) {
    live.rings.push({ r: 0.12, alpha: Math.min(1, curMag / Math.max(live.thr, 10)) });
  }
  for (let i = live.rings.length - 1; i >= 0; i--) {
    const rg = live.rings[i];
    rg.r += (dt / 1000) * 1.35;
    rg.alpha *= Math.pow(0.92, dt / 16);
    if (rg.alpha < 0.04 || rg.r > 1.45) { live.rings.splice(i, 1); continue; }
    ctx.save();
    ctx.strokeStyle = cRed;
    ctx.globalAlpha = rg.alpha;
    ctx.lineWidth = 2;
    ctx.beginPath();
    for (let a = 0; a <= 24; a++) {
      const ang = (a / 24) * Math.PI * 2;
      const p = proj(Math.cos(ang) * rg.r, Math.sin(ang) * rg.r, floorZ);
      a ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y);
    }
    ctx.stroke();
    ctx.restore();
  }

  // 3D Mousetrap wooden base board: [-0.82..0.82] x [-0.40..0.40] x [-0.14..-0.02]
  const bx = 0.82, by = 0.40, bz0 = -0.14, bz1 = -0.02;
  const corners = [
    projBody(-bx, -by, bz0), projBody(bx, -by, bz0), projBody(bx, by, bz0), projBody(-bx, by, bz0),
    projBody(-bx, -by, bz1), projBody(bx, -by, bz1), projBody(bx, by, bz1), projBody(-bx, by, bz1),
  ];
  // shaded top deck of the mousetrap
  ctx.save();
  ctx.fillStyle = cPink;
  ctx.globalAlpha = 0.12;
  ctx.beginPath();
  ctx.moveTo(corners[4].x, corners[4].y);
  ctx.lineTo(corners[5].x, corners[5].y);
  ctx.lineTo(corners[6].x, corners[6].y);
  ctx.lineTo(corners[7].x, corners[7].y);
  ctx.closePath();
  ctx.fill();
  ctx.restore();

  // wireframe edges of the trap base
  ctx.strokeStyle = cPink;
  ctx.lineWidth = 1.3;
  ctx.beginPath();
  [[0,1],[1,2],[2,3],[3,0],[4,5],[5,6],[6,7],[7,4],[0,4],[1,5],[2,6],[3,7]].forEach(([a, b]) => {
    ctx.moveTo(corners[a].x, corners[a].y); ctx.lineTo(corners[b].x, corners[b].y);
  });
  ctx.stroke();

  // bait pedal
  const pedal = [
    projBody(-0.65, -0.20, bz1 + 0.01), projBody(-0.30, -0.20, bz1 + 0.04),
    projBody(-0.30, 0.20, bz1 + 0.04), projBody(-0.65, 0.20, bz1 + 0.01),
  ];
  ctx.strokeStyle = cZ;
  ctx.lineWidth = 1.4;
  ctx.beginPath();
  pedal.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y));
  ctx.closePath();
  ctx.stroke();

  // helical spring coil along Y at x = 0.0 (18 segments)
  ctx.strokeStyle = cZ;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  for (let i = 0; i <= 18; i++) {
    const u = i / 18;
    const sy = -0.32 + u * 0.64;
    const ang = u * Math.PI * 8;
    const p = projBody(Math.cos(ang) * 0.06, sy, bz1 + 0.05 + Math.sin(ang) * 0.06);
    i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y);
  }
  ctx.stroke();

  // steel kill bar (bow) — dynamic snap physics & trigger slam
  // barSnap: 0.0 = armed / open (+X side), 1.0 = snapped shut (-X bait side)
  if (Math.abs(live.barSnap - live.barSnapTarget) > 0.001) {
    const spd = live.barSnapTarget > live.barSnap ? 0.35 : 0.08; // slams down fast, resets gently
    live.barSnap += (live.barSnapTarget - live.barSnap) * spd;
    live.dirty = true;
  }
  const isSnapped = live.barSnap > 0.6;
  const barVib = Math.min(0.4, curMag / Math.max(live.thr * 2, 200) * 0.25) * Math.sin(performance.now() * 0.05);
  // Armed open angle ~0.20 rad (cocked back), snapped closed angle ~2.94 rad (slammed flat over bait)
  const barAng = 0.20 + live.barSnap * 2.74 + (isSnapped ? -Math.abs(barVib * 0.2) : barVib);
  const barLen = 0.70;
  const barX = Math.cos(barAng) * barLen, barZ = bz1 + 0.04 + Math.sin(barAng) * barLen;
  const bow = [
    projBody(0, -0.34, bz1 + 0.04),
    projBody(barX, -0.34, barZ),
    projBody(barX, 0.34, barZ),
    projBody(0, 0.34, bz1 + 0.04),
  ];
  ctx.strokeStyle = isSnapped ? cRed : (curMag > live.thr ? cPink : cCyan);
  ctx.lineWidth = isSnapped ? 2.8 : 2.2;
  ctx.beginPath();
  bow.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y));
  ctx.stroke();

  // MPU-6500 chip footprint
  const chip = [
    projBody(0.28, -0.15, bz1 + 0.02), projBody(0.56, -0.15, bz1 + 0.02),
    projBody(0.56, 0.15, bz1 + 0.02), projBody(0.28, 0.15, bz1 + 0.02),
  ];
  ctx.save();
  ctx.fillStyle = cMag;
  ctx.globalAlpha = 0.20;
  ctx.beginPath();
  chip.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y));
  ctx.closePath();
  ctx.fill();
  ctx.restore();
  ctx.strokeStyle = cMag;
  ctx.lineWidth = 1.4;
  ctx.beginPath();
  chip.forEach((p, i) => i ? ctx.lineTo(p.x, p.y) : ctx.moveTo(p.x, p.y));
  ctx.closePath();
  ctx.stroke();

  // 3D Coordinate Axes Triad (X, Y, Z)
  const origin = projBody(0, 0, 0.15);
  const drawAxis = (bx, by, bz, col, lbl) => {
    const tip = projBody(bx, by, bz + 0.15);
    ctx.strokeStyle = col;
    ctx.lineWidth = 1.8;
    ctx.beginPath();
    ctx.moveTo(origin.x, origin.y);
    ctx.lineTo(tip.x, tip.y);
    ctx.stroke();
    ctx.fillStyle = col;
    ctx.beginPath();
    ctx.arc(tip.x, tip.y, 2.8, 0, Math.PI * 2);
    ctx.fill();
    ctx.font = 'bold 11px monospace';
    ctx.fillText(lbl, tip.x + 5, tip.y + 3);
  };
  drawAxis(0.65, 0, 0, cX, 'X');
  drawAxis(0, 0.65, 0, cY, 'Y');
  drawAxis(0, 0, 0.65, cZ, 'Z');

  // 3D Phase-Space Vibration Ribbon (reduced to last 35 points in single batched path)
  const vScale = Math.max(live.thr * 1.6, live.xyzRange, 35);
  const trailLen = Math.min(35, iEnd - iStart);
  if (trailLen > 2) {
    ctx.save();
    ctx.strokeStyle = curMag > live.thr ? cRed : cMag;
    ctx.lineWidth = 1.5;
    ctx.globalAlpha = 0.7;
    ctx.beginPath();
    for (let k = trailLen; k >= 0; k--) {
      const idx = iEnd - k;
      const p = projBody((live.x[idx] || 0) / vScale * 0.75, (live.y[idx] || 0) / vScale * 0.75, 0.15 + (live.z[idx] || 0) / vScale * 0.75);
      k === trailLen ? ctx.moveTo(p.x, p.y) : ctx.lineTo(p.x, p.y);
    }
    ctx.stroke();
    ctx.restore();
  }

  // Instantaneous dynamic vector arrow
  const curTip = projBody(curDx / vScale * 0.75, curDy / vScale * 0.75, 0.15 + curDz / vScale * 0.75);
  ctx.strokeStyle = curMag > live.thr ? cRed : cMag;
  ctx.lineWidth = 2.2;
  ctx.beginPath();
  ctx.moveTo(origin.x, origin.y);
  ctx.lineTo(curTip.x, curTip.y);
  ctx.stroke();
  ctx.fillStyle = curMag > live.thr ? cRed : cMag;
  ctx.beginPath();
  ctx.arc(curTip.x, curTip.y, 4, 0, Math.PI * 2);
  ctx.fill();

  // Artificial Horizon HUD Gauge
  const ahR = 26, ahX = stageW - ahR - 10, ahY = ahR + 10;
  ctx.save();
  ctx.beginPath();
  ctx.arc(ahX, ahY, ahR, 0, Math.PI * 2);
  ctx.clip();
  ctx.fillStyle = 'rgba(0,0,0,0.35)';
  ctx.fillRect(ahX - ahR, ahY - ahR, ahR * 2, ahR * 2);
  ctx.translate(ahX, ahY);
  ctx.rotate(rollRad);
  const pitchPx = Math.max(-ahR, Math.min(ahR, (pitchDeg / 60) * ahR));
  ctx.fillStyle = cPink;
  ctx.globalAlpha = 0.22;
  ctx.fillRect(-ahR * 1.5, pitchPx, ahR * 3, ahR * 2);
  ctx.globalAlpha = 0.9;
  ctx.strokeStyle = cCyan;
  ctx.lineWidth = 1.6;
  ctx.beginPath();
  ctx.moveTo(-ahR, pitchPx);
  ctx.lineTo(ahR, pitchPx);
  ctx.stroke();
  ctx.restore();

  ctx.strokeStyle = tiltTotalDeg > 45 ? cRed : cCyan;
  ctx.lineWidth = 1.5;
  ctx.beginPath();
  ctx.arc(ahX, ahY, ahR, 0, Math.PI * 2);
  ctx.stroke();
  ctx.strokeStyle = cZ;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(ahX - 10, ahY); ctx.lineTo(ahX - 3, ahY);
  ctx.moveTo(ahX + 3, ahY); ctx.lineTo(ahX + 10, ahY);
  ctx.stroke();

  // HUD text labels
  const oriState = isSnapped ? '💥 TRAP SNAPPED!' : (tiltTotalDeg > 120 ? '🙃 INVERTED' : tiltTotalDeg > 35 ? '📐 STEEP TILT' : tiltTotalDeg > 8 ? '🧭 TILTED' : '🟢 LEVEL');
  ctx.fillStyle = cMuted;
  ctx.font = '11px monospace';
  ctx.fillText(`🧊 3D ORIENTATION`, 8, 15);
  ctx.fillStyle = isSnapped ? cRed : (tiltTotalDeg > 35 ? cRed : cMag);
  ctx.fillText(`${oriState} ${isSnapped ? '🐭' : `(∠${tiltTotalDeg.toFixed(1)}°)`}`, 8, 30);
  ctx.fillStyle = cText;
  const fS = v => (v >= 0 ? '+' : '') + v.toFixed(1) + '°';
  ctx.fillText(`roll ${fS(rollDeg)}  pitch ${fS(pitchDeg)}  yaw ${fS(yawDeg)}`, 8, stageH - 8);
  ctx.restore();

  // Divider between 3D stage and strip charts
  ctx.strokeStyle = cGrid;
  ctx.lineWidth = 1;
  ctx.beginPath();
  if (wide) { ctx.moveTo(stageW + 4, 4); ctx.lineTo(stageW + 4, h - 4); }
  else { ctx.moveTo(4, stageH + 3); ctx.lineTo(w - 4, stageH + 3); }
  ctx.stroke();

  // ---------------- 2. FOUR SYNCHRONIZED MULTI-SENSOR STRIP CHARTS ----------------
  const gap = 7;
  const stripH = (chartH - gap * 3) / 4;
  const xOf = idx => chartX + ((idx - left) / LIVE_SPAN) * chartW;

  // --- Strip A: 3-Axis Dynamic Acceleration (Δx, Δy, Δz in mg) ---
  const yA0 = chartY;
  const midA = yA0 + stripH * 0.5;
  let peakXyz = Math.max(live.thr * 1.25, 25);
  for (let i = iStart; i <= iEnd; i++) {
    const m = Math.max(Math.abs(live.x[i] || 0), Math.abs(live.y[i] || 0), Math.abs(live.z[i] || 0));
    if (m > peakXyz) peakXyz = m;
  }
  live.xyzRange = peakXyz >= live.xyzRange ? peakXyz : live.xyzRange + (peakXyz - live.xyzRange) * 0.12;
  const scA = (stripH * 0.44) / Math.max(live.xyzRange, 10);

  ctx.strokeStyle = cGrid;
  ctx.lineWidth = 1;
  ctx.strokeRect(chartX, yA0, chartW, stripH);
  ctx.beginPath(); ctx.moveTo(chartX, midA); ctx.lineTo(chartX + chartW, midA); ctx.stroke();

  // +-threshold lines if within range
  if (live.thr > 0 && live.thr < live.xyzRange * 1.05) {
    ctx.save();
    ctx.strokeStyle = cRed;
    ctx.setLineDash([4, 4]);
    ctx.globalAlpha = 0.65;
    [midA - live.thr * scA, midA + live.thr * scA].forEach(yy => {
      ctx.beginPath(); ctx.moveTo(chartX, yy); ctx.lineTo(chartX + chartW, yy); ctx.stroke();
    });
    ctx.restore();
  }

  const drawBipolarTrace = (arr, col, midY, sc, topY, botY) => {
    ctx.strokeStyle = col;
    ctx.lineWidth = 1.25;
    ctx.beginPath();
    let st = false;
    for (let i = iStart; i <= iEnd; i++) {
      const px = xOf(i);
      const py = Math.max(topY + 2, Math.min(botY - 2, midY - (arr[i] || 0) * sc));
      if (st) ctx.lineTo(px, py); else { ctx.moveTo(px, py); st = true; }
    }
    if (st && iEnd + 1 < arr.length) {
      const py = Math.max(topY + 2, Math.min(botY - 2, midY - interpAt(arr, iEnd, frac) * sc));
      ctx.lineTo(chartX + chartW, py);
    }
    ctx.stroke();
  };
  drawBipolarTrace(live.x, cX, midA, scA, yA0, yA0 + stripH);
  drawBipolarTrace(live.y, cY, midA, scA, yA0, yA0 + stripH);
  drawBipolarTrace(live.z, cZ, midA, scA, yA0, yA0 + stripH);

  ctx.fillStyle = cMuted;
  ctx.font = '11px monospace';
  ctx.fillText(`3-AXIS ACCEL Δx/Δy/Δz (±${Math.round(live.xyzRange)} mg)`, chartX + 6, yA0 + 13);

  // --- Strip B: Log Magnitude |a| (mg) with Neon Fill ---
  const yB0 = chartY + stripH + gap;
  let peakMag = Math.max(live.thr * 3, 100);
  for (let i = iStart; i <= iEnd; i++) if (live.v[i] > peakMag) peakMag = live.v[i];
  const targetTop = Math.log10(peakMag);
  live.top = targetTop >= live.top ? targetTop : live.top + (targetTop - live.top) * 0.15;
  const topB = live.top, botB = 0;
  const yLog = v => yB0 + stripH - 4 - ((Math.log10(Math.max(v, 1)) - botB) / (topB - botB)) * (stripH - 18);

  ctx.strokeStyle = cGrid;
  ctx.lineWidth = 1;
  ctx.strokeRect(chartX, yB0, chartW, stripH);
  ctx.fillStyle = cMuted;
  ctx.font = '10px monospace';
  for (let d = 10; d <= 1e4 && Math.log10(d) <= topB; d *= 10) {
    const yy = yLog(d);
    ctx.beginPath(); ctx.moveTo(chartX, yy); ctx.lineTo(chartX + chartW, yy); ctx.stroke();
    ctx.fillText(d >= 1000 ? d / 1000 + 'g' : d + 'mg', chartX + chartW - 34, yy - 2);
  }
  // baseline & threshold
  const hLine = (val, col, dash) => {
    const yy = yLog(val);
    if (yy < yB0 || yy > yB0 + stripH) return;
    ctx.setLineDash(dash); ctx.strokeStyle = col;
    ctx.beginPath(); ctx.moveTo(chartX, yy); ctx.lineTo(chartX + chartW, yy); ctx.stroke();
    ctx.setLineDash([]);
  };
  hLine(live.base, cMuted, [2, 4]);
  hLine(live.thr, cRed, [6, 4]);

  // magnitude path + translucent fill
  ctx.beginPath();
  let firstX = chartX;
  for (let i = iStart; i <= iEnd; i++) {
    const px = xOf(i), py = yLog(live.v[i]);
    if (i === iStart) { firstX = px; ctx.moveTo(px, py); } else ctx.lineTo(px, py);
  }
  if (iEnd >= iStart) {
    const lastY = yLog(curMag);
    ctx.lineTo(chartX + chartW, lastY);
    ctx.strokeStyle = cMag;
    ctx.lineWidth = 1.6;
    ctx.stroke();
    ctx.lineTo(chartX + chartW, yB0 + stripH - 2);
    ctx.lineTo(firstX, yB0 + stripH - 2);
    ctx.closePath();
    ctx.save();
    ctx.fillStyle = cMag;
    ctx.globalAlpha = 0.14;
    ctx.fill();
    ctx.restore();
  }
  ctx.fillStyle = cMuted;
  ctx.font = '11px monospace';
  ctx.fillText(`LOG MAGNITUDE |a| (${curMag.toFixed(1)} mg · thr ${live.thr.toFixed(0)} mg)`, chartX + 6, yB0 + 13);

  // --- Strip C: 3-Axis Gyroscope Angular Velocity (ωx, ωy, ωz in °/s) ---
  const yC0 = chartY + (stripH + gap) * 2;
  const midC = yC0 + stripH * 0.5;
  let peakGyro = 5;
  for (let i = iStart; i <= iEnd; i++) {
    const m = Math.max(Math.abs(live.wx[i] || 0), Math.abs(live.wy[i] || 0), Math.abs(live.wz[i] || 0));
    if (m > peakGyro) peakGyro = m;
  }
  live.gyroRange = peakGyro >= live.gyroRange ? peakGyro : live.gyroRange + (peakGyro - live.gyroRange) * 0.12;
  const scC = (stripH * 0.44) / Math.max(live.gyroRange, 2);

  ctx.strokeStyle = cGrid;
  ctx.lineWidth = 1;
  ctx.strokeRect(chartX, yC0, chartW, stripH);
  ctx.beginPath(); ctx.moveTo(chartX, midC); ctx.lineTo(chartX + chartW, midC); ctx.stroke();

  drawBipolarTrace(live.wx, cX, midC, scC, yC0, yC0 + stripH);
  drawBipolarTrace(live.wy, cY, midC, scC, yC0, yC0 + stripH);
  drawBipolarTrace(live.wz, cZ, midC, scC, yC0, yC0 + stripH);

  ctx.fillStyle = cMuted;
  ctx.font = '11px monospace';
  ctx.fillText(`3-AXIS GYRO ωx/ωy/ωz (±${live.gyroRange.toFixed(1)} °/s)`, chartX + 6, yC0 + 13);

  // --- Strip D: 3D Orientation Angles (Roll, Pitch, Yaw in °) ---
  const yD0 = chartY + (stripH + gap) * 3;
  const midD = yD0 + stripH * 0.5;
  let peakOri = 15;
  for (let i = iStart; i <= iEnd; i++) {
    const m = Math.max(Math.abs(live.roll[i] || 0), Math.abs(live.pitch[i] || 0), Math.abs(live.yaw[i] || 0));
    if (m > peakOri) peakOri = m;
  }
  live.oriRange = peakOri >= live.oriRange ? peakOri : live.oriRange + (peakOri - live.oriRange) * 0.12;
  const scD = (stripH * 0.44) / Math.max(live.oriRange, 5);

  ctx.strokeStyle = cGrid;
  ctx.lineWidth = 1;
  ctx.strokeRect(chartX, yD0, chartW, stripH);
  ctx.beginPath(); ctx.moveTo(chartX, midD); ctx.lineTo(chartX + chartW, midD); ctx.stroke();

  drawBipolarTrace(live.roll, cX, midD, scD, yD0, yD0 + stripH);
  drawBipolarTrace(live.pitch, cY, midD, scD, yD0, yD0 + stripH);
  drawBipolarTrace(live.yaw, cZ, midD, scD, yD0, yD0 + stripH);

  ctx.fillStyle = cMuted;
  ctx.font = '11px monospace';
  ctx.fillText(`ORIENTATION R ${fS(rollDeg)} · P ${fS(pitchDeg)} · Y ${fS(yawDeg)} (±${Math.round(live.oriRange)}°)`, chartX + 6, yD0 + 13);
}
(function liveLoop() {
  if (!document.hidden) drawLive(false);
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
  if (!wsOpen) {
    live.thr = s.sensor.threshold_g * 1000;
    live.base = s.sensor.baseline_g * 1000;
    if (s.sensor.gravity) live.g = s.sensor.gravity;
    if (s.sensor.orientation) live.ori = s.sensor.orientation;
    if (s.sensor.temp_c != null) live.temp = s.sensor.temp_c;
  }
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
    const n = m.v.length;
    if (!live.init && n) {
      live.v.fill(m.base || m.v[0]);
      if (m.g) { live.g = m.g.slice(); live.gSmooth = m.g.slice(); }
      if (m.ori) {
        live.ori = m.ori.slice();
        live.oriSmooth = m.ori.slice();
        live.roll.fill(m.ori[0]);
        live.pitch.fill(m.ori[1]);
        live.yaw.fill(m.ori[2]);
      }
      live.lag = LIVE_CUSHION;
      live.init = true;
    }
    live.v.push(...m.v);
    const pushArr = (dst, src, fallback) => {
      if (src && src.length === n) dst.push(...src);
      else for (let i = 0; i < n; i++) dst.push(fallback);
      if (dst.length > LIVE_N) dst.splice(0, dst.length - LIVE_N);
    };
    pushArr(live.x, m.x, 0);
    pushArr(live.y, m.y, 0);
    pushArr(live.z, m.z, 0);
    const w0 = m.w ? m.w[0] : 0, w1 = m.w ? m.w[1] : 0, w2 = m.w ? m.w[2] : 0;
    pushArr(live.wx, null, w0);
    pushArr(live.wy, null, w1);
    pushArr(live.wz, null, w2);
    // orientation angles (deg): use firmware 6-DOF complementary filter if present, else gravity fallback
    let rDeg = 0, pDeg = 0, yDeg = 0;
    if (m.ori) {
      [rDeg, pDeg, yDeg] = m.ori;
      live.ori = m.ori;
    } else if (m.g) {
      const gn = Math.hypot(m.g[0], m.g[1], m.g[2]) || 1;
      rDeg = Math.atan2(m.g[1] / gn, m.g[2] / gn) * 180 / Math.PI;
      pDeg = Math.atan2(-m.g[0] / gn, Math.hypot(m.g[1] / gn, m.g[2] / gn)) * 180 / Math.PI;
      yDeg = (live.yaw[live.yaw.length - 1] || 0) + w2 * 0.02 * n;
    }
    pushArr(live.roll, null, rDeg);
    pushArr(live.pitch, null, pDeg);
    pushArr(live.yaw, null, yDeg);
    if (live.v.length > LIVE_N) live.v.splice(0, live.v.length - LIVE_N);
    if (m.g) live.g = m.g;
    if (m.w) live.w = m.w;
    if (m.temp != null) live.temp = m.temp;
    live.lag = Math.min(live.lag + n, LIVE_N - LIVE_SPAN - 2);
    live.thr = m.thr; live.base = m.base; live.dirty = true;
  } else if (m.t === 'event') {
    recent = [m.e, ...recent.filter(x => x.id !== m.e.id)].slice(0, 6); renderEvents();
    if (m.e.candidate) showCandidate(m.e);
    // When a snap event arrives, slam the 3D kill bar shut!
    const isSnap = m.e.cls === 'snap' || (m.e.snap_p && m.e.snap_p >= (cfg ? cfg.alert_p : 0.7));
    if (isSnap) {
      live.barSnapTarget = 1.0;
      live.rings.push({ r: 0.1, alpha: 1.0 });
      live.dirty = true;
    }
    loadStatus();
  } else if (m.t === 'lab') {
    updateLab(m.armed_label, m.remaining_ms);
    // When lab test is armed, cock the 3D bar back open
    if (m.armed_label && m.remaining_ms > 0) {
      live.barSnapTarget = 0.0;
      live.dirty = true;
    }
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
  $('#btnViewClassic').onclick = () => setLiveView('classic');
  $('#btnViewFancy').onclick = () => setLiveView('fancy');
  setLiveView(live.view);

  const cvF = $('#cvFancy');
  cvF.addEventListener('pointerdown', ev => {
    live.orbit.dragging = true;
    live.orbit.lastX = ev.clientX;
    live.orbit.lastY = ev.clientY;
    cvF.setPointerCapture(ev.pointerId);
  });
  cvF.addEventListener('pointermove', ev => {
    if (!live.orbit.dragging) return;
    const dx = ev.clientX - live.orbit.lastX, dy = ev.clientY - live.orbit.lastY;
    live.orbit.lastX = ev.clientX;
    live.orbit.lastY = ev.clientY;
    live.orbit.yaw += dx * 0.01;
    live.orbit.pitch = Math.max(-1.2, Math.min(1.3, live.orbit.pitch + dy * 0.01));
    live.dirty = true;
  });
  const endDrag = () => { live.orbit.dragging = false; };
  cvF.addEventListener('pointerup', endDrag);
  cvF.addEventListener('pointercancel', endDrag);
  cvF.addEventListener('dblclick', () => {
    live.orbit.yaw = -0.62;
    live.orbit.pitch = 0.42;
    // Double click also toggles/re-arms the 3D trap bar for testing
    live.barSnapTarget = live.barSnapTarget > 0.5 ? 0.0 : 1.0;
    live.dirty = true;
  });
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
  $('#btnPurgeUnlabelled').onclick = async () => {
    if (!confirm(t('confirmPurge', 'Really delete all unlabelled recordings? Labelled training samples will remain intact.'))) return;
    try {
      const res = await api.post('/api/samples/purge-unlabelled');
      toast(`🧹 ${res.purged ?? 0} unlabelled deleted`);
      loadDataset();
      loadStatus();
    } catch (e) {
      toast('❌ ' + e.message);
    }
  };
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
  setInterval(() => { if (!document.hidden) loadStatus(); }, 10000);
}
init();
