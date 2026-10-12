#!/bin/bash
# Route all internet traffic via WireGuard (home IP) for sites that block datacenter IPs
# Keep WG endpoint routed directly to avoid routing loop
WG_GW="${WG_GATEWAY:-}"
WG_ENDPOINT="${WG_ENDPOINT:-}"
PROXY_GW="${PROXY_GATEWAY:-}"

if [ -n "$WG_GW" ] && [ -n "$WG_ENDPOINT" ] && [ -n "$PROXY_GW" ]; then
  if ip route show default | grep -q "$WG_GW"; then
    echo "[entrypoint] WG default route already set"
  else
    # Route WG endpoint via proxy network (prevent loop)
    ip route add "$WG_ENDPOINT/32" via "$PROXY_GW" 2>/dev/null || true
    # Switch default route to WireGuard
    ip route replace default via "$WG_GW"
    echo "[entrypoint] Default route set via WG ($WG_GW), endpoint $WG_ENDPOINT via $PROXY_GW"
  fi
else
  echo "[entrypoint] WG routing skipped (set WG_GATEWAY, WG_ENDPOINT, PROXY_GATEWAY to enable)"
fi

# Optional page title override (browser tab + installed web app name)
if [ -n "${PAGE_TITLE:-}" ]; then
  python3 - "$PAGE_TITLE" <<'PY'
import html, json, re, sys
t = sys.argv[1]
p = "/var/www/index.html"; s = open(p).read()
s = re.sub(r"<title>.*?</title>", "<title>" + html.escape(t) + "</title>", s, count=1)
open(p, "w").write(s)
p = "/var/www/site.webmanifest"; m = json.load(open(p))
m["name"] = m["short_name"] = t
json.dump(m, open(p, "w"), indent=4)
PY
  echo "[entrypoint] page title set to: ${PAGE_TITLE}"
fi

exec /usr/bin/supervisord -c /etc/neko/supervisord.conf
