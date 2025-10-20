# Tools

Tools for Eden and other subprojects.

## Third-Party

- [CPMUtil Scripts](./cpm)

## Eden

- `generate_converters.py`: Generates converters for given formats of textures (C++ helper).
- `svc_generator.py`: Generates the files `src/core/hle/kernel/svc.cpp` and `src/core/hle/kernel/svc.h` based off prototypes.
- `shellcheck.sh`: Ensure POSIX compliance (and syntax sanity) for all tools in this directory and subdirectories.
- `llvmpipe-run.sh`: Sets environment variables needed to run any command (or Eden) with llvmpipe.
- `optimize-assets.sh`: Optimize PNG assets with OptiPng.
- `update-cpm.sh`: Updates CPM.cmake to the latest version.
- `update-icons.sh`: Rebuild all icons (macOS, Windows, bitmaps) based on the master SVG file (`dist/dev.eden_emu.eden.svg`)
    * Also optimizes the master SVG
    * Requires: `png2icns` (libicns), ImageMagick, [`svgo`](https://github.com/svg/svgo)
- `dtrace-tool.sh`
- `lanczos_gen.c`
- `clang-format.sh`: Runs `clang-format` on the entire codebase.
    * Requires: clang

## Android
It's recommended to run these scritps after almost any Android change, as they are relatively fast and important both for APK bloat and CI.

- `unused-strings.sh`: Finds unused strings in `strings.xml` files.
- `stale-translations.sh`: Finds translated strings that aren't present in the source `strings.xml` file.

## Translations

- [Translation Scripts](./translations)