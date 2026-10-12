#!/usr/bin/env python3
"""Rebrand the neko-lite client: page title, manifest name, theme colour and icons.
Usage: brand.py <repo dir> <title> <theme hex> <logo png>"""
import json, os, re, subprocess, sys

repo, title, theme, logo = sys.argv[1:5]
pub = os.path.join(repo, "client/public")

# icons
os.makedirs(os.path.join(repo, "branding"), exist_ok=True)
subprocess.run(["cp", logo, os.path.join(repo, "branding/logo.png")], check=True)
subprocess.run(["bash", os.path.join(repo, "scripts/make-icons.sh"), os.path.join(repo, "branding/logo.png"), pub], check=True)

# index.html
p = os.path.join(pub, "index.html"); s = open(p).read()
s = re.sub(r"<title>.*?</title>", f"<title>{title}</title>", s)
s = s.replace('<link rel="icon" type="image/png" sizes="32x32"', '<link rel="icon" href="favicon.ico" sizes="any">\n    <link rel="icon" type="image/png" sizes="32x32"', 1) if 'favicon.ico' not in s else s
s = re.sub(r'(<link rel="mask-icon"[^>]*color=")[^"]*(")', rf"\g<1>{theme}\2", s)
s = re.sub(r'(<meta name="msapplication-TileColor" content=")[^"]*(")', rf"\g<1>{theme}\2", s)
s = re.sub(r'(<meta name="theme-color" content=")[^"]*(")', rf"\g<1>{theme}\2", s)
s = s.replace("We're sorry but n.eko doesn't work properly", f"We're sorry but {title} doesn't work properly")
open(p, "w").write(s)

# the mask icon is the n.eko logo; drop the reference rather than ship a wrong logo
s = open(p).read()
s = re.sub(r'\s*<link rel="mask-icon"[^>]*>', "", s)
open(p, "w").write(s)

# manifest
p = os.path.join(pub, "site.webmanifest"); m = json.load(open(p))
m["name"] = title; m["short_name"] = title
m["theme_color"] = theme; m["background_color"] = "#000000"
m["icons"] = [{"src": "android-chrome-192x192.png", "sizes": "192x192", "type": "image/png"},
              {"src": "android-chrome-512x512.png", "sizes": "512x512", "type": "image/png"}]
json.dump(m, open(p, "w"), indent=4); open(p, "a").write("\n")

# browserconfig (Windows tiles)
p = os.path.join(pub, "browserconfig.xml")
if os.path.exists(p):
    s = open(p).read(); s = re.sub(r"<TileColor>.*?</TileColor>", f"<TileColor>{theme}</TileColor>", s); open(p, "w").write(s)
print("branded:", title)
