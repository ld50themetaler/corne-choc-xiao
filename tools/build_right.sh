#!/bin/bash
set -e

WORKSPACE_DIR="/home/ld50/zmk_workspace"
OUTPUT_DIR="/home/ld50/zmk_build/artifacts"

mkdir -p "$OUTPUT_DIR"

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
docker run --rm \
  -v "$WORKSPACE_DIR:/workspace" \
  -v "$OUTPUT_DIR:/workspace/out" \
  -v "$REPO_DIR/config:/workspace/config" \
  -w /workspace/zmk \
  zmkfirmware/zmk-build-arm:stable \
  bash -c '
    set -e
    git config --global --add safe.directory "*"
    cd /workspace/zmk

    echo "=== Running west zephyr-export ==="
    west zephyr-export

    echo "=== Building chippy_right for xiao_ble//zmk with NVS Storage ==="
    west build -p -s app -d /workspace/build/right -b xiao_ble//zmk -- \
      -DZMK_CONFIG=/workspace/config \
      -DSHIELD=chippy_right -DZMK_EXTRA_MODULES="/workspace/modules/zmk-feature-cdc-acm-bootloader-trigger"

    cp /workspace/build/right/zephyr/zmk.uf2 /workspace/out/chippy_right_xiao_ble.uf2
    echo "=== Right side build finished successfully ==="
  '

cp "$OUTPUT_DIR/chippy_right_xiao_ble.uf2" "$REPO_DIR/firmware/chippy_right_xiao_ble.uf2"
if [ -d "/mnt/c/Antigravity/Corne_Xiao_Choc" ]; then
  cp "$OUTPUT_DIR/chippy_right_xiao_ble.uf2" /mnt/c/Antigravity/Corne_Xiao_Choc/chippy_right_xiao_ble.uf2
fi

echo "Checking output artifacts:"
ls -lh "$OUTPUT_DIR"
