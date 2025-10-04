#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# check which file a package is in

# like common.sh, change for your directories
JSON=$(find "$DIRS" -maxdepth "$MAXDEPTH" -name cpmfile.json -exec grep -l "$1" {} \;)

[ -z "$JSON" ] && echo "!! No cpmfile definition for $1"

echo "$JSON"