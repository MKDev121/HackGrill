#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

echo "=========================================================="
echo " Building VoIP Translation App (All Components)"
echo "=========================================================="

# 1. Build C++ Transmission App
echo "--> [1/3] Building C++ Transmission App..."
cd "$ROOT_DIR/transmission"
mkdir -p build && cd build
cmake ..
cmake --build . --config Release

# 2. Setup Python Processing Environment
echo "--> [2/3] Setting up Python Processing Environment..."
cd "$ROOT_DIR/processing"
if [ ! -d "venv" ]; then
    python3 -m venv venv || python -m venv venv
fi

if [ -f "venv/bin/activate" ]; then
    source venv/bin/activate
elif [ -f "venv/Scripts/activate" ]; then
    source venv/Scripts/activate
fi

pip install --upgrade pip
pip install -r requirements.txt

# 3. Build Flutter Desktop Frontend
echo "--> [3/3] Getting Flutter Dependencies..."
cd "$ROOT_DIR/frontend"
flutter pub get

echo "=========================================================="
echo " All components built successfully!"
echo " Run './scripts/run_local.sh' to launch."
echo "=========================================================="
