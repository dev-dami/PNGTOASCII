#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEST_DIR="$ROOT_DIR/tests/fixtures/downloaded"
mkdir -p "$DEST_DIR"

if ! command -v curl >/dev/null 2>&1; then
  echo "curl is not installed; skipping image downloads"
  exit 0
fi

URLS=(
  "https://upload.wikimedia.org/wikipedia/commons/8/89/HD_transparent_picture.png"
  "https://upload.wikimedia.org/wikipedia/commons/4/47/PNG_transparency_demonstration_1.png"
  "https://upload.wikimedia.org/wikipedia/commons/6/63/Wikipedia-logo.png"
)

NAMES=(
  "transparent.png"
  "transparency-demo.png"
  "wikipedia-logo.png"
)

downloaded=0
for i in "${!URLS[@]}"; do
  url="${URLS[$i]}"
  out="$DEST_DIR/${NAMES[$i]}"

  if curl -fsSL --retry 2 "$url" -o "$out"; then
    echo "Downloaded: ${NAMES[$i]}"
    downloaded=$((downloaded + 1))
  else
    echo "Warning: failed to download $url"
    rm -f "$out"
  fi
done

if [[ "$downloaded" -eq 0 ]]; then
  echo "No external images downloaded (network restricted or URLs unavailable)."
  echo "You can still run local deterministic tests via: make test"
  exit 0
fi

echo "Saved $downloaded image(s) to $DEST_DIR"
