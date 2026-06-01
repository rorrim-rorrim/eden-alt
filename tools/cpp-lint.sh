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
	osdef	Fixes OS defines that are not recommended to use:
			ANDROID
			_WIN64
			linux
			__unix__
			__unix
EOF
}

while :; do
	case "$1" in
	once)
        find "$SRC_DIR" -type f -name "*.h" -exec grep -L "#pragma once" {} +
		break
		;;
	osdef)
        find "$SRC_DIR" -type f -name "*.h" \
			-exec grep -nw "ANDROID\|_WIN64\|__linux\|__unix\|APPLE\|__APPLE" {} + || echo
        find "$SRC_DIR" -type f -name "*.h" -exec grep -nw "ifdef linux\|(linux)" {} + || echo
        find "$SRC_DIR" -type f -name "*.h" -exec grep -nw "ifdef unix\|(unix)" {} + || echo
		break
		;;
	*) usage ;;
	esac

	shift
done
