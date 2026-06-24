#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

# TODO: cache cpmfile defs?
must_install() {
	for cmd in "$@"; do
		command -v "$cmd" >/dev/null 2>&1 || { echo "-- $cmd must be installed" && exit 1; }
	done
}

must_install jq find mktemp tar 7z unzip sha512sum git patch curl

LIBS=$(jq -j 'keys_unsorted | join(" ")' cpmfile.json)

export LIBS
