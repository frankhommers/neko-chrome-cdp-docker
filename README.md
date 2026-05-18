# neko-chrome-cdp-docker

Docker setup for **Neko Chrome** with Chrome DevTools Protocol (CDP) access.

This image extends the original [Neko](https://github.com/m1k1o/neko) Chrome container so the same browser session can be used both interactively through Neko's WebRTC UI and programmatically through CDP-compatible tools such as Playwright, Puppeteer, or custom automation scripts.

## The Problem

Neko is great for interactive browser access via WebRTC, but the stock Chrome image is not ideal when you also want automation access:

1. Its bundled Chrome version can lag behind what automation tooling expects
2. Chrome in non-headless mode binds CDP to `127.0.0.1` only, so it cannot be reached directly from outside the container
3. Neko's managed policy disables DevTools/CDP entirely (`DeveloperToolsAvailability: 2`)

## The Solution

A custom Dockerfile that fixes all three issues:

- **Upgrades Chrome stable** to the latest available version via apt
- **Adds a socat CDP proxy** forwarding `0.0.0.0:9223` → `127.0.0.1:9222` inside the container
- **Fixes Chrome policy** to re-enable DevTools/CDP

The result: a single Neko container that serves both interactive WebRTC browsing and CDP automation. No separate headless Chrome needed.

## Upstream

This project is based on the original Neko Chrome image:

- Upstream project: <https://github.com/m1k1o/neko>
- Base image: `ghcr.io/m1k1o/neko/google-chrome:3`
- Client: stripped-down Vue frontend based on Neko's original client

The goal is not to replace Neko, but to package a Chrome/CDP-friendly variant for automation use cases. See `UPSTREAM.md` for the upstream tracking policy and reviewed baseline.

## Architecture

```text
CDP-compatible client (Playwright, Puppeteer, custom tooling)
    │
    └─ CDP ──→ host 127.0.0.1:9222
                  │
                  └─→ container 0.0.0.0:9223 (socat proxy)
                        │
                        └─→ Chrome 127.0.0.1:9222 (non-headless)
                              │
                              └─→ Neko WebRTC UI
```

You can watch and interact with the browser session through Neko while automation controls the same Chrome instance over CDP.

## Ports

- `9222/tcp` on host localhost only: CDP endpoint for automation clients
- `8080/tcp`: Neko Web UI, intended to be placed behind a reverse proxy
- `52000-52100/udp`: WebRTC media streams

## Quick Start

```bash
cp .env.example .env
# Edit .env with your Neko credentials, domain, and public IP
docker compose build
docker compose up -d
```

### Connect with Playwright

```ts
import { chromium } from "playwright";

const browser = await chromium.connectOverCDP("http://127.0.0.1:9222");
const context = browser.contexts()[0] ?? await browser.newContext();
const page = context.pages()[0] ?? await context.newPage();
await page.goto("https://example.com");
```

### Connect with Puppeteer

```js
import puppeteer from "puppeteer-core";

const browser = await puppeteer.connect({
  browserURL: "http://127.0.0.1:9222",
});

const page = await browser.newPage();
await page.goto("https://example.com");
```

## Prerequisites

- Docker with Compose v2
- A reverse proxy, for example Traefik, on a shared Docker network
- UDP ports `52000-52100` open for WebRTC
- `NEKO_NAT1TO1` / public IP configured so WebRTC can reach the host from outside

## Key Discoveries

These details are easy to miss when combining Neko, Chrome, and CDP.

### Chrome non-headless always binds CDP to 127.0.0.1

`--remote-debugging-address=0.0.0.0` is ignored in non-headless mode. Chrome still binds CDP to loopback. The socat proxy is the lightest-weight workaround.

### Neko's DevTools policy blocks CDP

Neko sets `DeveloperToolsAvailability: 2` (disabled) in `/etc/opt/chrome/policies/managed/policies.json`. This image replaces that policy and sets it to `0`.

### Non-default user-data-dir required

Chrome refuses `--remote-debugging-port` with the default profile data directory. The supervisord config uses:

```text
/home/neko/.config/google-chrome-cdp
```

### Automation client compatibility depends on Chrome/CDP version

Some automation clients expect a sufficiently recent Chrome/CDP version. The Neko base image can lag behind, so this image upgrades `google-chrome-stable` during build.

### Restart clients after rebuilding the browser container

When the Neko container is rebuilt or restarted, existing CDP/WebSocket connections become stale. Restart any automation process connected to the old browser instance.

## Persistent Chrome Data

Chrome's user data (shortcuts, preferences, cookies, extensions state) is stored in a Docker named volume (`chrome-data`) mounted at:

```text
/home/neko/.config/google-chrome-cdp
```

This means:

- `docker compose down` + `docker compose up`: data persists
- `docker compose down -v`: data is wiped because the volume is deleted
- image rebuild: data persists because the volume is independent of the image

### First-time setup

New Tab Page shortcuts cannot reliably be pre-seeded via the Dockerfile. Chrome's NTP has its own internal state that updates through the UI. After first deployment, add shortcuts manually via the "Add shortcut" button on the new tab page, or use browser automation through CDP. Once added, they persist in the volume.

## Chrome Policy

The Dockerfile replaces Neko's restrictive Chrome policy with a clean one (`policies.json`):

- DevTools/CDP enabled
- uBlock Origin force-installed
- SponsorBlock removed
- No bookmarks bar, password manager, autofill, or sync

## Audio

Audio is force-disabled at two levels, so no `neko.yaml` audio config is needed:

1. Chrome flags: `--mute-audio`, `--disable-audio-output`, `--disable-audio-input`
2. PulseAudio disabled: Dockerfile sets `autospawn = no` and `daemon-binary = /bin/true`

## Neko Lite Client

The `client/` directory contains a stripped-down Vue frontend based on Neko's original client. Chat, emotes, file transfer, members list, sidebar, and about dialog are removed. The Dockerfile uses a multi-stage build to compile and serve this client instead of the default Neko UI.

## Files

```text
├── Dockerfile              # Multi-stage: Vue build + Chrome upgrade + socat + policy
├── docker-compose.yml      # Neko container config + persistent volume
├── docker-compose.yml.example
├── neko.yaml.example       # Neko config template
├── .env.example            # Credentials and deployment settings template
├── policies.json           # Chrome managed policy (DevTools, uBlock, clean defaults)
├── google-chrome.conf      # Supervisord: Chrome stable with CDP flags
├── cdp-proxy.conf          # Supervisord: socat CDP proxy
└── client/                 # Neko Lite Vue frontend (stripped)
```

## License

The project-specific Docker/configuration code is licensed under MIT. The Neko-derived client code in `client/` remains under Apache License 2.0; see `THIRD_PARTY_NOTICES.md` and `LICENSES/Apache-2.0.txt`.
