#!/usr/bin/env bash
# Launch Chromium in kiosk mode pointing at the local Brew Machine UI.
# Disables the screensaver and hides the mouse cursor while idle.
set -euo pipefail

URL="${KIOSK_URL:-http://localhost:8080}"

# Stop the screen from blanking
if command -v xset >/dev/null 2>&1; then
  xset s noblank
  xset s off
  xset -dpms
fi

# Hide the cursor when idle
if command -v unclutter >/dev/null 2>&1; then
  unclutter -idle 0.1 -root &
fi

# Wait for the backend to be up before opening the kiosk
for i in {1..30}; do
  if curl -s -o /dev/null -w "%{http_code}" "$URL" | grep -q "200"; then break; fi
  sleep 1
done

# Find Chromium (Pi OS calls it `chromium-browser`; Debian uses `chromium`)
BROWSER=$(command -v chromium-browser || command -v chromium || true)
if [ -z "$BROWSER" ]; then
  echo "No Chromium found. Install chromium-browser." >&2
  exit 1
fi

exec "$BROWSER" \
  --kiosk \
  --noerrdialogs --disable-infobars --no-first-run \
  --disable-translate --disable-features=TranslateUI \
  --check-for-update-interval=31536000 \
  --window-size=800,480 \
  --window-position=0,0 \
  --start-fullscreen \
  --overscroll-history-navigation=0 \
  --disable-pinch \
  "$URL"
