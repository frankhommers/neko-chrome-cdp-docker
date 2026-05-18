#!/usr/bin/env bash
set -euo pipefail

UPSTREAM_REPO="https://github.com/m1k1o/neko.git"
BASELINE_FILE="${BASELINE_FILE:-UPSTREAM.md}"

if [[ ! -f "$BASELINE_FILE" ]]; then
  echo "Baseline file not found: $BASELINE_FILE" >&2
  exit 2
fi

baseline_head="$(grep -E '^- Upstream repository HEAD: `' "$BASELINE_FILE" | sed -E 's/.*`([^`]+)`.*/\1/' | head -1)"
baseline_client="$(grep -E '^- Last upstream commit touching `client/`: `' "$BASELINE_FILE" | sed -E 's/.*`([^`]+)`.*/\1/' | head -1)"

if [[ -z "$baseline_head" || -z "$baseline_client" ]]; then
  echo "Could not read upstream baseline from $BASELINE_FILE" >&2
  exit 2
fi

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

git clone --quiet --filter=blob:none "$UPSTREAM_REPO" "$tmp/neko"
cd "$tmp/neko"

current_head="$(git rev-parse HEAD)"
current_head_short="$(git rev-parse --short HEAD)"
current_client="$(git log -1 --format=%H -- client)"
current_client_short="$(git rev-parse --short "$current_client")"

baseline_head_full="$(git rev-parse "$baseline_head")"
baseline_client_full="$(git rev-parse "$baseline_client")"

echo "Upstream repo: $UPSTREAM_REPO"
echo "Baseline HEAD:        $baseline_head_full"
echo "Current HEAD:         $current_head ($current_head_short)"
echo "Baseline client tip:  $baseline_client_full"
echo "Current client tip:   $current_client ($current_client_short)"
echo

if [[ "$current_client" != "$baseline_client_full" ]]; then
  echo "Neko upstream client has changed since the reviewed baseline."
  echo
  echo "Commits touching client/ since baseline:"
  git log --oneline --date=short --format='%h %ad %s' "${baseline_client_full}..${current_client}" -- client
  echo
  echo "Review these changes against UPSTREAM.md and update the baseline after porting or consciously skipping them."
  exit 1
fi

if [[ "$current_head" != "$baseline_head_full" ]]; then
  echo "Neko upstream repository HEAD changed, but client/ has not changed since the reviewed baseline."
  echo "No client sync action is required. Consider updating the repository HEAD baseline after review."
  exit 0
fi

echo "Neko upstream client is on the reviewed baseline."
