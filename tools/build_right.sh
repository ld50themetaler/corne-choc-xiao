#!/bin/bash
set -e

WORKSPACE_DIR="/home/ld50/zmk_workspace"
OUTPUT_DIR="/home/ld50/zmk_build/artifacts"

mkdir -p "$OUTPUT_DIR"

docker run --rm \
  -v "$WORKSPACE_DIR:/workspace" \
  -v "$OUTPUT_DIR:/workspace/out" \
  -w /workspace/zmk \
  zmkfirmware/zmk-build-arm:stable \
  bash -c '
    set -e
    git config --global --add safe.directory "*"
    cd /workspace/zmk

    echo "=== Building chippy_right for xiao_ble//zmk with NVS Storage ==="
    west build -p -s app -d /workspace/build/right -b xiao_ble//zmk -- \
      -DSHIELD=chippy_right

    cp /workspace/build/right/zephyr/zmk.uf2 /workspace/out/chippy_right_xiao_ble.uf2
    echo "=== Right side build finished successfully ==="
  '

cp "$OUTPUT_DIR/chippy_right_xiao_ble.uf2" /mnt/c/Antigravity/Corne_Xiao_Choc/chippy_right_xiao_ble.uf2
echo "Checking output artifacts:"
ls -lh "$OUTPUT_DIR"
