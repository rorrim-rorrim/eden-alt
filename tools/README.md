# Tools

Tools for Eden and other subprojects.

## Third-Party

- [CPMUtil Scripts](./cpm)

## Eden

- `find-unused-strings.pl`: Find unused strings (for Android XML files).
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
- `unused-strings.sh`: Finds unused strings in Android `strings.xml` files.
    * It's recommended to run this after almost any Android change; this operation is relatively fast.

## Translations

- [Translation Scripts](./translations)