#!/bin/sh -e

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# env vars:
# - UPDATE: fix hashes if needed

# shellcheck disable=SC1091
. tools/cpm/common.sh

for package in "$@"
do
	export package
	# shellcheck disable=SC1091
	. tools/cpm/package.sh

    if [ "$CI" != null ]; then
        continue
    fi

    [ "$HASH_URL" != null ] && continue
    [ "$HASH_SUFFIX" != null ] && continue

    echo "-- Package $package"

    [ "$HASH" = null ] && echo "-- * Warning: no hash specified" && continue

    export USE_TAG=true
    ACTUAL=$(tools/cpm/url-hash.sh "$DOWNLOAD")

    # shellcheck disable=SC2028
    [ "$ACTUAL" != "$HASH" ] && echo "-- * Expected $HASH" && echo "-- * Got      $ACTUAL"

    if [ "$UPDATE" = "true" ] && [ "$ACTUAL" != "$HASH" ]; then
        NEW_JSON=$(echo "$JSON" | jq ".hash = \"$ACTUAL\"")

        FILE=$(tools/cpm/which.sh "$package")

        jq --indent 4 --argjson repl "$NEW_JSON" ".\"$package\" *= \$repl" "$FILE" > "$FILE".new
        mv "$FILE".new "$FILE"

        echo "-- * -- Updated $FILE"
    fi
done