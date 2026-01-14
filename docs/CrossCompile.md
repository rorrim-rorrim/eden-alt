# Cross compiling

General guide for cross compiling.

## Gentoo

Gentoo's cross-compilation setup is relatively easy, provided you're already familiar with portage.

### Crossdev

First, emerge crossdev via `sudo emerge -a sys-devel/crossdev`.

Now, set up the environment depending on the target architecture; e.g.

```sh
sudo crossdev powerpc64le
sudo crossdev aarch64
```

### QEMU

Installing a qemu user setup is recommended for testing. To do so, you will need the relevant USE flags:

```sh
app-emulation/qemu static-user qemu_user_targets_ppc64le qemu_user_targets_aarch64
```

Note that to use cross-emerged libraries, you will need to tell qemu where the sysroot is. You can do this with an alias:

```sh
alias qemu-ppc64le="qemu-ppc64le -L /usr/powerpc64le-unknown-linux-gnu"
alias qemu-aarch64="qemu-aarch64 -L /usr/aarch64-unknown-linux-gnu"
```

### Dependencies

Some packages have broken USE flags on other architectures; you'll also need to set up python targets. In `/usr/<target>-unknown-linux-gnu/etc/portage/package.use`:

```sh
>=net-misc/curl-8.16.0-r1 ssl

*/* PYTHON_TARGETS: python3_13 PYTHON_SINGLE_TARGET: python3_13
*/* pam

sys-apps/util-linux pam su
app-shells/bash -readline
>=dev-libs/libpcre2-10.47 unicode
>=x11-libs/libxkbcommon-1.12.3 X
>=sys-libs/zlib-1.3.1-r1 minizip
>=app-alternatives/gpg-1-r3 ssl
>=app-crypt/gnupg-2.5.13-r2 ssl

dev-libs/* -introspection
media-libs/harfbuzz -introspection
dev-libs/quazip -qt5 qt6
```

Dependencies should be about the same [as normal Gentoo](./Deps.md), but removing gamemode and renderdoc is recommended. Keep in mind that when emerging, you want to use `emerge-<target>-unknown-linux-gnu`, e.g. `emerge-powerpc64le-unknown-linux-gnu`.

Enable GURU in the cross environment (as root):

```sh
mkdir -p /usr/powerpc64le-unknown-linux-gnu/etc/portage/repos.conf
cat << EOF > /usr/powerpc64le-unknown-linux-gnu/etc/portage/repos.conf/guru.conf
[guru]
location = /var/db/repos/guru
auto-sync = no
priority = 1
EOF
```

Now emerge your dependencies:

```sh
sudo emerge-powerpc64le-unknown-linux-gnu -aU app-arch/lz4 app-arch/zstd app-arch/unzip \
    dev-libs/libfmt dev-libs/libusb dev-libs/mcl dev-libs/sirit dev-libs/oaknut \
    dev-libs/unordered_dense dev-libs/boost dev-libs/openssl dev-libs/discord-rpc \
    dev-util/spirv-tools dev-util/spirv-headers dev-util/vulkan-headers \
    dev-util/vulkan-utility-libraries dev-util/glslang \
    media-libs/libva media-libs/opus media-video/ffmpeg \
    media-libs/VulkanMemoryAllocator media-libs/libsdl2 media-libs/cubeb \
    net-libs/enet net-libs/mbedtls \
    sys-libs/zlib \
    dev-cpp/nlohmann_json dev-cpp/simpleini dev-cpp/cpp-httplib dev-cpp/cpp-jwt dev-cpp/catch \
    net-wireless/wireless-tools \
    dev-qt/qtbase:6 dev-libs/quazip \
    virtual/pkgconfig
```

### Building

A toolchain is provided in `CMakeModules/GentooCross.cmake`. To use it:

```sh
cmake -S . -B build/ppc64 -DCMAKE_TOOLCHAIN_FILE=CMakeModules/GentooCross.cmake -G Ninja -DCROSS_TARGET=powerpc64le -DENABLE_OPENGL=OFF
```

Now build as normal:

```sh
cmake --build build/ppc64 -j$(nproc)
```

### Alternatively

Only emerge the absolute necessities:

```sh
sudo emerge-powerpc64le-unknown-linux-gnu -aU media-video/ffmpeg media-libs/libsdl2 dev-qt/qtbase:6
```

Then set `YUZU_USE_CPM=ON`:

```sh
cmake -S . -B build/ppc64 -DCMAKE_TOOLCHAIN_FILE=CMakeModules/GentooCross.cmake -G Ninja -DCROSS_TARGET=powerpc64le -DENABLE_OPENGL=OFF -DYUZU_USE_CPM=ON
```

## ARM64

### Debian ARM64

A painless guide for cross compilation (or to test NCE) from a x86_64 system without polluting your main.

- Install QEMU: `sudo pkg install qemu`
- Download Debian 13: `wget https://cdimage.debian.org/debian-cd/current/arm64/iso-cd/debian-13.0.0-arm64-netinst.iso`
- Create a system disk: `qemu-img create -f qcow2 debian-13-arm64-ci.qcow2 30G`
- Run the VM: `qemu-system-aarch64 -M virt -m 2G -cpu max -bios /usr/local/share/qemu/edk2-aarch64-code.fd -drive if=none,file=debian-13.0.0-arm64-netinst.iso,format=raw,id=cdrom -device scsi-cd,drive=cdrom -drive if=none,file=debian-13-arm64-ci.qcow2,id=hd0,format=qcow2 -device virtio-blk-device,drive=hd0 -device virtio-gpu-pci -device usb-ehci -device usb-kbd -device intel-hda -device hda-output -nic user,model=virtio-net-pci`

## PowerPC

This is a guide for FreeBSD users mainly.

Now you got a PowerPC sysroot - quickly decompress it somewhere, say `/home/user/opt/powerpc64le`. Create a toolchain file, for example `powerpc64le-toolchain.cmake`; always [consult the manual](https://man.freebsd.org/cgi/man.cgi?query=cmake-toolchains&sektion=7&manpath=FreeBSD+13.2-RELEASE+and+Ports).

There is a script to automatically do all of this under `./tools/setup-cross-sysroot.sh`.

Remember to add `-mabi=elfv1` to `CFLAGS`/`CXXFLAGS` otherwise the program will crash.

Specify:

- `YUZU_USE_CPM`: Set this to `ON` so packages can be found and built if your sysroot doesn't have them.
- `YUZU_USE_EXTERNAL_FFMPEG`: Set this to `ON` as well.

Then run using a program such as QEMU to emulate userland syscalls:

```sh
cmake --build build-ppc64-pc-freebsd -t dynarmic_tests -- -j8 && qemu-ppc64-static -L $HOME/opt/ppc64-freebsd/sysroot ./build-ppc64-pc-freebsd/bin/dynarmic_tests
```
