#!/usr/bin/env bash
# VeloTick marker
set -e

echo "[VeloTick] Preparing system deps (OpenSSL, zlib)..."
if command -v apt >/dev/null 2>&1; then
  sudo apt update -y
  sudo apt install -y libssl-dev zlib1g-dev
else
  echo "[VeloTick] apt not found; please ensure OpenSSL and zlib are available."
fi

echo "[VeloTick] Building C++ gateway..."
mkdir -p build
cd build
cmake ../cpp -DPYBIND11_FINDPYTHON=ON -DCMAKE_BUILD_TYPE=Release
cmake --build . -j

echo "[VeloTick] Launching gateway..."
./velotick_gateway || (echo "[VeloTick] On Windows, run build\\velotick_gateway.exe" && exit 0)

echo "VeloTick build done. 接下来可以 ./run.sh 启动行情网关。"
# VeloTick marker