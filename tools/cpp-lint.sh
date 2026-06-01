#!/bin/sh -ex

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# tools/../
ROOTDIR=$(CDPATH='' cd -- "$(dirname -- "$0")/../" && pwd)

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

EOF
}

while :; do
	case "$1" in
	once)
        find "$ROOTDIR/src" -type f -name "*.h" -exec grep -L "#pragma once" {} +
		break
		;;
	osdef)
        find "$ROOTDIR/src" -type f -name "*.h" -exec grep -nw "ANDROID" {} +
		break
		;;
	*) usage ;;
	esac

	shift
done
