# Cross compiling

General guide for cross compiling.

## Debian ARM64

A painless guide for cross compilation (or to test NCE) from a x86_64 system without polluting your main.

- Install QEMU: `sudo pkg install qemu`
- Download Debian 13: `wget https://cdimage.debian.org/debian-cd/current/arm64/iso-cd/debian-13.0.0-arm64-netinst.iso`
- Create a system disk: `qemu-img create -f qcow2 debian-13-arm64-ci.qcow2 30G`
- Run the VM: `qemu-system-aarch64 -M virt -m 2G -cpu max -bios /usr/local/share/qemu/edk2-aarch64-code.fd -drive if=none,file=debian-13.0.0-arm64-netinst.iso,format=raw,id=cdrom -device scsi-cd,drive=cdrom -drive if=none,file=debian-13-arm64-ci.qcow2,id=hd0,format=qcow2 -device virtio-blk-device,drive=hd0 -device virtio-gpu-pci -device usb-ehci -device usb-kbd -device intel-hda -device hda-output -nic user,model=virtio-net-pci`

## PowerPC

This is a guide for FreeBSD users mainly.

Now you got a PowerPC sysroot - quickly decompress it somewhere, say `/home/user/opt/powerpc64le`. Create a toolchain file, for example `powerpc64le-toolchain.cmake`; always [consult the manual](https://man.freebsd.org/cgi/man.cgi?query=cmake-toolchains&sektion=7&manpath=FreeBSD+13.2-RELEASE+and+Ports).

There is a script to automatically do all of this under `./tools/setup-cross-sysroot.sh`

Specify:
- `YUZU_USE_CPM`: Set this to `ON` so packages can be found and built if your sysroot doesn't have them.
- `YUZU_USE_EXTERNAL_FFMPEG`: Set this to `ON` as well.

Then run using a program such as QEMU to emulate userland syscalls:

```sh
cmake --build build-ppc64-pc-freebsd -t dynarmic_tests -- -j8 && qemu-ppc64-static -L $HOME/opt/ppc64-freebsd/sysroot ./build-ppc64-pc-freebsd/bin/dynarmic_tests
```
