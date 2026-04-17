#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
FRONTEND_DIR="$SCRIPT_DIR/frontend"
INSTALL_DIR="/usr/local/share/hms-colada"
STATIC_DIR="$INSTALL_DIR/static/browser"
BINARY="/usr/local/bin/hms_colada"
SERVICE="hms-colada"

echo "============================================"
echo "  hms-colada build & deploy"
echo "============================================"
echo ""

# ── Step 1: Build C++ backend ────────────────────────────────────────
echo "[1/5] Building C++ backend..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
echo ""

# ── Step 2: Run tests ────────────────────────────────────────────────
echo "[2/5] Running tests..."
./tests/run_tests
TESTS_EXIT=$?
if [ $TESTS_EXIT -ne 0 ]; then
    echo "TESTS FAILED — aborting deploy"
    exit 1
fi
echo ""

# ── Step 3: Build Angular frontend ───────────────────────────────────
echo "[3/5] Building Angular frontend..."
cd "$FRONTEND_DIR"
if [ ! -d "node_modules" ]; then
    echo "  Installing npm dependencies..."
    npm install
fi
npx ng build --configuration production
echo ""

# ── Step 4: Install files ────────────────────────────────────────────
echo "[4/5] Installing..."

# Stop service if running
sudo systemctl stop "$SERVICE" 2>/dev/null || true

# Copy binary
sudo cp "$BUILD_DIR/hms_colada" "$BINARY"
echo "  Binary: $BINARY"

# Copy frontend static files
sudo mkdir -p "$STATIC_DIR"
sudo cp -r "$FRONTEND_DIR/dist/browser/"* "$STATIC_DIR/"
echo "  Static: $STATIC_DIR"

# Install systemd service (template — credentials from config.json or env)
sudo cp "$SCRIPT_DIR/hms-colada.service" /etc/systemd/system/
sudo systemctl daemon-reload
echo "  Service: /etc/systemd/system/hms-colada.service"

# ── Step 5: Start service ────────────────────────────────────────────
echo "[5/5] Starting service..."

# Update config to point static_dir to installed location
CONFIG_FILE="$HOME/.hms-colada/config.json"
if [ -f "$CONFIG_FILE" ]; then
    python3 -c "
import json
with open('$CONFIG_FILE') as f:
    c = json.load(f)
c['static_dir'] = '$STATIC_DIR'
with open('$CONFIG_FILE', 'w') as f:
    json.dump(c, f, indent=2)
print('  Config updated: static_dir=$STATIC_DIR')
"
fi

sudo systemctl enable "$SERVICE"
sudo systemctl start "$SERVICE"

sleep 2

# Verify
if sudo systemctl is-active --quiet "$SERVICE"; then
    echo ""
    echo "============================================"
    echo "  hms-colada deployed successfully!"
    echo "============================================"
    echo ""
    echo "  Binary:  $BINARY"
    echo "  Static:  $STATIC_DIR"
    echo "  Config:  $CONFIG_FILE"
    echo "  Service: sudo systemctl status $SERVICE"
    echo "  Logs:    sudo journalctl -u $SERVICE -f"
    echo "  Web UI:  http://$(hostname -I | awk '{print $1}'):8889"
    echo ""
else
    echo ""
    echo "WARNING: Service failed to start. Check logs:"
    echo "  sudo journalctl -u $SERVICE -n 20"
    exit 1
fi
