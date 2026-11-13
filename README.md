# worldtv

Live world TV on the couch: a 10-foot Qt6/QML channel browser for the
Steam Deck (1280x800), fully drivable with a keyboard or gamepad. The
channel catalog comes from the iptv-org open database, filtered to
playable, non-NSFW streams, with per-mirror failover in the player.

## Features

- Browse ~4500 playable channels by country or category
- Search, favorites, and a recents row
- Player overlay with mirror failover: if a stream dies, press N for
  the next mirror of the same channel
- Gamepad support via SDL2 (arrow keys, Return, Escape map naturally)
- In-app catalog refresh (fetches iptv-org without a rebuild)
- Embedded QML: release binaries carry their UI, no source tree needed

## Platforms

| Platform | Build | Artifact |
|---|---|---|
| Linux x86_64 | CI (aqtinstall Qt 6.10) | AppImage + fuse-free portable tar.gz |
| Windows x86_64 | CI (MSYS2 UCRT64) | portable zip |
| macOS arm64 | CI (aqtinstall Qt 6.10) | dmg |
| Android arm64 | local (source-Qt cross build) | APK, attached to releases |

Tag a release with `vX.Y.Z` and the CI pipeline builds, tests, smokes
the packaged artifacts, and attaches them to the GitHub release with
SHA256SUMS. Dispatch the workflow manually to get artifacts without a
tag.

## Building from source

Needs Qt 6.9+, CMake 3.24+, ninja, libmpv, and SDL2 (desktop only).

    cmake -B build -G Ninja -DWORLDTV_EMBED_QML=ON
    cmake --build build --parallel

The channel catalog (`app/catalog.json`) is embedded at build time.
Refresh it with `tools/fetch_data.sh`, or use the in-app refresh
button.

## Data source and content

Channel and stream data comes from the iptv-org project
(github.com/iptv-org). worldtv only filters (playable + non-NSFW) and
plays it. Public streams can disappear or be geo-restricted at any
time; the mirror list per channel is the answer to that.

## License

Free for personal, non-commercial use. Companies and any commercial
use require a license from the copyright holder.
