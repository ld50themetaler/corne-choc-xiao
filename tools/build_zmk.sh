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

    echo "=== Running west zephyr-export ==="
    west zephyr-export

    echo "=== Building chippy_left for xiao_ble//zmk with Studio, NVS Storage, and Prospector Module ==="
    west build -p -s app -d /workspace/build/left -b xiao_ble//zmk -S studio-rpc-usb-uart -- \
      -DSHIELD=chippy_left -DCONFIG_ZMK_STUDIO=y -DZMK_EXTRA_MODULES=/workspace/modules/prospector-zmk-module

    cp /workspace/build/left/zephyr/zmk.uf2 /workspace/out/chippy_left_xiao_ble.uf2
    echo "=== Left side build finished successfully ==="
  '

cp "$OUTPUT_DIR/chippy_left_xiao_ble.uf2" /mnt/c/Antigravity/Corne_Xiao_Choc/chippy_left_xiao_ble.uf2
echo "Checking output artifacts:"
ls -lh "$OUTPUT_DIR"
