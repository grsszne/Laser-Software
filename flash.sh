#!/usr/bin/env bash
# Build and flash Laser PHY-A (STM32F723) using STM32CubeIDE's ARM toolchain
# and STM32CubeProgrammer's command-line interface.
# Place this script in the root of Laser-Software.
set -Eeuo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$REPO_DIR/phy-a-bringup/PHY_A_Bringup"
BUILD_DIR="$PROJECT_DIR/Release"
FIRMWARE="$BUILD_DIR/PHY_A_Bringup.elf"

TOOLCHAIN_DIR="/Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.14.3.rel1.macosaarch64_1.0.0.202602081740/tools/bin"
PROGRAMMER_DIR="/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/Resources/bin"

if [[ ! -d "$BUILD_DIR" ]]; then
  echo "Missing build directory: $BUILD_DIR" >&2
  exit 1
fi
if [[ ! -x "$TOOLCHAIN_DIR/arm-none-eabi-gcc" ]]; then
  echo "Missing ARM compiler: $TOOLCHAIN_DIR/arm-none-eabi-gcc" >&2
  exit 1
fi
if [[ ! -x "$PROGRAMMER_DIR/STM32_Programmer_CLI" ]]; then
  echo "Missing STM32 programmer CLI: $PROGRAMMER_DIR/STM32_Programmer_CLI" >&2
  exit 1
fi

export PATH="$TOOLCHAIN_DIR:$PATH"

echo "==> Building PHY_A_Bringup (Release)"
make -C "$BUILD_DIR" -j4 all

if [[ ! -s "$FIRMWARE" ]]; then
  echo "Build completed, but firmware ELF is missing/empty: $FIRMWARE" >&2
  exit 1
fi

echo "==> Flashing and verifying via ST-Link / SWD"
(
  cd "$PROGRAMMER_DIR"
  ./STM32_Programmer_CLI \
    -c port=SWD mode=NORMAL reset=SWrst freq=400 \
    -w "$FIRMWARE" -v -rst
)

echo "==> Build, flash, verify, reset complete"

