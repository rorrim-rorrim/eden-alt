#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# env vars:
# - UPDATE: update if available
# - FORCE: forcefully update

# shellcheck disable=SC1091
. tools/cpm/common.sh

for package in "$@"
do
	export package
	# shellcheck disable=SC1091
	. tools/cpm/package.sh

    SKIP=$(value "skip_updates")

    [ "$SKIP" = "true" ] && continue

    [ "$REPO" = null ] && continue
    [ "$GIT_HOST" != "github.com" ] && continue # TODO
    [ "$TAG" = null ] && continue

    echo "-- Package $package"

	# TODO(crueter): Support for Forgejo updates w/ forgejo_token
    # Use gh-cli to avoid ratelimits lmao
    TAGS=$(gh api --method GET "/repos/$REPO/tags")

    # filter out some commonly known annoyances
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("vulkan-sdk"; "i") | not)]')
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("yotta"; "i") | not)]')

    # ignore betas/alphas
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("alpha"; "i") | not)]')
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("beta"; "i") | not)]')
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("rc"; "i") | not)]')

    # thanks fmt
    [ "$package" = fmt ] && TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("v0.11"; "i") | not)]')

    LATEST=$(echo "$TAGS" | jq -r '.[0].name')

    [ "$LATEST" = "$TAG" ] && [ "$FORCE" != "true" ] && echo "-- * Up-to-date" && continue

    if [ "$HAS_REPLACE" = "true" ]; then
        # this just extracts the tag prefix
        VERSION_PREFIX=$(echo "$ORIGINAL_TAG" | cut -d"%" -f1)

        # then we strip out the prefix from the new tag, and make that our new git_version
        NEW_GIT_VERSION=$(echo "$LATEST" | sed "s/$VERSION_PREFIX//g")
    fi

    echo "-- * Version $LATEST available, current is $TAG"

    export USE_TAG=true
    HASH=$(tools/cpm/hash.sh "$REPO" "$LATEST")

    echo "-- * New hash: $HASH"

    if [ "$UPDATE" = "true" ]; then
        if [ "$HAS_REPLACE" = "true" ]; then
            NEW_JSON=$(echo "$JSON" | jq ".hash = \"$HASH\" | .git_version = \"$NEW_GIT_VERSION\"")
        else
            NEW_JSON=$(echo "$JSON" | jq ".hash = \"$HASH\" | .tag = \"$LATEST\"")
        fi

        FILE=$(tools/cpm/which.sh "$package")

        jq --indent 4 --argjson repl "$NEW_JSON" ".\"$package\" *= \$repl" "$FILE" > "$FILE".new
        mv "$FILE".new "$FILE"

        echo "-- * -- Updated $FILE"
    fi
done
