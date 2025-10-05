# User Handbook - The Basics

## Introduction

Eden is a very complicated piece of software, and as such there are many knobs and toggles that can be configured. Most of these are invisible to normal users, however power users may be able to leverage them to their advantage.

This handbook primarily describes such knobs and toggles. Normal configuration options are described within the emulator itself and will not be covered in detail.

## User configuration

### Configuration directories

Eden will store configuration files in the following directories:

- **Windows**: `%AppData%\Roaming`.
- **Android**: Data is stored internally.
- **Linux, macOS, FreeBSD, Solaris, OpenBSD**: `$XDG_DATA_HOME`, `$XDG_CACHE_HOME`, `$XDG_CONFIG_HOME`.

If a `user` directory is present in the current working directory, that will override all global configuration directories and the emulator will use that instead.

### Environment variables

Throughout the handbook, environment variables are mentioned. These are often either global (system wide) or local (set in a script, bound only to the current session). It's heavily recommended to use them in a local context only, as this allows to rollback changes easily (if for example, there are regressions setting them).

The recommended way is to create a `.bat` file alongside the emulator `.exe`; contents of which could resemble something like:

```bat
set "__GL_THREADED_OPTIMIZATIONS=1"
set "SOME_OTHER_VAR=1"
eden.exe
```

Android doesn't have a convenient way to set environment variables.

For other platforms, the recommended method is using a bash script:

```sh
export __GL_THREADED_OPTIMIZATIONS=1
export SOME_OTHER_VAR=1
./eden
```

Then just running `chmod +x script.sh && source script.sh`.

## Compatibility list

Eden doesn't mantain a compatibility list. However, [EmuReady](https://www.emuready.com/) has a more fine-grained compatibility information for multiple emulators/forks as well.
