#!/usr/bin/env python3
"""Preprocess iptv-org api dumps into worldtv's embedded catalog.json.

Input  (downloaded by fetch_data.sh): data/channels.json, data/streams.json
Output: app/catalog.json — only channels that have a playable stream,
NSFW filtered out, best stream per channel, deduped by (name, country).
"""
import json, os, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "data")
OUT = os.path.join(HERE, "..", "app", "catalog.json")

Q_RANK = {"2160p": 7, "1080p": 6, "720p": 5, "480p": 4, "360p": 3, "270p": 2, "180p": 1}

COUNTRY_NAMES = json.load(open(os.path.join(HERE, "countries_min.json")))

def flag_emoji(cc):
    """US -> U+1F1FA U+1F1F8 (regional indicators)."""
    if cc == "UK":  # iptv-org legacy code; flags only exist for ISO GB
        cc = "GB"
    if not cc or len(cc) != 2 or not cc.isalpha() or not cc.isascii():
        return "\N{GLOBE WITH MERIDIANS}"
    return "".join(chr(0x1F1E6 + ord(k) - ord("A")) for k in cc.upper())

def main():
    channels = json.load(open(os.path.join(DATA, "channels.json")))
    streams = json.load(open(os.path.join(DATA, "streams.json")))

    by_id = {c["id"]: c for c in channels}

    # 1. attach ALL streams per channel (primary first, best quality);
    # dead mirrors are common in IPTV, so the player falls down the list.
    streams_by = {}  # channel_id -> [stream dicts]
    for s in streams:
        cid = s.get("channel")
        if not cid or cid not in by_id:
            continue
        ch = by_id[cid]
        if ch.get("is_nsfw") or "xxx" in (ch.get("categories") or []):
            continue
        streams_by.setdefault(cid, []).append(s)

    # 2. build entries, dedupe by (name, country) keeping the richer set
    entries = {}
    for cid, sl in streams_by.items():
        ch = by_id[cid]
        name = (ch.get("name") or "").strip()
        if not name:
            continue
        cc = (ch.get("country") or "").upper()
        seen = set()
        urls = []
        for s in sorted(sl, key=lambda s: -Q_RANK.get(s.get("quality") or "", 0)):
            u = s.get("url")
            if not u or u in seen:
                continue
            seen.add(u)
            urls.append({"u": u, "q": s.get("quality") or "",
                         "ua": s.get("user_agent") or "",
                         "ref": s.get("referrer") or ""})
        if not urls:
            continue
        key = (name.casefold(), cc)
        cand = {
            "id": cid,
            "name": name,
            "cc": cc,
            "cats": [c for c in (ch.get("categories") or [])][:2],
            "logo": ch.get("logo") or "",
            "urls": urls,
        }
        cur = entries.get(key)
        if cur is None or len(cand["urls"]) > len(cur["urls"]):
            entries[key] = cand

    chans = sorted(entries.values(), key=lambda c: c["name"].casefold())

    # 3. country buckets (by playable count)
    counts = {}
    for c in chans:
        counts[c["cc"]] = counts.get(c["cc"], 0) + 1
    countries = []
    for cc, n in counts.items():
        countries.append({
            "cc": cc if cc else "INT",
            "name": COUNTRY_NAMES.get(cc, "International" if not cc else cc),
            "flag": flag_emoji(cc),
            "n": n,
        })
    countries.sort(key=lambda x: (-x["n"], x["name"]))

    used_cats = {}
    for c in chans:
        for cat in c["cats"]:
            used_cats[cat] = used_cats.get(cat, 0) + 1

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    catalog = {"countries": countries, "cats": used_cats, "channels": chans}
    with open(OUT, "w") as f:
        json.dump(catalog, f, ensure_ascii=False, separators=(",", ":"))
    print(f"catalog.json: {len(chans)} channels, {len(countries)} countries, "
          f"{len(used_cats)} categories, {os.path.getsize(OUT)//1024} KB")

if __name__ == "__main__":
    sys.exit(main())
