#!/usr/bin/env python3
"""Import game covers into web/assets/gamelogos + write the Game Library manifest.

The covers shipped with OptimizeKit's "Game Library" module come from the Khadafi
optimizer (assets/gamelogos in its extraction folder). This script:

  1. downscales every cover to a web thumbnail (keeps the inventory small),
  2. writes manifest.json with the real game name and family for each key,
  3. prints a summary so a missing file can never fail silently.

Usage:
    python tools/import_gamelogos.py [source_dir]

Default source dir is %USERPROFILE%\\Downloads\\Khadafi_extract\\assets\\gamelogos.
Covers are third-party box art: they stay out of the exe (tools/embed_web.py does
not embed them) and the module degrades gracefully when they are absent.
"""
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "web", "assets", "gamelogos")
DEFAULT_SRC = os.path.join(
    os.path.expanduser("~"), "Downloads", "Khadafi_extract", "assets", "gamelogos"
)
THUMB_W = 400
QUALITY = 80

# key -> (display name, family). Families map to a suggested pack in app.js.
GAMES = {
    "apex": ("Apex Legends", "fps"),
    "arkasa": ("ARK: Survival Ascended", "sandbox"),
    "avowed": ("Avowed", "rpg"),
    "bdo": ("Black Desert Online", "mmo"),
    "beamng": ("BeamNG.drive", "racing"),
    "bf2042": ("Battlefield 2042", "fps"),
    "bg3": ("Baldur's Gate 3", "rpg"),
    "brawlhalla": ("Brawlhalla", "fighting"),
    "codaw": ("CoD: Advanced Warfare", "fps"),
    "codbo1": ("CoD: Black Ops", "fps"),
    "codbo2": ("CoD: Black Ops II", "fps"),
    "codbo3": ("CoD: Black Ops III", "fps"),
    "codbocw": ("CoD: Black Ops Cold War", "fps"),
    "codghosts": ("CoD: Ghosts", "fps"),
    "codiw": ("CoD: Infinite Warfare", "fps"),
    "codmw2_2009": ("CoD: MW2 (2009)", "fps"),
    "codmw3_2011": ("CoD: MW3 (2011)", "fps"),
    "codmwr": ("CoD: Modern Warfare Remastered", "fps"),
    "codww2": ("CoD: WWII", "fps"),
    "cs2": ("Counter-Strike 2", "fps"),
    "cyberpunk": ("Cyberpunk 2077", "rpg"),
    "darktide": ("Warhammer 40K: Darktide", "coop"),
    "dayz": ("DayZ", "sandbox"),
    "dbd": ("Dead by Daylight", "horror"),
    "deadlock": ("Deadlock", "moba"),
    "deeprock": ("Deep Rock Galactic", "coop"),
    "deltaforce": ("Delta Force", "fps"),
    "destiny2": ("Destiny 2", "mmo"),
    "diablo4": ("Diablo IV", "rpg"),
    "division2": ("The Division 2", "mmo"),
    "dota2": ("Dota 2", "moba"),
    "ds3": ("Dark Souls III", "rpg"),
    "duneawakening": ("Dune: Awakening", "mmo"),
    "eafc25": ("EA Sports FC 25", "sports"),
    "eldenring": ("Elden Ring", "rpg"),
    "enshrouded": ("Enshrouded", "coop"),
    "ets2": ("Euro Truck Simulator 2", "racing"),
    "fallguys": ("Fall Guys", "party"),
    "farcry6": ("Far Cry 6", "fps"),
    "ffxiv": ("Final Fantasy XIV", "mmo"),
    "firstdescendant": ("The First Descendant", "coop"),
    "fortnite": ("Fortnite", "br"),
    "forza5": ("Forza Horizon 5", "racing"),
    "genshin": ("Genshin Impact", "mmo"),
    "ghostts": ("Ghost of Tsushima", "rpg"),
    "greyzone": ("Gray Zone Warfare", "fps"),
    "gtav": ("Grand Theft Auto V", "openworld"),
    "helldivers2": ("Helldivers 2", "coop"),
    "hll": ("Hell Let Loose", "fps"),
    "hogwarts": ("Hogwarts Legacy", "rpg"),
    "horizonfw": ("Horizon Forbidden West", "rpg"),
    "hunt": ("Hunt: Showdown", "fps"),
    "indianajones": ("Indiana Jones & the Great Circle", "rpg"),
    "kcd2": ("Kingdom Come: Deliverance II", "rpg"),
    "lastepoch": ("Last Epoch", "mmo"),
    "lethal": ("Lethal Company", "horror"),
    "lol": ("League of Legends", "moba"),
    "lostark": ("Lost Ark", "mmo"),
    "marvelrivals": ("Marvel Rivals", "fps"),
    "mhwilds": ("Monster Hunter Wilds", "coop"),
    "minecraft": ("Minecraft", "sandbox"),
    "naraka": ("Naraka: Bladepoint", "fighting"),
    "newworld": ("New World", "mmo"),
    "nomanssky": ("No Man's Sky", "sandbox"),
    "oncehuman": ("Once Human", "coop"),
    "overwatch2": ("Overwatch 2", "fps"),
    "paladins": ("Paladins", "fps"),
    "palworld": ("Palworld", "sandbox"),
    "phasmo": ("Phasmophobia", "horror"),
    "poe": ("Path of Exile", "mmo"),
    "poe2": ("Path of Exile 2", "mmo"),
    "pubg": ("PUBG: Battlegrounds", "br"),
    "r6": ("Rainbow Six Siege", "fps"),
    "rdr2": ("Red Dead Redemption 2", "openworld"),
    "readyornot": ("Ready or Not", "fps"),
    "remnant2": ("Remnant II", "coop"),
    "roblox": ("Roblox", "sandbox"),
    "rocketleague": ("Rocket League", "sports"),
    "rust": ("Rust", "sandbox"),
    "satisfactory": ("Satisfactory", "sandbox"),
    "sekiro": ("Sekiro: Shadows Die Twice", "rpg"),
    "sf6": ("Street Fighter 6", "fighting"),
    "smite": ("SMITE", "moba"),
    "sonsoftheforest": ("Sons of the Forest", "coop"),
    "sot": ("Sea of Thieves", "coop"),
    "spacemarine2": ("Warhammer 40K: Space Marine 2", "coop"),
    "spiderman2": ("Marvel's Spider-Man 2", "rpg"),
    "spidermanr": ("Spider-Man Remastered", "rpg"),
    "splitgate2": ("Splitgate 2", "fps"),
    "squad": ("Squad", "fps"),
    "stalker2": ("S.T.A.L.K.E.R. 2", "fps"),
    "starfield": ("Starfield", "rpg"),
    "tarkov": ("Escape from Tarkov", "fps"),
    "tekken8": ("Tekken 8", "fighting"),
    "thefinals": ("The Finals", "fps"),
    "throne": ("Throne and Liberty", "mmo"),
    "tlou": ("The Last of Us Part I", "rpg"),
    "valheim": ("Valheim", "coop"),
    "valorant": ("Valorant", "fps"),
    "vrising": ("V Rising", "coop"),
    "warframe": ("Warframe", "coop"),
    "warthunder": ("War Thunder", "sports"),
    "warzone": ("Call of Duty: Warzone", "br"),
    "witcher3": ("The Witcher 3", "rpg"),
    "wot": ("World of Tanks", "sports"),
    "wukong": ("Black Myth: Wukong", "rpg"),
}

# family -> pack id used by app.js PACKS (esport | lowlatency | streaming | laptop | cleanboot)
FAMILY_PACK = {
    "fps": "esport",
    "br": "esport",
    "fighting": "esport",
    "sports": "esport",
    "racing": "esport",
    "rpg": "esport",
    "openworld": "esport",
    "moba": "lowlatency",
    "mmo": "lowlatency",
    "coop": "lowlatency",
    "horror": "lowlatency",
    "party": "cleanboot",
    "sandbox": "cleanboot",
}


def main() -> int:
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    if not os.path.isdir(src):
        print(f"[gamelogos] source folder not found: {src}", file=sys.stderr)
        return 1
    try:
        from PIL import Image
    except ImportError:
        print("[gamelogos] Pillow required: python -m pip install pillow", file=sys.stderr)
        return 1

    os.makedirs(OUT_DIR, exist_ok=True)
    games, missing, kept = [], [], 0

    for key, (name, family) in sorted(GAMES.items()):
        hit = None
        for ext in (".jpg", ".jpeg", ".png", ".webp"):
            cand = os.path.join(src, key + ext)
            if os.path.exists(cand):
                hit = cand
                break
        if not hit:
            missing.append(key)
            continue
        im = Image.open(hit).convert("RGB")
        if im.width > THUMB_W:
            im = im.resize((THUMB_W, round(im.height * THUMB_W / im.width)), Image.LANCZOS)
        dest = os.path.join(OUT_DIR, key + ".jpg")
        im.save(dest, "JPEG", quality=QUALITY, optimize=True, progressive=True)
        kept += 1
        games.append({
            "key": key, "name": name, "family": family,
            "pack": FAMILY_PACK.get(family, "esport"), "cover": key + ".jpg",
        })

    manifest = {
        "source": "Khadafi extract — assets/gamelogos (thumbnails, downscaled to %dpx)" % THUMB_W,
        "note": "Suggested profile comes from the game's family, never from a template: the app shows which tweaks it will apply before it touches anything.",
        "games": games,
    }
    # encoding pinned: Windows would otherwise write the em dash as cp1252 and the browser would fail to parse it
    with open(os.path.join(OUT_DIR, "manifest.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(manifest, f, ensure_ascii=False, indent=1)

    size = sum(os.path.getsize(os.path.join(OUT_DIR, g["cover"])) for g in games)
    print(f"[gamelogos] {kept} covers -> {os.path.relpath(OUT_DIR, ROOT)} ({size / 1024:.0f} KB)")
    if missing:
        print(f"[gamelogos] no cover for: {', '.join(missing)}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main())
