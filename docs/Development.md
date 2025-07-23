# Development

* **Windows**: [Windows Building Guide](./docs/build/Windows.md)
* **Linux**: [Linux Building Guide](./docs/build/Linux.md)
* **Android**: [Android Building Guide](./docs/build/Android.md)
* **Solaris**: [Solaris Building Guide](./docs/build/Solaris.md)
* **FreeBSD**: [FreeBSD Building Guide](./docs/build/FreeBSD.md)
* **macOS**: [macOS Building Guide](./docs/build/macOS.md)

# How to test JIT

## gdb

Run `./build/bin/eden-cli -c <path to your config file (see logs where you run eden normally to see where it is)> -d -g <path to game>`

Then hook up an aarch64-gdb (use `yay aarch64-gdb` or `sudo pkg in arch64-gdb` to install)
Then type `target remote localhost:1234` and type `c` (for continue) - and then if it crashes just do a `bt` (backtrace) and `layout asm`.

### gdb cheatsheet

- `c`: Continue
- `p <expr>`: Print variable, `p/x <expr>` for hexadecimal.
- `r`: Run
- `bt`: Print backtrace
- `info threads`: Print all active threads
- `thread <number>`: Switch to the given thread (see `info threads`)
- `layout asm`: Display in assembly mode (TUI)
- `si`: Step assembly instruction
- `s` or `step`: Step over LINE OF CODE (not assembly)
- `display <expr>`: Display variable each step.
- `n`: Next (skips over call frame of a function)
- `frame <number>`: Switches to the given frame (from `bt`)
- `br <expr>`: Set breakpoint at `<expr>`.
- `delete`: Deletes all breakpoints.
- `catch throw`: Breakpoint at throw. Can also use `br __cxa_throw`

Expressions can be `variable_names` or `1234` (numbers) or `*var` (dereference of a pointer) or `*(1 + var)` (computed expression).

For more information type `info gdb` and read [the man page](https://man7.org/linux/man-pages/man1/gdb.1.html).

# Bisecting

Each commit should be buildable (independently) so bisecting is easier. See [git-bisect](https://git-scm.com/docs/git-bisect) for more information.

## Notes

Since going into the past can be tricky (especially due to the dependencies from the project being lost thru time). This should "restore" the URLs for the respective submodules.

```sh
#!/bin/sh
rm .gitmodules
wget -O .gitmodules https://git.eden-emu.dev/eden-emu/eden/raw/branch/master/.gitmodules
git submodule update --init --recursive
```

If you're not on Linux I'm sorry for you. Git gud.
