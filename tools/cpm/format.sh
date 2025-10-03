#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

FILES=$(find . src -maxdepth 3 -name cpmfile.json)

for file in $FILES; do
    jq --indent 4 < $file > $file.new
    mv $file.new $file
done
