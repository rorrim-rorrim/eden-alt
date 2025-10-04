#!/bin/bash -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

SCRIPT_DIR=$(dirname -- "$0")

# shellcheck disable=SC1091
. tools/cpm/common.sh

# shellcheck disable=SC2086
"$SCRIPT_DIR"/check-hash.sh $LIBS