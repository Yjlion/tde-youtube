# Runtime environment used to take the screenshots in screenshots/.
# Build from the repository root:
#   docker build -t tde-youtube-build tools/
#   docker build -t tde-youtube-shots -f tools/screenshots/Dockerfile tools/screenshots
FROM tde-youtube-build
ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y --no-install-recommends \
      tdebase-trinity tdebase-tdeio-plugins-trinity khelpcenter-trinity twin-trinity \
      xvfb x11-utils xdotool imagemagick mpv python3 fonts-dejavu-core fonts-noto-core \
      dbus-x11 xauth x11-xserver-utils \
 && rm -rf /var/lib/apt/lists/* \
 && curl -fsSL -o /usr/local/bin/yt-dlp https://github.com/yt-dlp/yt-dlp/releases/latest/download/yt-dlp \
 && chmod +x /usr/local/bin/yt-dlp
