# itch upload checklist — World TV

Everything for upload day lives in this folder. Builds on disk are NOT
uploaded until you click through the itch form (Cloudflare blocks bots;
the page form is yours).

## One-time page setup
1. Create the project: Title `World TV`, slug `worldtv` (sticks early — pick it once).
2. Cover: `cover_630x500.png`. Header/banner art: `banner_1200x480.png`.
3. Upload the files from `builds/1.0.0/` — all "Downloadable", NONE played
   in the browser.
4. Paste the body from `ITCH_LISTING.md` (below the PAGE META marker) into
   the description. BBCode renders on itch.
5. Screenshots in upload order: `shots/01…03` (see ITCH_LISTING.md).
6. Pricing: `$0 or donate`, minimum 0. Do not raise the minimum without
   wiring payments — downloads get blocked by a yellow banner otherwise.
7. Tags from PAGE META (max 10, no genre/platform words).

## Regenerating the kit
    # 1. fresh screenshots (browse + live play + back), then art:
    cd ~/Work/worldtv
    mkdir -p /tmp/wtv-kit
    WORLDTV_WINDOW_SIZE=1920x1080 WORLDTV_AUTOPLAY=RedBullTV.at \
      WORLDTV_SHOT_DELAY=14000 \
      xvfb-run -a -s "-screen 0 1920x1080x24" \
      ./build-embed/worldtv --screenshot /tmp/wtv-kit/wtv
    python3 tools/make_itch_art.py

    # 2. builds: CI attaches them to the GitHub release on tag; pull into
    #    itch/builds/<ver>/ (AppImage, portable tar.gz, win zip, apk,
    #    SHA256SUMS) + add the INSTALL.txt. For 1.0.1 the Android APK was
    #    also built locally (source-Qt cross build, cmake/swap_libcpp.sh)
    #    and carries the manifest version stamp.

## Version bump notes
- Window size: the QML root MUST declare width/height (Main.qml); the env
  override goes through QQmlProperty::write. Plain QWindow::resize() gets
  clobbered by QML auto-sizing, and xcb re-shrinks the window shortly after
  show — the shot harness re-asserts size before each grab for that reason.
- Verify the binary before shooting: `./build-embed/worldtv --smoke`
  must print `SMOKE RESULT: OK` and `WTV_UI_OK channels=10288`.
