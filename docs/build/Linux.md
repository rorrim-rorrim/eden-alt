# ⚠️ This guide is for developers ONLY! Support will be provided to developers ONLY.

## 📋 Index:

* [Minimal Dependencies](#minimal-dependencies)
* [Cloning Eden with Git](#cloning-eden-with-git)
* [Arch / Manjaro](#arch-manjaro)
* [Ubuntu / Linux Mint / Debian](#ubuntu-linux-mint-debian)
* [Fedora](#fedora)
* [Building with Scripts](#building-with-scripts)
* [Running without Installing](#running-without-installing)

---

## Minimal Dependencies

You'll need to download and install the following to build Eden:

* [GCC](https://gcc.gnu.org/) v11+ (for C++20 support) & misc
* If GCC 12 is installed, [Clang](https://clang.llvm.org/) v14+ is required for compiling
* [CMake](https://www.cmake.org/) 3.22+
* [Git](https://git-scm.com/) for version control

The following are handled by Eden's externals:

* [FFmpeg](https://ffmpeg.org/) (should use `-DYUZU_USE_EXTERNAL_FFMPEG=ON`)
* [SDL2](https://www.libsdl.org/download-2.0.php) 2.0.18+ (should use `-DYUZU_USE_EXTERNAL_SDL2=ON`)
* [opus](https://opus-codec.org/downloads/) 1.3+

All other dependencies will be downloaded and built by [CPM](https://github.com/cpm-cmake/CPM.cmake/) if `YUZU_USE_CPM` is on, but will always use system dependencies if available:

* [Boost](https://www.boost.org/users/download/) 1.79.0+
* [Catch2](https://github.com/catchorg/Catch2) 2.13.7 - 2.13.9
* [fmt](https://fmt.dev/) 8.0.1+
* [lz4](http://www.lz4.org) 1.8+
* [nlohmann\_json](https://github.com/nlohmann/json) 3.8+
* [OpenSSL](https://www.openssl.org/source/) 1.1.1+
* [ZLIB](https://www.zlib.net/) 1.2+
* [zstd](https://facebook.github.io/zstd/) 1.5+
* [enet](http://enet.bespin.org/) 1.3+
* [cubeb](https://github.com/mozilla/cubeb)
* [SimpleIni](https://github.com/brofield/simpleini)

Certain other dependencies (httplib, jwt, sirit, etc.) will be fetched by CPM regardless. System packages *can* be used for these libraries but this is generally not recommended.

Dependencies are listed here as commands that can be copied/pasted. Inspect them before running.

## Cloning Eden with Git

**Master:**

```bash
git clone https://git.eden-emu.dev/eden-emu/eden
cd eden
```

## Arch / Manjaro

```sh
sudo pacman -Syu --needed base-devel boost catch2 cmake enet ffmpeg fmt git glslang libzip lz4 mbedtls ninja nlohmann-json openssl opus qt6-base qt6-multimedia sdl2 zlib zstd zip unzip
```
* Building with QT Web Engine requires `qt6-webengine` as well.
* Proper Wayland support requires `qt6-wayland`
* GCC 11 or later is required.

## Ubuntu / Linux Mint / Debian

```sh
sudo apt-get install autoconf cmake g++ gcc git glslang-tools libasound2 libboost-context-dev libglu1-mesa-dev libhidapi-dev libpulse-dev libtool libudev-dev libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-render-util0 libxcb-xinerama0 libxcb-xkb1 libxext-dev libxkbcommon-x11-0 mesa-common-dev nasm ninja-build qt6-base-private-dev libmbedtls-dev catch2 libfmt-dev liblz4-dev nlohmann-json3-dev libzstd-dev libssl-dev libavfilter-dev libavcodec-dev libswscale-dev pkg-config zlib1g-dev libva-dev libvdpau-dev qt6-tools-dev libzydis-dev zydis-tools libzycore-dev
```
* Ubuntu 22.04, Linux Mint 20, or Debian 12 or later is required.
* To enable QT Web Engine, add `-DYUZU_USE_QT_WEB_ENGINE=ON` when running CMake.

```sh
# Make build dir and enter
mkdir build && cd build

# Generate CMake Makefiles
cmake .. -GNinja -DCMAKE_C_COMPILER=gcc-11 -DCMAKE_CXX_COMPILER=g++-11

# Build
ninja
```

## Fedora

```sh
sudo dnf install autoconf ccache cmake fmt-devel gcc{,-c++} glslang hidapi-devel json-devel libtool libusb1-devel libzstd-devel lz4-devel nasm ninja-build openssl-devel pulseaudio-libs-devel qt6-linguist qt6-qtbase{-private,}-devel qt6-qtwebengine-devel qt6-qtmultimedia-devel speexdsp-devel wayland-devel zlib-devel ffmpeg-devel libXext-devel
```
* Force system libraries via CMake arguments:
  * SDL2: `-DYUZU_USE_BUNDLED_SDL2=OFF -DYUZU_USE_EXTERNAL_SDL2=OFF`
  * FFmpeg: `-DYUZU_USE_EXTERNAL_FFMPEG=OFF`
* [RPM Fusion](https://rpmfusion.org/) is required for `ffmpeg-devel`
* Fedora 32 or later is required.
* Fedora 36+ users with GCC 12 need Clang and should configure CMake with:
```sh
# Generate CMake Makefiles (for Clang compiler)
cmake .. -GNinja -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang
```

## Building Eden in Release Mode (Optimised)

```bash
# Make build dir and enter
mkdir build && cd build

# Generate CMake Makefiles
cmake .. -GNinja -DYUZU_TESTS=OFF

# Build
ninja

# Install!
sudo ninja install
```

* Enable Discord Rich Presence:
```bash
# ...

# Generate CMake Makefiles (with Discord Rich Presence)
cmake .. -G "MSYS Makefiles" -DYUZU_TESTS=OFF -DUSE_DISCORD_PRESENCE=ON

# ...
```

* If ninja fails with `undefined reference to symbol 'spvOptimizerOptionsCreate'`, add `-DYUZU_USE_EXTERNAL_VULKAN_SPIRV_TOOLS=OFF`
* Optionally use `cmake-gui ..` to adjust options (e.g., disable Qt GUI)

## Building Eden in Debug Mode (Slow)

```bash
# Make build dir and enter
mkdir build && cd build

# Generate CMake Makefiles
cmake .. -GNinja -DCMAKE_BUILD_TYPE=Debug -DYUZU_TESTS=OFF

# Build
ninja
```

## Building with Debug Symbols

```bash
# Make build dir and enter

# Generate CMake Makefiles
cmake .. -GNinja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DYUZU -DYUZU_TESTS=OFF

# Build
ninja
```

## Building with Scripts

* Provided script: `.ci/linux/build.sh`
* Must specify arch target, e.g.: `.ci/linux/build.sh amd64`
* Valid targets:
  * `native`: Optimize to your native host architecture
  * `legacy`: x86\_64 generic, only needed for CPUs older than 2013 or so
  * `amd64`: x86\_64-v3, for CPUs newer than 2013 or so
  * `steamdeck` / `zen2`: For Steam Deck or Zen >= 2 AMD CPUs (untested on Intel)
  * `rog-ally` / `allyx` / `zen4`: For ROG Ally X or Zen >= 4 AMD CPUs (untested on Intel)
  * `aarch64`: For armv8-a CPUs, older than mid-2021 or so
  * `armv9`: For armv9-a CPUs, newer than mid-2021 or so
* Extra CMake flags go after the arch target.

### Environment Variables

* `NPROC`: Number of compilation threads (default: all cores)
* `TARGET`: Set `appimage` to disable standalone `eden-cli` and `eden-room`
* `BUILD_TYPE`: Build type (default: `Release`)

Boolean flags (set `true` to enable, `false` to disable):

* `DEVEL` (default `FALSE`): Disable Qt update checker

* `USE_WEBENGINE` (default `FALSE`): Enable Qt WebEngine

* `USE_MULTIMEDIA` (default `TRUE`): Enable Qt Multimedia

* AppImage packaging script: `.ci/linux/package.sh`

  * Accepts same arch targets as build script
  * Use `DEVEL=true` to rename app to `Eden Nightly`

## Running without Installing

After building, binaries `eden` and `eden-cmd` will be in `build/bin/`.

```bash
# Build Dir
cd build/bin/

# SDL2 build
./eden-cmd

# Qt build
./eden
```
