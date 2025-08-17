# Building for OpenBSD

```sh
pkg_add -u
pkg_add cmake nasm git boost unzip--iconv autoconf-2.72p0 bash ffmpeg glslang g++-11.2.0p18 gmake
git --recursive https://git.eden-emu.dev/eden-emu/eden
```

Select g++-11.2. The compiler can then be invoked via `ec++`.

```sh
cmake -DDYNARMIC_USE_PRECOMPILED_HEADERS=OFF -DCMAKE_BUILD_TYPE=Debug -DENABLE_QT=OFF -DENABLE_OPENSSL=OFF -DENABLE_WEB_SERVICE=OFF -B /usr/obj/eden
```

- Modify `externals/ffmpeg/CMakeFiles/ffmpeg-build/build.make` to use `-j$(nproc)` instead of just `-j`.
