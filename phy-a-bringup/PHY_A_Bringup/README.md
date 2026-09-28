# PHY_A_Bringup editor setup

Open `Core/Src/main.c` in Neovim. The checked-in `compile_flags.txt` describes
the CubeIDE Debug configuration: STM32F723xx, Cortex-M7, FPv5 single-precision
FPU, hard-float ABI, HAL/CMSIS include paths, and Debug preprocessor definitions.
These flags configure clangd analysis; CubeIDE still owns building and flashing.

Clangd must be able to find `arm-none-eabi-gcc` on PATH, and its startup
`--query-driver` allowlist must include that compiler's absolute path so it can
discover the ARM standard library headers. The local Neovim config discovers
the compiler bundled with STM32CubeIDE on macOS automatically.

When changing the MCU, include directories, or preprocessor definitions in
CubeIDE, update `compile_flags.txt` to match `.cproject`. For projects with
per-file build flags, generate a `compile_commands.json` from the actual build
and place it here; clangd prefers it over `compile_flags.txt`.

Do not enable Vim modelines for generated sources: CubeMX's `ex: printf(...)`
comment near the end of `main.c` is mistaken for a modeline.
