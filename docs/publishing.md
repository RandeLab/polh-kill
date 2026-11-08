# Publishing ze captures on GitHub Pages

Every vibration the trap feels is stored on the ESP and, if you give it a token, published to
**https://randelab.github.io/polh-kill/** – waveforms, predictions, labels, model quality. Ze trap
becomes a public intellectual. Whether this is a good idea, history will decide.

## How it works

```
ESP32-S3 ──(Git Data API, one commit per batch)──▶ gh-pages/data/…   ◀── GitHub Pages
main/pages/ ──(GitHub Action "Deploy ze dashboard")──▶ gh-pages/ (index.html, dash.js, style, interview)
```

- The ESP stores **every capture** on its own 6 MB flash partition: features + prediction forever
  (60 bytes each, ~90 000 fit), the 1 s waveform for the newest **650** (6 kB each).
- Every 15 minutes (adjustable, Settings → GitHub Pages), and right after a snap, it pushes a batch
  of up to 20 new captures as **one commit** to the `gh-pages` branch:
  - `data/captures/<id>.json` – waveform (x, y, z in mg), features, label, prediction
  - `data/index.json` – all captures in one list (what the dashboard shows)
  - `data/samples.csv` – all features, for pandas & co.
  - `data/model.json` – class statistics and leave-one-out confusion matrix
  - `data/status.json` – summary (no IP addresses, no WiFi names)
- Relabelling or deleting in the learning lab marks things as changed; the next push rewrites the
  index, so ze past is rewritten, as always.
- Device commits are written in ze voice of ze philosopher too. Of course.

## Token (once)

1. GitHub → *Settings → Developer settings → Fine-grained personal access tokens → Generate new token*.
2. **Resource owner:** RandeLab · **Repository access:** only `RandeLab/polh-kill`.
3. **Permissions → Repository → Contents: Read and write.** Nothing else.
4. Expiration: your choice (the device tells you on the settings page when it is rejected).
5. Open **http://polh-kill.local/setup** (or the IP), paste the token into *GitHub token*, save.
   The ESP reboots and pushes within a minute.

The token lives only in the ESP's NVS. It can write to this one repo and nothing else. If the
organisation requires approval for fine-grained tokens, an owner of RandeLab has to approve it once.

## Dashboard

`pages/` on `main` holds the dashboard (plain HTML + JS, no build). A push to `main` that touches
`pages/`, `data/style.css` or `data/interview.mp3` runs `.github/workflows/pages.yml`, which copies
those files onto `gh-pages` and never touches `gh-pages/data/`.

Pages settings: *Settings → Pages → Source: Deploy from a branch → gh-pages / (root)*.
