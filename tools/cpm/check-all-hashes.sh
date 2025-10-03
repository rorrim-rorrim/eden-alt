#!/bin/bash -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# shellcheck disable=SC2038
# shellcheck disable=SC2016
[ -z "$PACKAGES" ] && PACKAGES=$(find . src -maxdepth 3 -name cpmfile.json | xargs jq -s 'reduce .[] as $item ({}; . * $item)')
LIBS=$(echo "$PACKAGES" | jq -j 'keys_unsorted | join(" ")')

tools/cpm/check-hash.sh $LIBS