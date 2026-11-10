// Measure ze live stream of ze trap: are ze 100 ms packets arriving evenly?
//   node tools/live_latency.js [host] [seconds]      (Node >= 22, built-in WebSocket)
//   node tools/live_latency.js polh-kill.local 20
// Reference 2026-11-08 (RandeLab WiFi): 193/200 packets, gap median 100 ms, p99 261 ms, max 287 ms.
const host = process.argv[2] || 'polh-kill.local';
const secs = Number(process.argv[3] || 20);
const ws = new WebSocket(`ws://${host}/ws`);
const gaps = [];
let first = 0, last = 0, points = 0;  // count from ze first packet: .local lookup and connect take seconds
ws.onmessage = m => {
  const d = JSON.parse(m.data);
  if (d.t !== 'live') return;
  const now = performance.now();
  if (!first) first = now;
  if (last) gaps.push(now - last);
  last = now;
  points += d.v.length;
};
ws.onerror = e => { console.error('websocket error', e.message || e); process.exit(1); };
setTimeout(() => {
  if (!gaps.length) { console.log('no live messages - is ze trap reachable?'); process.exit(1); }
  gaps.sort((a, b) => a - b);
  const q = p => gaps[Math.floor(p * (gaps.length - 1))].toFixed(0);
  const span = (last - first) / 1000;
  console.log(`messages ${gaps.length + 1} in ${span.toFixed(1)} s (expected ${Math.round(span * 10) + 1}), points ${points} (${(points / span).toFixed(1)}/s)`);
  console.log(`gap ms: median ${q(0.5)}, p90 ${q(0.9)}, p99 ${q(0.99)}, max ${q(1)}`);
  process.exit(0);
}, secs * 1000);
