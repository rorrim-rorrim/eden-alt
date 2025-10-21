# Caveats

## Arch Linux

- httplib AUR package is broken. Set `httplib_FORCE_BUNDLED=ON` if you have it installed.
- Eden is also available as an [AUR package](https://aur.archlinux.org/packages/eden-git). If you are unable to build, either use that or compare your process to the PKGBUILD.

## Gentoo Linux

Do not use the system sirit or xbyak packages.

## macOS

macOS is largely untested. Expect crashes, significant Vulkan issues, and other fun stuff.

## Solaris

Qt Widgets appears to be broken. For now, add `-DENABLE_QT=OFF` to your configure command. In the meantime, a Qt Quick frontend is in the works--check back later!

This is needed for some dependencies that call cc directly (tz):

```sh
echo '#!/bin/sh' >cc
echo 'gcc $@' >>cc
chmod +x cc
export PATH="$PATH:$PWD"
```

Default MESA is a bit outdated, the following environment variables should be set for a smoother experience:
```sh
export MESA_GL_VERSION_OVERRIDE=4.6
export MESA_GLSL_VERSION_OVERRIDE=460
export MESA_EXTENSION_MAX_YEAR=2025
export MESA_DEBUG=1
export MESA_VK_VERSION_OVERRIDE=1.3
# Only if nvidia/intel drm drivers cause crashes, will severely hinder performance
export LIBGL_ALWAYS_SOFTWARE=1
```

- Modify the generated ffmpeg.make (in build dir) if using multiple threads (base system `make` doesn't use `-j4`, so change for `gmake`).
- If using OpenIndiana, due to a bug in SDL2's CMake configuration, audio driver defaults to SunOS `<sys/audioio.h>`, which does not exist on OpenIndiana. Using external or bundled SDL2 may solve this.
- System OpenSSL generally does not work. Instead, use `-DYUZU_USE_BUNDLED_OPENSSL=ON` to use a bundled static OpenSSL, or build a system dependency from source.

## HaikuOS

It's recommended to do a `pkgman full-sync` before installing. See [HaikuOS: Installing applications](https://www.haiku-os.org/guides/daily-tasks/install-applications/). Sometimes the process may be interrupted by an error like "Interrupted syscall". Simply firing the command again fixes the issue. By default `g++` is included on the default installation.

GPU support is generally lacking/buggy, hence it's recommended to only install `pkgman install mesa_lavapipe`. Performance is acceptable for most homebrew applications and even some retail games.

For reasons unberknownst to any human being, `glslangValidator` will crash upon trying to be executed, the solution to this is to build `glslang` yourself. Apply the patch in `.patch/spirv-tools/0002-haikuos-fix.patch`. The main issue is `ShFinalize()` is deallocating already destroyed memory; the "fix" in question is allowing the program to just leak memory and the OS will take care of the rest.

For this reason this patch is NOT applied to default on all platforms (for obvious reasons) - instead this is a HaikuOS specific patch, apply with `git apply <absolute path to patch>` after cloning SPIRV-Tools then `make -C build` and add the resulting binary (in `build/StandAlone/glslang`) into PATH.

`cubeb_devel` will also not work, either disable cubeb or uninstall it.

## OpenBSD

After configuration, you may need to modify `externals/ffmpeg/CMakeFiles/ffmpeg-build/build.make` to use `-j$(nproc)` instead of just `-j`.

`-lc++-experimental` doesn't exist in OpenBSD but the LLVM driver still tries to link against it, to solve just symlink `ln -s /usr/lib/libc++.a /usr/lib/libc++experimental.a`.

If clang has errors, try using `g++-11`.

## FreeBSD

Eden is not currently available as a port on FreeBSD, though it is in the works. For now, the recommended method of usage is to compile it yourself.

The available OpenSSL port (3.0.17) is out-of-date, and using a bundled static library instead is recommended; to do so, add `-DYUZU_USE_BUNDLED_OPENSSL=ON` to your CMake configure command.

## NetBSD

Install `pkgin` if not already `pkg_add pkgin`, see also the general [pkgsrc guide](https://www.netbsd.org/docs/pkgsrc/using.html). For NetBSD 10.1 provide `echo 'PKG_PATH="https://cdn.netbsd.org/pub/pkgsrc/packages/NetBSD/x86_64/10.0_2025Q3/All/"' >/etc/pkg_install.conf`. If `pkgin` is taking too much time consider adding the following to `/etc/rc.conf`:
```sh
ip6addrctl=YES
ip6addrctl_policy=ipv4_prefer
```

System provides a default `g++-10` which doesn't support the current C++ codebase; install `clang-19` with `pkgin install clang-19`. Then build with `cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -B build`.

Make may error out when generating C++ headers of SPIRV shaders, hence it's recommended to use `gmake` over the default system one.

glslang is not available on NetBSD, to circumvent this simply build glslang by yourself:
```sh
pkgin python313
git clone --depth=1 https://github.com/KhronosGroup/glslang.git
cd glslang
python3.13 ./update_glslang_sources.py
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -- -j`nproc`
cmake --install build
```
