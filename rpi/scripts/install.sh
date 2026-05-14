#!/usr/bin/env bash
# Install brew-machine on a Raspberry Pi running Pi OS Bookworm (or later).
# Run from the rpi/ directory as root:  sudo bash scripts/install.sh
set -euo pipefail

if [[ "$EUID" -ne 0 ]]; then
  echo "This script must be run as root (use sudo)." >&2
  exit 1
fi

REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
USER_NAME="${SUDO_USER:-pi}"
USER_HOME="$(eval echo ~$USER_NAME)"

echo "Installing brew-machine into $REPO_DIR for user $USER_NAME"

# 1. Make sure required groups exist for the service user
for g in gpio i2c dialout; do
  if ! id -nG "$USER_NAME" | grep -qw "$g"; then
    usermod -aG "$g" "$USER_NAME" || true
  fi
done

# 2. Install node_modules
sudo -u "$USER_NAME" bash -c "cd '$REPO_DIR' && npm install --omit=dev"

# 3. Ensure data dir exists and is owned by the service user
mkdir -p "$REPO_DIR/data"
chown -R "$USER_NAME":"$USER_NAME" "$REPO_DIR/data"

# 4. Install systemd units (substituting the real repo path / user)
for unit in brew-machine.service brew-kiosk.service; do
  sed -e "s|/home/pi/Brew-Machine-MkIV/rpi|$REPO_DIR|g" \
      -e "s|^User=pi$|User=$USER_NAME|" \
      -e "s|/home/pi|$USER_HOME|g" \
      "$REPO_DIR/systemd/$unit" > "/etc/systemd/system/$unit"
done

# 5. Make sure the kiosk launcher is executable
chmod +x "$REPO_DIR/scripts/kiosk-start.sh"

# 6. Enable 1-Wire and I2C in /boot/firmware/config.txt if not already
CFG=/boot/firmware/config.txt
[ -f "$CFG" ] || CFG=/boot/config.txt
add_overlay () {
  local line="$1"
  grep -qE "^\s*$line" "$CFG" 2>/dev/null || echo "$line" >> "$CFG"
}
add_overlay "dtoverlay=w1-gpio"
add_overlay "dtparam=i2c_arm=on"
# Hardware PWM for the boil SSR on GPIO12 (BOIL_SSR.bcm).
add_overlay "dtoverlay=pwm-2chan,pin=12,func=4,pin2=13,func2=4"

systemctl daemon-reload
echo
echo "Done. Next steps:"
echo "  1. Edit $REPO_DIR/config/pinmap.js and assign real BCM pin numbers."
echo "  2. Reboot for the dtoverlays to take effect:    sudo reboot"
echo "  3. After reboot:"
echo "     sudo systemctl enable --now brew-machine"
echo "     sudo systemctl enable --now brew-kiosk"
echo "     # or run interactively first:"
echo "     cd $REPO_DIR && npm start"
