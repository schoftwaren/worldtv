#!/bin/sh
# Refresh the channel catalog: iptv-org api dumps -> prepdata.py -> app/catalog.json
# Run this when you want newer channel data, then rebuild (the json is embedded).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
mkdir -p "$HERE/../data"
echo "fetching iptv-org dumps..."
curl -sL -m 120 "https://iptv-org.github.io/api/channels.json" -o "$HERE/../data/channels.json"
curl -sL -m 120 "https://iptv-org.github.io/api/streams.json" -o "$HERE/../data/streams.json"
python3 "$HERE/prepdata.py"
echo "done — rebuild worldtv to embed the new catalog (or use the in-app refresh button, which fetches without a rebuild)."
