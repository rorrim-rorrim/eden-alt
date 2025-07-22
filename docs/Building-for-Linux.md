### Dependencies

You'll need to download and install the following to build Eden:

  * [GCC](https://gcc.gnu.org/) v11+ (for C++20 support) & misc
  * If GCC 12 is installed, [Clang](https://clang.llvm.org/) v14+ is required for compiling
  * [CMake](https://www.cmake.org/) 3.22+

The following are handled by Eden's externals:

  * [FFmpeg](https://ffmpeg.org/)
  * [SDL2](https://www.libsdl.org/download-2.0.php) 2.0.18+
  * [opus](https://opus-codec.org/downloads/)
  
All other dependencies will be downloaded by [vcpkg](https://vcpkg.io/) if needed:

  * [Boost](https://www.boost.org/users/download/) 1.79.0+
  * [Catch2](https://github.com/catchorg/Catch2) 2.13.7 - 2.13.9
  * [fmt](https://fmt.dev/) 8.0.1+
  * [lz4](http://www.lz4.org) 1.8+
  * [nlohmann_json](https://github.com/nlohmann/json) 3.8+
  * [OpenSSL](https://www.openssl.org/source/)
  * [ZLIB](https://www.zlib.net/) 1.2+
  * [zstd](https://facebook.github.io/zstd/) 1.5+

If an ARM64 build is intended, export `VCPKG_FORCE_SYSTEM_BINARIES=1`.

Dependencies are listed here as commands that can be copied/pasted. Of course, they should be inspected before being run.

- Arch / Manjaro:
  - `sudo pacman -Syu --needed base-devel boost catch2 cmake ffmpeg fmt git glslang libzip lz4 mbedtls ninja nlohmann-json openssl opus qt6-base qt6-multimedia sdl2 zlib zstd zip unzip`
  - Building with QT Web Engine requires `qt6-webengine` as well.
  - Proper wayland support requires `qt6-wayland`
  - GCC 11 or later is required.
- Ubuntu / Linux Mint / Debian:
  - `sudo apt-get install autoconf cmake g++-11 gcc-11 git glslang-tools libasound2 libboost-context-dev libglu1-mesa-dev libhidapi-dev libpulse-dev libtool libudev-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxext-dev libxkbcommon-x11-0 mesa-common-dev nasm ninja-build qtbase6-dev qtbase6-private-dev qtwebengine6-dev qtmultimedia6-dev libmbedtls-dev catch2 libfmt-dev liblz4-dev nlohmann-json3-dev libzstd-dev libssl-dev libavfilter-dev libavcodec-dev libswscale-dev`
  - Ubuntu 22.04, Linux Mint 20, or Debian 12 or later is required.
  -  Users need to manually specify building with QT Web Engine enabled.  This is done using the parameter `-DYUZU_USE_QT_WEB_ENGINE=ON` when running CMake. 
  - Users need to manually specify building with GCC 11. This can be done by adding the parameters `-DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11` when running CMake. i.e.
  - Users need to manually disable building SDL2 from externals if they intend to use the version provided by their system by adding the parameters `-DYUZU_USE_EXTERNAL_SDL2=OFF`

```
git submodule update --init --recursive
cmake .. -GNinja -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11
```

- Fedora:
  - `sudo dnf install autoconf ccache cmake fmt-devel gcc{,-c++} glslang hidapi-devel json-devel libtool libusb1-devel libzstd-devel lz4-devel nasm ninja-build openssl-devel pulseaudio-libs-devel qt5-linguist qt5-qtbase{-private,}-devel qt5-qtwebengine-devel qt5-qtmultimedia-devel speexdsp-devel wayland-devel zlib-devel ffmpeg-devel libXext-devel`
  - Fedora 32 or later is required.
  - Due to GCC 12, Fedora 36 or later users need to install `clang`, and configure CMake to use it via `-DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang`
  - CMake arguments to force system libraries:
    - SDL2: `-DYUZU_USE_BUNDLED_SDL2=OFF -DYUZU_USE_EXTERNAL_SDL2=OFF`
    - FFmpeg: `-DYUZU_USE_EXTERNAL_FFMPEG=OFF`
  - [RPM Fusion](https://rpmfusion.org/) (free) is required to install `ffmpeg-devel`

### Cloning Eden with Git

**Master:**

  ```bash
  git clone --recursive https://git.eden-emu.dev/eden-emu/eden
  cd eden
  ```

The `--recursive` option automatically clones the required Git submodules.

### Building Eden in Release Mode (Optimised)

If you need to run ctests, you can disable `-DYUZU_TESTS=OFF` and install Catch2.

```bash
mkdir build && cd build
cmake .. -GNinja -DYUZU_TESTS=OFF
ninja
sudo ninja install 
```
You may also want to include support for Discord Rich Presence by adding `-DUSE_DISCORD_PRESENCE=ON` after `cmake ..`

`-DYUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS=OFF` might be needed if ninja command failed with `undefined reference to symbol 'spvOptimizerOptionsCreate`, reason currently unknown

Optionally, you can use `cmake-gui ..` to adjust various options (e.g. disable the Qt GUI).

### Building Eden in Debug Mode (Slow)

```bash
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DYUZU_TESTS=OFF
ninja
```

### Building with debug symbols

```bash
mkdir build && cd build
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU -DYUZU_TESTS=OFF
ninja
```

### Running without installing

After building, the binaries `eden` and `eden-cmd` (depending on your build options) will end up in `build/bin/`.

  ```bash
  # SDL
  cd build/bin/
  ./eden-cmd

  # Qt
  cd build/bin/
  ./eden
  ```
