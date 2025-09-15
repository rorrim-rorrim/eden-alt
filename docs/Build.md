# Building Eden

> [!WARNING]
> This guide is intended for developers ONLY. If you are not a developer or packager, you are unlikely to receive support.

This is a full-fledged guide to build Eden on all supported platforms.

## Dependencies
First, you must [install some dependencies](Deps.md).

## Clone
Next, you will want to clone Eden via the terminal:

```sh
git clone https://git.eden-emu.dev/eden-emu/eden.git
cd eden
```

Or use Qt Creator (Create Project -> Import Project -> Git Clone).

## Android

Android has a completely different build process than other platforms. See its [dedicated page](build/Android.md).

## Initial Configuration

If the configure phase fails, see the `Troubleshooting` section below. Usually, as long as you followed the dependencies guide, the defaults *should* successfully configure and build.

### Qt Creator

This is the recommended GUI method for Linux, macOS, and Windows.

<details>
<summary>Click to Open</summary>

> [!WARNING]
> On MSYS2, to use Qt Creator you are recommended to *also* install Qt from the online installer, ensuring to select the "MinGW" version.

Open the CMakeLists.txt file in your cloned directory via File -> Open File or Project (Ctrl+O), if you didn't clone Eden via the project import tool.

Select your desired "kit" (usually, the default is okay). RelWithDebInfo or Release is recommended:

![Qt Creator kits](img/creator-1.png)

Hit "Configure Project", then wait for CMake to finish configuring (may take a while on Windows).

</details>

### Command Line

This is recommended for *BSD, Solaris, Linux, and MSYS2. MSVC is possible, but not recommended.

<details>
<summary>Click to Open</summary>

Note that CMake must be in your PATH, and you must be in the cloned Eden directory. On Windows, you must also set up a Visual C++ development environment. This can be done by running `C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvars64.bat` in the same terminal.

Recommended generators:
- MSYS2: `MSYS Makefiles`
- MSVC: Install **[ninja](https://ninja-build.org/)** and use `Ninja`, OR use `Visual Studio 17 2022`
- macOS: `Ninja` (preferred) or `Xcode`
- Others: `Ninja` (preferred) or `UNIX Makefiles`

BUILD_TYPE should usually be `Release` or `RelWithDebInfo` (debug symbols--compiled executable will be large). If you are using a debugger and annoyed with stuff getting optimized out, try `Debug`.

Also see the [Options](Options.md) page for additional CMake options.

```sh
cmake -S . -B build -G "GENERATOR" -DCMAKE_BUILD_TYPE=<BUILD_TYPE> -DYUZU_TESTS=OFF
```

If you are on Windows and prefer to use Clang:

```sh
cmake -S . -B build -G "GENERATOR" -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl
```

<details>

### CLion

TODO

## Troubleshooting

If your initial configure failed:
- *Carefully* re-read the [dependencies guide](Deps.md)
- Clear the CPM cache (`.cache/cpm`) and CMake cache (`<build directory>/CMakeCache.txt`)
- Evaluate the error and find any related settings
- See the [CPM docs](CPM.md) to see if you may need to forcefully bundle any packages

Otherwise, feel free to ask for help in Revolt or Discord.

## Caveats

Many platforms have quirks, bugs, and other fun stuff that may cause issues when building OR running. See the [Caveats page](Caveats.md) before continuing.

## Building & Running

### Qt Creator

Simply hit Ctrl+B, or the "hammer" icon in the bottom left. To run, hit the "play" icon, or Ctrl+R.

### Command Line

If you are not on Windows, and are using the `UNIX Makefiles` generator, you must also add `-j$(nproc)` to this command.

```
cmake --build build
```

Your compiled executable will be in:
- `build/bin/eden.exe` for Windows,
- `build/bin/eden.app/Contents/MacOS/eden` for macOS,
- and `build/bin/eden` for others.

## Scripts

Some platforms have convenience scripts provided for building.

- **[Linux](scripts/Linux.md)**
- **[Windows](scripts/Windows.md)**

macOS scripts will come soon. Maybe.