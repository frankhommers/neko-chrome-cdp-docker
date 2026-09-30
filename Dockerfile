# Stage 1: Build Vue client
FROM node:20-alpine AS client-build
COPY client/ /build/
WORKDIR /build
RUN npm ci && npm run build

# Stage 2: Build the damagegate GStreamer plugin (drops unchanged X11 frames)
FROM debian:trixie-slim AS damagegate-build
RUN apt-get update -qq && \
    apt-get install -y -qq --no-install-recommends gcc libc6-dev pkg-config \
      libgstreamer1.0-dev libgstreamer-plugins-base1.0-dev libx11-dev libxdamage-dev libxfixes-dev && \
    rm -rf /var/lib/apt/lists/*
COPY gst-damagegate/ /src/
RUN gcc -O2 -Wall -Werror -fPIC -shared -o /libgstdamagegate.so /src/gstdamagegate.c \
      $(pkg-config --cflags --libs gstreamer-1.0 gstreamer-base-1.0 gstreamer-video-1.0 x11 xdamage xfixes)

# Stage 3: Final image
FROM ghcr.io/m1k1o/neko/google-chrome:3
COPY --from=damagegate-build /libgstdamagegate.so /usr/lib/x86_64-linux-gnu/gstreamer-1.0/libgstdamagegate.so
RUN rm -rf /var/www/*
COPY --from=client-build /build/dist/ /var/www/
RUN apt-get update -qq && \
    apt-get install -y -qq socat iproute2 && \
    apt-get install -y -qq --only-upgrade google-chrome-stable && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
ENTRYPOINT ["/entrypoint.sh"]
# Disable audio: kill PulseAudio config so nothing tries to connect
RUN echo "autospawn = no" > /etc/pulse/client.conf && \
    echo "daemon-binary = /bin/true" >> /etc/pulse/client.conf

COPY policies.json /etc/opt/chrome/policies/managed/policies.json
COPY google-chrome.conf /etc/neko/supervisord/google-chrome.conf
COPY cdp-proxy.conf /etc/neko/supervisord/cdp-proxy.conf
