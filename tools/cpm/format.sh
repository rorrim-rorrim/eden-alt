#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# like common.sh, change for your directories
FILES=$(find . src -maxdepth 3 -name cpmfile.json)

for file in $FILES; do
    jq --indent 4 < "$file" > "$file".new
    mv "$file".new "$file"
done
