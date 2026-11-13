# World TV — itch.io listing kit (v1.0.0)

## PAGE META (do not paste)

| Field | Value |
|---|---|
| Title | World TV |
| URL slug | worldtv |
| Short tagline (form, ~120 chars) | Live TV from 180 countries on one couch. 10,288 channels, mirror failover, full gamepad support. |
| Classification | Downloadable |
| Genre | Other → Tool |
| Platforms | Linux, Windows, Android |
| Pricing | $0 or donate (minimum 0 — do not set a paid minimum, no payment setup needed) |
| Cover | itch/cover_630x500.png |
| Banner (page header) | itch/banner_1200x480.png |
| Icon | itch/icon_512.png |
| Community | Disabled (or Comments only) |
| Tags (max 10, keywords only) | live-tv, iptv, streaming, television, couch, steam-deck, gamepad, channel-surfing, media-player, world |

### Screenshot upload order (itch/shots/)
1. `01_browse_grid.png` — the 10,288-channel grid with country sidebar
2. `02_live_player_redbull.png` — full-screen player mid-stream
3. `03_back_to_grid.png` — back on the couch

### Upload files (itch/builds/1.0.0/)
| File | "This file will be played in the browser" | Download page |
|---|---|---|
| `worldtv-1.0.0-linux-x86_64.AppImage` | no | default |
| `worldtv-1.0.0-linux-x86_64-portable.tar.gz` | no | default |
| `worldtv-1.0.0-windows-x86_64.zip` | no | default |
| `worldtv-1.0.0-android-arm64.apk` | no | default |
| `SHA256SUMS` | no | hidden or default |

---

## PASTE-READY BODY (everything below this line)

[b]You flip through the dial. The dial is the entire planet.[/b]

World TV is channel surfing without the cable package. One window, ten thousand live channels, every continent on the remote. Tonight you can watch the news in Tashkent, a cooking show in Seoul, and a cattle auction somewhere in Brazil, and you never once stood up from the couch.

[b]Click run. That is the whole tutorial.[/b]

The channel grid is there before your coffee cools. Pick a flag, pick a channel, you are watching TV. There is no account, no sign-in, no algorithm deciding what your evening looks like. The grid does not rank you, rate you, or come with a subscription form.

[b]The couch remote, fully supported.[/b]

Made for the Steam Deck and its 1280x800 couch screen first. Arrows move, Return plays, Escape backs out. A real gamepad works the same way through SDL2, and a plain keyboard has been in the family for years.

[b]Streams die. The mirror does not.[/b]

Free public streams come and go, so every channel ships with its full mirror list and the player keeps them one keypress away. If a feed freezes mid-sentence, press [b]N[/b] and the same channel continues on its next mirror. That is the trick that makes ten thousand channels usable instead of a museum of dead links.

[b]Find your flavor.[/b]

180 countries. 29 categories, from news to kids to weather to channels that exist purely to relax you. Search by name, keep a favorites row, and the channels you actually watched wait in a recents row like they lived here all along.

[b]Where the channels come from.[/b]

The catalog is built from the iptv-org open database (github.com/iptv-org), filtered to playable, non-NSFW streams. World TV does not host a single stream itself, it just organizes the pile and plays it. There is a refresh button inside the app, because the pile changes every day. Public streams can disappear or be geo-blocked at any time; the mirror list is the answer to that, not a promise that all ten thousand are awake tonight.

[b]The details:[/b]
[list]
[*]10,288 playable channels, ~16,000 feeds, 180 countries
[*]Per-mirror failover in the player (N = next feed)
[*]Search, favorites, recents
[*]Country and category browsing with flags
[*]In-app catalog refresh, no rebuild required
[*]Gamepad support via SDL2, full keyboard control
[*]Embedded UI, no source tree or config files needed
[/list]

[b]Controls[/b]
[list]
[*]Arrows / left stick: move
[*]Return / A: play
[*]Escape / B: back
[*]N: next mirror (player)
[*]F: favorite
[*]/ : search
[*]Space: pause (player)
[/list]

[b]Downloads[/b]
[list]
[*][b]Linux:[/b] AppImage or fuse-free portable tar.gz
[*][b]Windows:[/b] portable zip, no installer
[*][b]Android:[/b] arm64 APK
[/list]

[b]Release notes[/b]
[list]
[*][b]1.0.0[/b] — first public release. Catalog from iptv-org, mirror failover, gamepad support, embedded UI, Linux/Windows/Android builds.
[/list]

[b]License[/b]

Free for personal, non-commercial use. Companies and any commercial use require a license from the copyright holder.

Copyright (c) 2026 sinjas. Released under the PolyForm Noncommercial License 1.0.0: https://polyformproject.org/licenses/noncommercial/1.0.0
