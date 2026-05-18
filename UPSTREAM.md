# Upstream tracking

This project is based on the original [Neko](https://github.com/m1k1o/neko) Chrome image and a stripped-down version of Neko's Vue client.

The goal is to stay compatible with upstream Neko behavior and feature/fix evolution, while keeping this image focused on Chrome + CDP automation.

## Upstream sources

- Upstream repository: <https://github.com/m1k1o/neko>
- Base Docker image: `ghcr.io/m1k1o/neko/google-chrome:3`
- Local client source: `client/`
- Upstream client source: `client/` in `m1k1o/neko`

## Current reviewed upstream baseline

Last reviewed upstream state:

- Upstream repository HEAD: `2c124c50`
- Last upstream commit touching `client/`: `6c743c9c733ee170d66a6f73c96ba98355812287`
- Upstream Neko tags containing that client commit: `v3.0.11` through `v3.1.4`
- Local `client/package.json` version: `2.5.0`
- Upstream `client/package.json` version at review time: `2.5.0`

When upstream client changes are reviewed and intentionally ported or skipped, update the baseline above.

## Divergence policy

The local `client/` is intentionally not a byte-for-byte copy of upstream Neko's client.

Features intentionally removed from the local client:

- chat
- emotes/emoji UI
- file transfer UI
- members list
- sidebar
- about dialog
- related dependencies that only support the removed UI

Do not bulk-copy upstream `client/` over the local client. That would likely reintroduce removed UI and dependencies.

## What to keep on-par with upstream

Port or consciously evaluate upstream changes related to:

- WebRTC connection behavior
- video playback and stream handling
- websocket/session lifecycle
- heartbeat/keepalive behavior
- keyboard, mouse, clipboard, touch, mobile input
- control/hosting/locking semantics
- browser compatibility fixes
- security fixes
- protocol/message schema changes required by newer Neko server images
- localization changes if they affect retained UI strings

Usually skip or adapt carefully:

- chat, emoji, emotes, file transfer, members/sidebar/about features
- dependencies only needed for removed UI
- styling changes that only affect removed components

## Review workflow

1. Check whether upstream Neko changed since the stored baseline:

   ```bash
   ./scripts/check-neko-upstream.sh
   ```

2. Inspect upstream commits touching `client/`:

   ```bash
   git clone https://github.com/m1k1o/neko.git /tmp/neko-upstream
   cd /tmp/neko-upstream
   git log --oneline --date=short --format='%h %ad %s' -- client
   ```

3. Compare local client with upstream:

   ```bash
   diff -qr \
     --exclude=node_modules \
     --exclude=dist \
     --exclude=.git \
     /tmp/neko-upstream/client \
     ./client
   ```

4. For each upstream client change, decide:

   - port unchanged
   - port with adaptation for the stripped UI
   - skip because it only affects intentionally removed UI

5. After review, update the baseline in this file.

6. Verify:

   ```bash
   docker compose config
   docker build .
   ```

## Scheduled check

`.github/workflows/upstream-neko.yml` runs the upstream check weekly and can also be triggered manually. It fails when upstream Neko has newer client changes than the baseline in this file, so the repository gets a visible signal to review upstream changes.
