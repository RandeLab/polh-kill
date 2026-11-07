"""Plot the polh-kill dataset export (http://polh-kill.local/api/export.csv).

    python tools/plot_samples.py polh-kill-samples.csv

Shows every feature as a strip plot per class, so you can see which features
separate a snap from a bump. Needs matplotlib (pip install matplotlib).
"""
import csv
import sys
from collections import defaultdict

import matplotlib.pyplot as plt

CLASSES = ["snap", "bump", "footstep", "mouse", "other"]
COLORS = {"snap": "#E24B4A", "bump": "#378ADD", "footstep": "#639922", "mouse": "#BA7517", "other": "#888780"}


def main(path):
    with open(path, newline="", encoding="utf-8") as f:
        rows = list(csv.DictReader(f))
    if not rows:
        sys.exit("empty file - teach ze trap something first")
    features = [k for k in rows[0] if k not in ("id", "ts", "label", "rate_hz", "raw")]
    by_class = defaultdict(list)
    for r in rows:
        by_class[r["label"]].append(r)

    cols = 4
    nrows = (len(features) + cols - 1) // cols
    fig, axes = plt.subplots(nrows, cols, figsize=(4 * cols, 2.6 * nrows))
    for ax, feat in zip(axes.flat, features):
        for i, c in enumerate(CLASSES):
            vals = [float(r[feat]) for r in by_class.get(c, [])]
            ax.scatter([i + (j % 7 - 3) * 0.04 for j in range(len(vals))], vals, s=12, color=COLORS[c], alpha=0.8)
        ax.set_title(feat, fontsize=9)
        ax.set_xticks(range(len(CLASSES)), CLASSES, fontsize=7)
    for ax in list(axes.flat)[len(features):]:
        ax.axis("off")
    counts = ", ".join(f"{c}: {len(by_class.get(c, []))}" for c in CLASSES)
    fig.suptitle(f"polh-kill dataset ({counts})")
    fig.tight_layout()
    plt.show()


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "polh-kill-samples.csv")
