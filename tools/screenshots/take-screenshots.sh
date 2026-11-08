#!/bin/bash
#
# Builds tde-youtube and takes the screenshots in screenshots/ and doc/en/.
# Runs inside the tools/screenshots Docker image:
#
#   docker build -t tde-youtube-build tools/
#   docker build -t tde-youtube-shots -f tools/screenshots/Dockerfile tools/screenshots
#   docker run --rm -v "$PWD":/src tde-youtube-shots /src/tools/screenshots/take-screenshots.sh
#
# NOTE: YouTube refuses to hand out video streams and descriptions to
# servers in data centres ("Sign in to confirm you're not a bot"). So that
# the screenshots can be made on such machines, this script:
#   * points mpv at a wrapper that plays a local copy of the Creative Commons
#     film "Big Buck Bunny" (CC-BY 3.0, Blender Foundation, downloaded from
#     download.blender.org) instead of streaming from YouTube, and
#   * makes the description request wait instead of failing.
# Search results, thumbnails and everything else are real.

set -e
SRC=${SRC:-/src}
OUT_SHOTS=$SRC/screenshots
OUT_DOC=$SRC/doc/en
WORK=/tmp/shots
mkdir -p "$WORK" "$OUT_SHOTS"

# ---- build & install -------------------------------------------------------
mkdir -p /tmp/build && cd /tmp/build
cmake "$SRC" -DCMAKE_INSTALL_PREFIX=/opt/trinity -DBUILD_DOC=ON >/dev/null
make -j"$(nproc)" >/dev/null
make install >/dev/null

# ---- demo video and wrappers -------------------------------------------------
if [ ! -f "$WORK/bbb.m4v" ]; then
    curl -fsSL -o "$WORK/bbb.zip" https://download.blender.org/peach/bigbuckbunny_movies/BigBuckBunny_640x360.m4v.zip
    python3 -c "import zipfile,sys; z=zipfile.ZipFile(sys.argv[1]); n=[i for i in z.namelist() if i.endswith('.m4v')][0]; open(sys.argv[2],'wb').write(z.read(n))" "$WORK/bbb.zip" "$WORK/bbb.m4v"
    rm -f "$WORK/bbb.zip"
fi

mkdir -p "$WORK/bin"
cat > "$WORK/bin/mpv" <<WRAP
#!/bin/bash
# Drop the YouTube URL, play the local Big Buck Bunny copy instead.
args=()
for a in "\$@"; do
    case "\$a" in --|http://*|https://*|--ytdl*|--script-opts=*) ;; *) args+=("\$a") ;; esac
done
exec /usr/bin/mpv "\${args[@]}" --ytdl=no --start=128 --loop-file=inf "$WORK/bbb.m4v"
WRAP
cat > "$WORK/bin/yt-dlp" <<WRAP
#!/bin/bash
# Searches go to the real yt-dlp; description requests just wait.
for a in "\$@"; do [ "\$a" = "--skip-download" ] && exec sleep 600; done
exec /usr/local/bin/yt-dlp "\$@"
WRAP
chmod +x "$WORK/bin/mpv" "$WORK/bin/yt-dlp"
# The wrappers shadow the real programs, so the settings show the defaults.
export PATH="$WORK/bin:$PATH"

# ---- session ------------------------------------------------------------------
export SCREEN_SIZE=1280x800x24
source "$SRC/tools/screenshots/session.sh"

mkdir -p ~/.trinity/share/config
cat > ~/.trinity/share/config/tde-youtuberc <<CFG
[General]
ResultsPerPage=20

[Layout]
HorizontalSplitter=440,840
VerticalSplitter=520,190
Initialised=true

[MainWindow]
Height 800=760
Width 1280=1200
CFG
rm -f ~/.trinity/share/config/tde-youtube-libraryrc

app() { dcop "$(dcop | grep '^tde-youtube' | head -1)" tde-youtube-mainwindow "$@"; }
shot() { sleep "${2:-1}"; import -window root "$OUT_SHOTS/$1.png"; echo "took $1.png"; }
win() { xdotool search --sync --onlyvisible --name "$1" | head -1; }
key() { xdotool key --delay 120 "$@"; }

placewin() {
    local w; w=$(win "TDE YouTube")
    xdotool windowmove "$w" 40 10 windowsize "$w" 1200 770
    xdotool windowactivate --sync "$w" 2>/dev/null || true
}

clickrow() {   # clickrow N  -> click the N-th (0 based) row of the visible list
    local w; w=$(win "TDE YouTube")
    eval "$(xdotool getwindowgeometry --shell "$w")"
    xdotool mousemove $((X + 220)) $((Y + 110 + 38 + $1 * 76)) click 1
}

# 1. search results
tde-youtube --search "blender open movie" >/tmp/app.log 2>&1 &
sleep 4; placewin; sleep 10
shot main-window

# 2. favourites: add three videos, show the tab
for i in 0 2 4; do clickrow $i; sleep 0.5; app activateAction video_favourite >/dev/null; done
app activateAction view_favourites >/dev/null
clickrow 0
shot favourites 2
app activateAction view_search >/dev/null

# 3. context menu
w=$(win "TDE YouTube"); eval "$(xdotool getwindowgeometry --shell "$w")"
xdotool mousemove $((X + 220)) $((Y + 110 + 38 + 76)) click 1 click 3
shot context-menu 1.5
key Escape

# 4. playing (search for the film, play the Blender Studio upload)
key ctrl+f; xdotool type --delay 30 "big buck bunny"; key Return
sleep 10
clickrow 1          # the upload by the Blender channel
key Return
shot playing 12
key ctrl+shift+f
shot fullscreen 4
key Escape
sleep 2

# 5. settings dialog (all pages)
app activateAction options_configure >/dev/null &
sleep 3
d=$(win "Configure")
xdotool windowmove "$d" 300 150
shot settings-playback 1
eval "$(xdotool getwindowgeometry --shell "$d")"
xdotool mousemove $((X + 52)) $((Y + 105)) click 1
shot settings-programs 1
xdotool mousemove $((X + 52)) $((Y + 170)) click 1
shot settings-history 1
key Escape
sleep 1

# 6. handbook in KHelpCenter
pkill -x tde-youtube || true
khelpcenter help:/tde-youtube/index.html >/tmp/khc.log 2>&1 &
sleep 8
h=$(win "Help Center" || true)
[ -n "$h" ] && xdotool windowmove "$h" 40 10 windowsize "$h" 1200 770
shot handbook 4
pkill khelpcenter || true

# copies for the handbook
cp "$OUT_SHOTS/main-window.png" "$OUT_DOC/main-window.png"
cp "$OUT_SHOTS/playing.png" "$OUT_DOC/playing.png"
cp "$OUT_SHOTS/favourites.png" "$OUT_DOC/favourites.png"
cp "$OUT_SHOTS/settings-playback.png" "$OUT_DOC/settings-playback.png"
cp "$OUT_SHOTS/settings-programs.png" "$OUT_DOC/settings-programs.png"
cp "$OUT_SHOTS/settings-history.png" "$OUT_DOC/settings-history.png"
chown -R --reference="$SRC" "$OUT_SHOTS" "$OUT_DOC" 2>/dev/null || true
echo done
