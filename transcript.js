/* Ze transcript follows ze interview: the line being spoken lights up and
   scrolls into view; click a line and ze audio jumps there. Used by the device
   UI and the GitHub Pages dashboard. And so on, and so on. */
'use strict';
window.polhTranscript = async function (audio, box, src = 'interview.json') {
  let lines;
  try { lines = await (await fetch(src)).json(); } catch (e) { return; }
  box.innerHTML = '';
  // ze ESP web server knows no HTTP range requests, so ze browser cannot seek in ze stream.
  // First jump: load ze whole mp3 (730 kB) into a blob, then seeking is pure freedom.
  let blobbed = null;
  const seek = async t => {
    if (!blobbed) {
      blobbed = fetch(audio.currentSrc || audio.src).then(r => r.blob()).then(b => new Promise(res => {
        audio.addEventListener('loadedmetadata', res, { once: true });
        audio.src = URL.createObjectURL(b);
      }));
    }
    await blobbed;
    audio.currentTime = t;
    audio.play().catch(() => {});
  };
  const ps = lines.map(l => {
    const p = document.createElement('p');
    p.innerHTML = `<span class="who">${l.who === 'host' ? '🎙️ Host' : '🧔 Ze philosopher'}</span> `;
    p.append(document.createTextNode(l.text));
    p.title = `${Math.floor(l.start / 60)}:${String(Math.floor(l.start % 60)).padStart(2, '0')}`;
    p.onclick = () => seek(l.start + 0.05);
    box.append(p);
    return p;
  });
  let current = -1;
  audio.addEventListener('timeupdate', () => {
    const t = audio.currentTime;
    const i = lines.findIndex(l => t >= l.start && t < l.end);
    if (i === current) return;
    if (current >= 0) ps[current].classList.remove('now');
    current = i;
    if (i >= 0) {
      ps[i].classList.add('now');
      // scroll inside the box only, never the whole page
      box.scrollTo({ top: ps[i].offsetTop - box.offsetTop - 8, behavior: 'smooth' });
    }
  });
};
