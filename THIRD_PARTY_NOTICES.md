# Third-party notices

This repository includes and modifies components from the upstream Neko project.

## Neko client

- Project: Neko
- Upstream: https://github.com/m1k1o/neko
- Package metadata: `client/package.json`
- Original author listed by upstream package metadata: Nurdism <https://github.com/nurdism>
- License: Apache License 2.0
- Local license copy: `LICENSES/Apache-2.0.txt`

The `client/` directory contains a stripped-down Vue frontend based on Neko's original client. Local changes remove UI features that are not needed for this image, such as chat, emotes, file transfer, members list, sidebar, and about dialog.

## Base image

This Docker image is built from:

```text
ghcr.io/m1k1o/neko/google-chrome:3
```

See the upstream Neko project for its own licensing and notices.
