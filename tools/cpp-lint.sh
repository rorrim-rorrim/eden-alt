#!/bin/sh -ex

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# tools/../
ROOTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")/../" && pwd)
BUILD_DIR="$ROOTDIR"/build
SRC_DIR="$ROOTDIR"/src

die() {
    echo "-- $*" >&2
    exit 1
}

usage() {
    cat <<EOF
Usage: $0 [command]

Dumb script that serves as a ad-hoc cpp-linter

Commands:
    once    Check for #pragma once prescence in header files
	osdef	Finds OS defines that are not recommended to use.
	inchk   Check includes being valid/toolchain not being stupid
EOF
}

while :; do
	case "$1" in
	once)
        find "$SRC_DIR" -type f -name "*.h" -exec grep -L "#pragma once" {} +
		break
		;;
	osdef)
		# not recommended macros
		PATTERN="ANDROID\|_WIN64\|__linux\|__unix\|APPLE\|__APPLE"
		PATTERN="$PATTERN\|ifdef ANDROID\|(ANDROID)"
		PATTERN="$PATTERN\|ifdef _WIN64\|(_WIN64)"
		PATTERN="$PATTERN\|ifdef __linux\|(__linux)"
		PATTERN="$PATTERN\|ifdef __unix\|(__unix)"
		PATTERN="$PATTERN\|ifdef APPLE\|(APPLE)"
		PATTERN="$PATTERN\|ifdef __APPLE\|(__APPLE)"
		PATTERN="$PATTERN\|ifdef linux\|(linux)"
		PATTERN="$PATTERN\|ifdef unix\|(unix)"
		# if statements for macros that shouldn't be if
		PATTERN="$PATTERN\|if _WIN32"
		PATTERN="$PATTERN\|if _AIX"
		PATTERN="$PATTERN\|if __managarm__"
		PATTERN="$PATTERN\|if __unix__"
		PATTERN="$PATTERN\|if __linux__"
		PATTERN="$PATTERN\|if __FreeBSD__"
		PATTERN="$PATTERN\|if __NetBSD__"
		PATTERN="$PATTERN\|if __OpenBSD__"
		PATTERN="$PATTERN\|if __DragonFly__"
		PATTERN="$PATTERN\|if __redox__"
		PATTERN="$PATTERN\|if __HAIKU__"
		PATTERN="$PATTERN\|if __OHOS__"
		PATTERN="$PATTERN\|if __FIREOS__"
        find "$SRC_DIR" -type f -name "*.h" -exec grep -nw "$PATTERN" {} + || echo
		break
		;;
	*) usage ;;
	esac

	shift
done
