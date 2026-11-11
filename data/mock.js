/* Offline mock of the polh-kill API, used when the UI is opened from disk
   (file://) or with ?mock. Lets you click through the learning lab without a device. */
'use strict';
(function () {
  if (!(location.protocol === 'file:' || new URLSearchParams(location.search).has('mock'))) return;
  const CL = ['snap', 'bump', 'footstep', 'mouse', 'other', 'armed-trap'];
  const EM = { snap: '💥', bump: '🤜', footstep: '👣', mouse: '🐭', other: '❓', 'armed-trap': '🪤' };
  const NB = CL.length;
  const subs = [];
  const cfg = { gh_every: 15, k_sigma: 6, min_g: 0.05, snap_g: 1, alert_p: 0.7, unsure_p: 0.35, armed: true, notify_unsure: true, daily: true, voice: true };
  const st = { events: 0, snaps: 3, armed: null, armedAt: 0, nextId: 1, nextSample: 1 };
  const recent = [], windows = {}, samples = [];
  const t0 = Date.now();
  const emit = m => subs.forEach(f => f(m));
  const rnd = (a, b) => a + Math.random() * (b - a);

  const PROFILE = { snap: [3.5, 190, 0.025], bump: [0.7, 40, 0.09], footstep: [0.15, 14, 0.12], mouse: [0.1, 320, 0.2], other: [0.6, 100, 0.06] };
  function makeWindow(cls) {
    const [amp0, f, tau] = PROFILE[cls], amp = amp0 * rnd(0.6, 1.4), n = 1000, trig = 200;
    const d = [rnd(-1, 1), rnd(-1, 1), rnd(-1, 1)], dn = Math.hypot(...d);
    const w = { rate: 1000, n, trig, threshold_g: 0.03, x: [], y: [], z: [] };
    for (let i = 0; i < n; i++) {
      const tt = (i - trig) / 1000;
      let v = tt >= 0 ? amp * Math.exp(-tt / tau) * Math.sin(2 * Math.PI * f * tt) : 0;
      if (cls === 'snap' && tt > 0.06) v += 0.4 * amp * Math.exp(-(tt - 0.06) / tau) * Math.sin(2 * Math.PI * f * (tt - 0.06));
      ['x', 'y', 'z'].forEach((a, k) => w[a].push(Math.round((v * d[k] / dn + (Math.random() - 0.5) * 0.008) * 1000)));
    }
    return { w, peak: amp };
  }
  function predict(peak) {
    const n = samples.length;
    const p = n < 2 ? (peak > cfg.snap_g ? 0.8 : 0.3 * peak / cfg.snap_g) : Math.min(0.98, Math.max(0.02, (Math.log10(peak) + 0.3) / 0.9));
    const probs = {}; CL.forEach(c => probs[c] = c === 'snap' ? p : (1 - p) / 4);
    return { p, probs, method: n < 2 ? 'heuristic' : n < 6 ? 'bayes' : 'bayes+rls' };
  }
  function inject(cls) {
    const { w, peak } = makeWindow(cls), id = st.nextId++, pr = predict(peak);
    windows[id] = w;
    const best = pr.p >= 0.5 ? 'snap' : (peak > 0.4 ? 'bump' : peak > 0.12 ? 'footstep' : 'mouse');
    const e = { id, ts: Math.floor(Date.now() / 1000), ago_ms: 0, peak_g: peak, cls: best, snap_p: pr.p, method: pr.method,
      candidate: !!st.armed, sample_id: 0, probs: pr.probs,
      features: { peak_log: Math.log10(peak), rms_log: Math.log10(peak / 4), jerk_log: 2.5, duration_ms: 80, zcr_hz: PROFILE[cls][1], band_0_30: 0.1, band_30_100: 0.2, band_100_250: 0.5, band_250_top: 0.2, axis_dom: 0.6, settle_ms: 90, peaks: 7, gyro_log: cls === 'snap' ? 2.4 : 0.8, delta_tilt: cls === 'snap' ? 4.5 : 0.2, axis_z: cls === 'snap' ? 0.75 : 0.3 } };
    if (st.armed) { e.test_label = st.armed; st.armed = null; }
    st.events++;
    recent.unshift(e); recent.splice(6);
    emit({ t: 'event', e });
    burst = { amp: peak * 1000, left: 40 };
  }
  let burst = null;
  setInterval(() => {
    const v = [], x = [], y = [], z = [];
    for (let i = 0; i < 5; i++) {
      let m = 3 + Math.random() * 3;
      if (burst && burst.left > 0) { m += burst.amp * Math.exp(-(40 - burst.left) / 6); burst.left--; }
      const ang1 = Math.random() * Math.PI * 2, ang2 = (Math.random() - 0.5) * Math.PI;
      v.push(+m.toFixed(1));
      x.push(Math.round(m * Math.cos(ang1) * Math.cos(ang2)));
      y.push(Math.round(m * Math.sin(ang1) * Math.cos(ang2)));
      z.push(Math.round(m * Math.sin(ang2)));
    }
    const t = Date.now() / 1000;
    const g = [0.15 * Math.sin(t * 0.5), -0.22 * Math.cos(t * 0.4), 0.96];
    const ori = [+(Math.atan2(g[1], g[2]) * 180 / Math.PI).toFixed(1), +(Math.atan2(-g[0], Math.hypot(g[1], g[2])) * 180 / Math.PI).toFixed(1), +(18 * Math.sin(t * 0.25)).toFixed(1)];
    const w = [+(Math.random() - 0.5).toFixed(1), +(Math.random() - 0.5).toFixed(1), +(Math.random() - 0.5).toFixed(1)];
    emit({ t: 'live', v, x, y, z, g, w, ori, temp: 27.4, thr: 32, base: 4.2 });
  }, 100);
  setInterval(() => emit({ t: 'lab', armed_label: st.armed, remaining_ms: st.armed ? Math.max(0, 60000 - (Date.now() - st.armedAt)) : 0, samples: samples.length }), 1000);

  function model() {
    const classes = CL.map(c => {
      const s = samples.filter(x => x.label === c);
      const mp = s.length ? s.reduce((a, x) => a + Math.log10(x.peak_g), 0) / s.length : 0;
      return { name: c, emoji: EM[c], n: s.length, mean: [mp, 0, 0, 80 + Math.random() * 40], sd: [] };
    });
    const conf = CL.map((c, i) => CL.map((d, j) => i === j ? samples.filter(x => x.label === c).length : 0));
    const tp = conf[0][0];
    return { total: samples.length, classes, loo: { total: samples.length, correct: samples.length, snap_tp: tp, snap_fp: 0, snap_fn: 0, confusion: conf } };
  }
  function label(eventId, lab) {
    const e = recent.find(x => x.id == eventId);
    if (!e) throw new Error('event gone');
    if (!e.sample_id) { e.sample_id = st.nextSample++; samples.push({ id: e.sample_id, ts: e.ts, label: lab, raw: true, peak_g: e.peak_g, ev: e.id }); }
    else samples.find(s => s.id === e.sample_id).label = lab;
    e.stored_label = lab;
    return { ok: true, sample_id: e.sample_id };
  }
  // a few demo events so the page is not empty
  setTimeout(() => { inject('footstep'); setTimeout(() => inject('snap'), 400); }, 300);

  window.polhMock = {
    subscribe: f => subs.push(f),
    async get(path) {
      const u = new URL(path, 'http://x'), p = u.pathname;
      if (p === '/api/status') return { name: 'polh-kill', version: '0.1.0-mock', uptime_ms: Date.now() - t0 + 3600e3, heap: 210000, psram: 8.1e6,
        net: { mode: 'online', ip: '192.168.1.42', ssid: 'RandeLab', rssi: -61, ap_clients: 0 },
        sensor: { ok: true, name: 'simulator', rate: 1000, samples: 1, baseline_g: 0.0042, sigma_g: 0.0015, threshold_g: 0.032, gravity: [0.02, -0.01, 1.0], simulated: true },
        stats: { events: st.events, snaps_total: st.snaps, snaps_boot: 0 },
        lab: { armed_label: st.armed, remaining_ms: st.armed ? 60000 - (Date.now() - st.armedAt) : 0, samples: samples.length },
        telegram: { configured: true, paired: true, bot: 'PolhKillBot', last_error: '', join_code: '482913', max_users: 10, pending: 1, users: [{ chat: '1', name: 'Aaron', owner: true }, { chat: '2', name: 'Marc', owner: false }] }, armed: cfg.armed, fs: { used: 120e3, total: 6e6, captures: st.events, labelled: samples.length, raw: st.events, raw_kb: 240, raw_budget_kb: 2560 },
        github: { configured: true, repo: 'RandeLab/polh-kill', branch: 'gh-pages', pages_url: 'https://randelab.github.io/polh-kill/', last_ok_ago_ms: 240000, backlog: 2, last_message: '🐭 3 new captures from ze trap', last_error: '' } };
      if (p === '/api/config') return { ...cfg };
      if (p === '/api/events') return recent;
      if (p === '/api/classes') return CL.map((c, i) => ({ name: c, emoji: EM[c], builtin: i < NB, n: samples.filter(s => s.label === c).length }));
      if (p === '/api/model') return model();
      if (p === '/api/samples') return samples.map(({ ev, ...s }) => s);
      if (p === '/api/window') {
        const ev = u.searchParams.get('event'), sa = u.searchParams.get('sample');
        const id = ev || (samples.find(s => s.id == sa) || {}).ev;
        if (!windows[id]) throw new Error('gone');
        return windows[id];
      }
      throw new Error('mock: ' + p);
    },
    async post(path, params) {
      if (path === '/api/classes/add') {
        const n = (params.name || '').toLowerCase();
        if (!/^[a-z0-9_-]{1,16}$/.test(n)) throw new Error('name: 1-16 chars, a-z 0-9 - _');
        if (CL.includes(n)) throw new Error('class exists');
        CL.push(n); EM[n] = params.emoji || '🏷️'; return { ok: true };
      }
      if (path === '/api/classes/delete') {
        const i = CL.indexOf(params.name);
        if (i < NB) throw new Error('built-in classes cannot be deleted');
        if (samples.some(s => s.label === params.name)) throw new Error('class still has samples');
        CL.splice(i, 1); return { ok: true };
      }
      if (path === '/api/sim/inject') { inject(params.label); return { ok: true }; }
      if (path === '/api/lab/arm') { st.armed = params.label; st.armedAt = Date.now(); return { ok: true }; }
      if (path === '/api/lab/cancel') { st.armed = null; return { ok: true }; }
      if (path === '/api/lab/label') return label(params.event, params.label);
      if (path === '/api/samples/relabel') { samples.find(s => s.id == params.id).label = params.label; return { ok: true }; }
      if (path === '/api/samples/delete') { samples.splice(samples.findIndex(s => s.id == params.id), 1); return { ok: true }; }
      if (path === '/api/model/wipe') { samples.length = 0; return { ok: true }; }
      if (path === '/api/config') { Object.keys(params).forEach(k => cfg[k] = typeof cfg[k] === 'boolean' ? params[k] == 1 : +params[k]); return { ok: true }; }
      return { ok: true };
    },
  };
})();
