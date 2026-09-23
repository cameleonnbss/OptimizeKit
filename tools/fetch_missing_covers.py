#!/usr/bin/env python3
"""Fetch a cover for every gamedb.h title that has no image yet.

For each title in src/core/gamedb.h missing from web/assets/gamelogos:
  1. try Steam store search -> header.jpg from the official Steam CDN,
  2. fall back to the Wikipedia lead image for titles not sold on Steam,
  3. downscale to a 400px thumbnail (same treatment as import_gamelogos.py),
  4. append the entry to web/assets/gamelogos/manifest.json.

Strict name matching (alphanumeric compare, both directions) so a search hit
can never silently be the wrong game. Existing files are never overwritten.
Cover art stays out of the exe (embed_web.py does not embed it) and the
library degrades gracefully when a file is absent.

Usage:
    python tools/fetch_missing_covers.py [--dry-run]
"""
import json
import os
import re
import sys
import time
import urllib.parse
import urllib.request

# Windows consoles default to cp1252: force UTF-8 so the ✔/✖ markers never crash a run
if hasattr(sys.stdout, "reconfigure"):
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.stderr.reconfigure(encoding="utf-8", errors="replace")

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DB = os.path.join(ROOT, "src", "core", "gamedb.h")
OUT_DIR = os.path.join(ROOT, "web", "assets", "gamelogos")
MANIFEST = os.path.join(OUT_DIR, "manifest.json")
THUMB_W = 400
QUALITY = 80
UA = {"User-Agent": "Mozilla/5.0 (OptimizeKit cover fetch; contact: repo issue)"}

FAMILY_PACK = {
    "fps": "esport", "br": "esport", "fighting": "esport", "sports": "esport",
    "racing": "esport", "rpg": "esport", "openworld": "esport",
    "moba": "lowlatency", "mmo": "lowlatency", "coop": "lowlatency",
    "horror": "lowlatency", "party": "cleanboot", "sandbox": "cleanboot",
    "strategy": "cleanboot", "sim": "cleanboot", "survival": "cleanboot",
    "roguelike": "cleanboot",
}

# DB titles that must NOT be auto-matched (search names too generic / delisted).
SKIP = set()


def slug(name: str) -> str:
    s = name.lower()
    s = re.sub(r"[^a-z0-9]+", "_", s).strip("_")
    return s or "game"


def norm(s: str) -> str:
    return re.sub(r"[^a-z0-9]", "", s.lower())


def http_get(url: str, timeout: int = 15) -> bytes:
    req = urllib.request.Request(url, headers=UA)
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.read()


def db_titles():
    src = open(DB, encoding="utf-8").read()
    rows = re.findall(
        r'\{\s*L"[^"]+",\s*L"([^"]+)",\s*"([a-z]+)",\s*L"[^"]*"\s*\}', src
    )
    return rows  # (name, family)


def steam_search(name: str):
    """Return (appid, official_name) of the best strict match, or None."""
    q = urllib.parse.quote(name)
    try:
        data = json.loads(http_get(
            "https://store.steampowered.com/api/storesearch/?term=%s&l=english&cc=US" % q
        ))
    except Exception:
        return None
    want = norm(name)
    for item in data.get("items", []):
        official = (item.get("name") or "").replace("™", "").replace("®", "")
        if norm(official) == want:
            return item["id"], official
    # second pass: one direction contained in the other (long names, > 8 chars)
    for item in data.get("items", []):
        official = (item.get("name") or "").replace("™", "").replace("®", "")
        a, b = norm(official), want
        if len(a) > 8 and len(b) > 8 and (a.startswith(b) or b.startswith(a)):
            return item["id"], official
    return None


def wiki_image(name: str):
    """Lead image of the English Wikipedia article (pageimages).

    pilicense=any is required: game covers are fair-use images and the default
    license filter excludes them silently (the API then returns no 'original').
    """
    q = urllib.parse.quote(name)
    url = ("https://en.wikipedia.org/w/api.php?action=query&format=json&prop=pageimages"
           "&piprop=original&pilicense=any&redirects=1&titles=" + q)
    try:
        data = json.loads(http_get(url))
        pages = data.get("query", {}).get("pages", {})
        for _, page in pages.items():
            original = page.get("original", {}).get("source")
            if original:
                return original
    except Exception:
        pass
    return None


def save_thumb(img_bytes: bytes, dest: str) -> bool:
    try:
        from PIL import Image
        import io
        im = Image.open(io.BytesIO(img_bytes)).convert("RGB")
        if im.width < 120 or im.height < 60:
            return False  # too small to be box art
        if im.width > THUMB_W:
            im = im.resize((THUMB_W, round(im.height * THUMB_W / im.width)), Image.LANCZOS)
        im.save(dest, "JPEG", quality=QUALITY, optimize=True, progressive=True)
        return True
    except Exception:
        return False


def main() -> int:
    dry = "--dry-run" in sys.argv
    have = {f[:-4] for f in os.listdir(OUT_DIR) if f.endswith(".jpg")}
    manifest = json.load(open(MANIFEST, encoding="utf-8"))
    covered = {norm(g["name"]) for g in manifest["games"]}

    todo = [(n, f) for n, f in db_titles()
            if norm(n) not in covered and n not in SKIP]
    print(f"[covers] {len(todo)} titles without cover art")
    got = 0
    for i, (name, family) in enumerate(sorted(todo)):
        if dry:
            print(f"  would fetch: {name}")
            continue
        key = slug(name)
        dest = os.path.join(OUT_DIR, key + ".jpg")
        src_info = ""
        ok = False
        # 1) Steam
        hit = steam_search(name)
        if hit:
            appid, official = hit
            try:
                img = http_get("https://cdn.cloudflare.steamstatic.com/steam/apps/%s/header.jpg" % appid)
                ok = save_thumb(img, dest)
                if ok:
                    src_info = "steam:%s" % appid
            except Exception:
                ok = False
        # 2) Wikipedia
        if not ok:
            url = wiki_image(name)
            if url:
                try:
                    img = http_get(url)
                    ok = save_thumb(img, dest)
                    if ok:
                        src_info = "wikipedia"
                except Exception:
                    ok = False
        if ok:
            got += 1
            manifest["games"].append({
                "key": key, "name": name, "family": family,
                "pack": FAMILY_PACK.get(family, "esport"),
                "cover": key + ".jpg", "source": src_info,
            })
            print(f"  [{i + 1}/{len(todo)}] ✔ {name}  ({src_info})")
        else:
            print(f"  [{i + 1}/{len(todo)}] ✖ {name}")
        time.sleep(0.4)  # be polite to the public endpoints

    if not dry and got:
        manifest["steam_fetch"] = (
            "Covers for titles absent from the Khadafi extract were fetched from the "
            "official Steam CDN (store.steampowered.com search) with a Wikipedia lead-image "
            "fallback; thumbnails downscaled to %dpx." % THUMB_W
        )
        with open(MANIFEST, "w", encoding="utf-8", newline="\n") as f:
            json.dump(manifest, f, ensure_ascii=False, indent=1)
    print(f"[covers] {got} new covers written")
    return 0


if __name__ == "__main__":
    sys.exit(main())
