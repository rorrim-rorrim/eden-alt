#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2026 crueter
# SPDX-License-Identifier: LGPL-3.0-or-later

file=cpmfile.json

jq --indent 4 -S <"$file" >"$file".new
mv "$file".new "$file"
