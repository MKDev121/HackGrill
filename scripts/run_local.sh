#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=========================================================="
echo " Launching Local VoIP Translation Engine"
echo "=========================================================="

# Cleanup handler on exit
cleanup() {
    echo -e "\n[Shutdown] Stopping all background processes..."
    if [ -n "$TRANS_PID" ]; then kill "$TRANS_PID" 2>/dev/null || true; fi
    if [ -n "$PROC_PID" ]; then kill "$PROC_PID" 2>/dev/null || true; fi
    exit 0
}
trap cleanup SIGINT SIGTERM EXIT

# 1. Start C++ Transmission App
echo "--> Starting C++ Transmission App on UDP :5004..."
if [ -f "$ROOT_DIR/transmission/build/transmission_app" ]; then
    "$ROOT_DIR/transmission/build/transmission_app" --local-port 5004 --remote-port 5006 &
    TRANS_PID=$!
elif [ -f "$ROOT_DIR/transmission/build/Release/transmission_app.exe" ]; then
    "$ROOT_DIR/transmission/build/Release/transmission_app.exe" --local-port 5004 --remote-port 5006 &
    TRANS_PID=$!
elif [ -f "$ROOT_DIR/transmission/build/Debug/transmission_app.exe" ]; then
    "$ROOT_DIR/transmission/build/Debug/transmission_app.exe" --local-port 5004 --remote-port 5006 &
    TRANS_PID=$!
else
    echo "Warning: transmission_app binary not found in build/ directory. Please run build_all.sh first."
fi

sleep 1

# 2. Start Python Processing App
echo "--> Starting Python Sarvam Edge Processing App..."
cd "$ROOT_DIR/processing"
if [ -f "venv/bin/activate" ]; then
    source venv/bin/activate
elif [ -f "venv/Scripts/activate" ]; then
    source venv/Scripts/activate
fi

python src/main.py --source-lang "hi-IN" --target-lang "en-IN" --chunk-ms 300 &
PROC_PID=$!

sleep 1

# 3. Start Flutter Desktop App
echo "--> Starting Flutter Desktop Application..."
cd "$ROOT_DIR/frontend"
flutter run -d windows || flutter run -d linux || flutter run -d macos

wait
