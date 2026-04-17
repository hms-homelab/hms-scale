#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BUILD_DIR="$SCRIPT_DIR/build"
FRONTEND_DIR="$SCRIPT_DIR/frontend"
STATIC_DIR="$BUILD_DIR/static/browser"

echo "=== hms-colada build & deploy ==="

# Build C++ backend
echo "[1/4] Building C++ backend..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)

echo "[2/4] Running tests..."
./tests/run_tests
echo ""

# Build Angular frontend
if [ -d "$FRONTEND_DIR/node_modules" ]; then
    echo "[3/4] Building Angular frontend..."
    cd "$FRONTEND_DIR"
    npx ng build --configuration production --output-path "$STATIC_DIR"
else
    echo "[3/4] Skipping frontend (run 'cd frontend && npm install' first)"
fi

# Deploy
echo "[4/4] Deploying..."
cd "$BUILD_DIR"
sudo systemctl stop hms-colada 2>/dev/null || true
sudo cp hms_colada /usr/local/bin/
sudo cp "$SCRIPT_DIR/hms-colada.service" /etc/systemd/system/
sudo systemctl daemon-reload
sudo systemctl enable hms-colada
sudo systemctl start hms-colada

echo ""
echo "hms-colada deployed successfully!"
echo "  Binary: /usr/local/bin/hms_colada"
echo "  Config: ~/.hms-colada/config.json"
echo "  Status: sudo systemctl status hms-colada"
echo "  Logs:   sudo journalctl -u hms-colada -f"
echo "  Web UI: http://localhost:8889"
