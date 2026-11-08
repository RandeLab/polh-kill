/* polh-kill GitHub Pages dashboard: reads the data the ESP pushes to data/ */
'use strict';

const $ = s => document.querySelector(s);
const el = (tag, attrs = {}, html = '') => { const e = document.createElement(tag); Object.assign(e, attrs); if (html) e.innerHTML = html; return e; };
const pct = p => (p < 0 ? '–' : Math.round(p * 100) + ' %');
const css = v => getComputedStyle(document.body).getPropertyValue(v).trim();
const esc = s => String(s).replace(/[&<>"]/g, c => ({ '&': '&amp;', '<': '&lt;', '>': '&gt;', '"': '&quot;' }[c]));
const bust = () => '?t=' + Math.floor(Date.now() / 60000);  // Pages caches ~10 min
const PAGE = 50;

let idx = null, emoji = {}, rows = [], page = 0, selected = null;

async function getJson(path) {
  const r = await fetch(path + bust());
  if (!r.ok) throw new Error(path + ' ' + r.status);
  return r.json();
}

function ago(ts) {
  if (!ts) return '';
  const s = Date.now() / 1000 - ts;
  if (s < 90) return 'just now';
  if (s < 5400) return Math.round(s / 60) + ' min ago';
  if (s < 129600) return Math.round(s / 3600) + ' h ago';
  return Math.round(s / 86400) + ' days ago';
}
const when = ts => (ts ? new Date(ts * 1000).toLocaleString() : '–');

function stat(v, l) { return el('div', { className: 'panel stat' }, `<div class="v">${esc(v)}</div><div class="l">${esc(l)}</div>`); }

async function loadStatus() {
  try {
    const s = await getJson('data/status.json');
    const box = $('#stats');
    box.innerHTML = '';
    box.append(stat(s.captures, 'captures'), stat(s.labelled, 'labelled'), stat(s.snaps_total, 'snaps total'),
      stat(s.noise_floor_mg.toFixed(1), 'noise floor (mg)'), stat(s.sensor, 'sensor'), stat(s.armed ? '🎯 armed' : '😴 disarmed', 'trap'));
    const fresh = ago(s.generated);
    $('#pillFresh').textContent = '🐙 updated ' + (fresh || '?');
    $('#pillFresh').className = 'pill ' + (s.generated && Date.now() / 1000 - s.generated < 7200 ? 'ok' : 'warn');
  } catch (e) {
    $('#stats').innerHTML = '';
    $('#stats').append(el('div', { className: 'panel' }, 'Ze trap has not published anything yet. It is waiting. Like all of us.'));
    $('#pillFresh').textContent = 'no data yet';
  }
}

async function loadIndex() {
  try { idx = await getJson('data/index.json'); } catch (e) { $('#capInfo').textContent = '– nothing yet'; return; }
  emoji = Object.fromEntries(idx.classes.map(c => [c.name, c.emoji]));
  const opts = (sel, label) => {
    sel.innerHTML = '';
    sel.append(el('option', { value: '*', textContent: label }));
    if (sel.id === 'fLabel') sel.append(el('option', { value: '', textContent: '— unlabelled' }));
    idx.classes.forEach(c => sel.append(el('option', { value: c.name, textContent: `${c.emoji} ${c.name}` })));
  };
  opts($('#fLabel'), 'any label');
  opts($('#fPred'), 'any prediction');
  render();
  const id = Number(location.hash.slice(1));
  if (id) showCapture(id);
}

function filtered() {
  const fl = $('#fLabel').value, fp = $('#fPred').value, fw = $('#fWave').checked;
  // columns: id, ts, label, pred, snap_p, peak_g, waveform
  return idx.captures.filter(c => (fl === '*' || c[2] === fl) && (fp === '*' || c[3] === fp) && (!fw || c[6])).reverse();
}

function render() {
  if (!idx) return;
  rows = filtered();
  const pages = Math.max(1, Math.ceil(rows.length / PAGE));
  page = Math.min(page, pages - 1);
  $('#capInfo').textContent = `${idx.captures.length} total · ${rows.length} shown`;
  $('#pInfo').textContent = `${page + 1} / ${pages}`;
  const tb = $('#capTable');
  tb.innerHTML = '<tr><th>#</th><th>when</th><th>label</th><th>model said</th><th class="num">snap</th><th class="num">peak (g)</th><th></th></tr>';
  rows.slice(page * PAGE, page * PAGE + PAGE).forEach(c => {
    const tr = el('tr', { className: 'cap' + (c[0] === selected ? ' sel' : '') });
    tr.innerHTML = `<td class="mono">${c[0]}</td><td>${esc(when(c[1]))}</td>` +
      `<td>${c[2] ? `<span class="tag lab">${emoji[c[2]] || ''} ${esc(c[2])}</span>` : '<span class="muted">–</span>'}</td>` +
      `<td>${emoji[c[3]] || ''} ${esc(c[3])}</td><td class="num">${pct(c[4])}</td><td class="num">${c[5].toFixed(2)}</td>` +
      `<td>${c[6] ? '📈' : ''}</td>`;
    if (c[6]) tr.onclick = () => showCapture(c[0]);
    tb.append(tr);
  });
}

async function showCapture(id) {
  selected = id;
  history.replaceState(null, '', '#' + id);
  render();
  let c;
  try { c = await (await fetch(`data/captures/${id}.json`)).json(); } catch (e) { return; }
  $('#detail').classList.remove('hide');
  $('#detTitle').innerHTML = `#${c.id} · ${esc(when(c.ts))} · ${c.label ? `labelled <b>${emoji[c.label] || ''} ${esc(c.label)}</b> · ` : ''}model said ${emoji[c.pred] || ''} ${esc(c.pred)} (snap ${pct(c.snap_p)}, ${esc(c.method)})`;
  drawWave(c);
  $('#detFeat').innerHTML = Object.entries(c.features).map(([k, v]) => `<span><em>${esc(k)}</em><b class="mono">${(+v).toFixed(3)}</b></span>`).join('');
  $('#detail').scrollIntoView({ behavior: 'smooth', block: 'nearest' });
}

function drawWave(w) {
  const cv = $('#cvWave'), dpr = window.devicePixelRatio || 1, W = cv.clientWidth, H = +cv.getAttribute('height');
  cv.width = W * dpr; cv.height = H * dpr; cv.style.height = H + 'px';
  const ctx = cv.getContext('2d');
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  const n = w.x.length, mag = w.x.map((v, i) => Math.hypot(v, w.y[i], w.z[i]));
  const max = Math.max(1, ...mag), mid = H / 2, sc = (H / 2 - 8) / max, X = i => i / (n - 1) * W;
  ctx.strokeStyle = css('--grid'); ctx.beginPath(); ctx.moveTo(0, mid); ctx.lineTo(W, mid); ctx.stroke();
  ctx.setLineDash([4, 4]); ctx.strokeStyle = css('--muted'); ctx.beginPath(); ctx.moveTo(X(w.trig), 0); ctx.lineTo(X(w.trig), H); ctx.stroke();
  const thr = w.threshold_g * 1000;
  ctx.strokeStyle = css('--red'); ctx.setLineDash([6, 4]); ctx.beginPath(); ctx.moveTo(0, mid - thr * sc); ctx.lineTo(W, mid - thr * sc); ctx.stroke(); ctx.setLineDash([]);
  const trace = (a, col, lw) => { ctx.strokeStyle = col; ctx.lineWidth = lw; ctx.beginPath(); a.forEach((v, i) => (i ? ctx.lineTo(X(i), mid - v * sc) : ctx.moveTo(X(i), mid - v * sc))); ctx.stroke(); };
  trace(w.x, css('--x'), 1); trace(w.y, css('--y'), 1); trace(w.z, css('--z'), 1); trace(mag, css('--mag'), 1.5);
  ctx.fillStyle = css('--muted'); ctx.font = '11px sans-serif';
  ctx.fillText(`±${(max / 1000).toFixed(2)} g · ${(n / w.rate * 1000).toFixed(0)} ms @ ${w.rate} Hz`, 4, 12);
}

async function loadModel() {
  let m;
  try { m = await getJson('data/model.json'); } catch (e) { return; }
  const tb = $('#classTable');
  tb.innerHTML = '<tr><th>class</th><th class="num">samples</th><th style="width:40%"></th><th class="num">typical peak (g)</th></tr>';
  const maxN = Math.max(10, ...m.classes.map(c => c.n));
  m.classes.forEach(c => tb.append(el('tr', {}, `<td>${c.emoji} ${esc(c.name)}</td><td class="num">${c.n}</td><td><div class="bar"><i style="width:${c.n / maxN * 100}%"></i></div></td><td class="num">${c.n ? Math.pow(10, c.mean[0]).toFixed(2) : '–'}</td>`)));
  const l = m.loo, names = m.classes.map(c => c.name);
  const prec = l.snap_tp + l.snap_fp ? l.snap_tp / (l.snap_tp + l.snap_fp) : 0, rec = l.snap_tp + l.snap_fn ? l.snap_tp / (l.snap_tp + l.snap_fn) : 0;
  $('#looSummary').textContent = l.total ? `accuracy ${pct(l.correct / l.total)} · snap recall ${pct(rec)} · snap precision ${pct(prec)} · ${l.total} labelled samples` : 'no labelled samples yet';
  const cm = $('#cmTable');
  cm.innerHTML = '<tr><th>true ↓ / predicted →</th>' + m.classes.map(c => `<th>${c.emoji}</th>`).join('') + '</tr>';
  l.confusion.forEach((row, i) => { if (i < names.length) cm.append(el('tr', {}, `<th>${m.classes[i].emoji} ${esc(names[i])}</th>` + row.slice(0, names.length).map((v, j) => `<td class="${i === j && v ? 'diag' : ''}">${v || '·'}</td>`).join(''))); });
}

function init() {
  try { if (localStorage.getItem('polh-theme') === 'randen') document.body.classList.add('theme-randen'); } catch (e) { /* private mode */ }
  $('#btnTheme').onclick = () => {
    document.body.classList.toggle('theme-randen');
    try { localStorage.setItem('polh-theme', document.body.classList.contains('theme-randen') ? 'randen' : 'pink'); } catch (e) { /* ignore */ }
  };
  ['#fLabel', '#fPred', '#fWave'].forEach(s => $(s).addEventListener('change', () => { page = 0; render(); }));
  $('#pPrev').onclick = () => { page = Math.max(0, page - 1); render(); };
  $('#pNext').onclick = () => { page++; render(); };
  loadStatus(); loadIndex(); loadModel();
  window.polhTranscript($('#aud'), $('#transcript'));
}
init();
