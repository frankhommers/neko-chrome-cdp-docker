#!/usr/bin/env bash
# Generate the neko-lite client icon set from one square source PNG (transparent background).
# Usage: scripts/make-icons.sh <source.png> <client/public dir>
set -euo pipefail
SRC=$1; OUT=$2
for s in 16 32; do convert "$SRC" -resize ${s}x${s} "$OUT/favicon-${s}x${s}.png"; done
convert "$SRC" -resize 180x180 "$OUT/apple-touch-icon.png"
convert "$SRC" -resize 192x192 "$OUT/android-chrome-192x192.png"
convert "$SRC" -resize 512x512 "$OUT/android-chrome-512x512.png"
for s in 70 144 150 310; do convert "$SRC" -resize ${s}x${s} "$OUT/mstile-${s}x${s}.png"; done
convert "$SRC" -resize 310x150 -background none -gravity center -extent 310x150 "$OUT/mstile-310x150.png"
convert "$SRC" -resize 16x16 "$OUT/favicon-16.tmp.png"
convert "$SRC" -resize 32x32 "$OUT/favicon-32.tmp.png"
convert "$SRC" -resize 48x48 "$OUT/favicon-48.tmp.png"
convert "$OUT/favicon-16.tmp.png" "$OUT/favicon-32.tmp.png" "$OUT/favicon-48.tmp.png" "$OUT/favicon.ico"
rm -f "$OUT"/favicon-*.tmp.png
