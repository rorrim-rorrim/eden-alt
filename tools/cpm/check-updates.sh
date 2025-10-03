#!/bin/bash -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# SPDX-FileCopyrightText: 2025 crueter
# SPDX-License-Identifier: GPL-3.0-or-later

# env vars:
# - UPDATE: update if available
# - FORCE: forcefully update

# shellcheck disable=SC2016
# shellcheck disable=SC2038
[ -z "$PACKAGES" ] && PACKAGES=$(find . src -maxdepth 3 -name cpmfile.json | xargs jq -s 'reduce .[] as $item ({}; . * $item)')

for package in "$@"
do
    JSON=$(echo "$PACKAGES" | jq -r ".\"$package\" | select( . != null )")

    [ -z "$JSON" ] && echo "!! No cpmfile definition for $package" && continue

    GIT_HOST=$(jq -r ".git_host" <<< "$JSON")
    [ "$GIT_HOST" = null ] && GIT_HOST=github.com

    REPO=$(jq -r ".repo" <<< "$JSON")
    TAG=$(jq -r ".tag" <<< "$JSON")
    SKIP=$(jq -r ".skip_updates" <<< "$JSON")

    [ "$SKIP" = "true" ] && continue

    [ "$REPO" = null ] && continue # echo "No repo defined for $package, skipping" && continue
    [ "$GIT_HOST" != "github.com" ] && continue # echo "Unsupported host $GIT_HOST for $package, skipping" && continue
    [ "$TAG" = null ] && continue # echo "No tag defined for $package, skipping" && continue

    echo "-- Package $package"

    GIT_VERSION=$(jq -r ".git_version" <<< "$JSON")

    echo "$TAG" | grep -e "%VERSION%" > /dev/null && HAS_REPLACE=true || HAS_REPLACE=false
    ORIGINAL_TAG="$TAG"

    TAG=${TAG//%VERSION%/$GIT_VERSION}

    ARTIFACT=$(jq -r ".artifact" <<< "$JSON")
    ARTIFACT=${ARTIFACT//%VERSION%/$GIT_VERSION}
    ARTIFACT=${ARTIFACT//%TAG%/$TAG}

    # Use gh-cli to avoid ratelimits lmao
    TAGS=$(gh api --method GET "/repos/$REPO/tags")

    # filter out some commonly known annoyances
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("vulkan-sdk"; "i") | not)]')
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("yotta"; "i") | not)]')

    # ignore betas/alphas
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("alpha"; "i") | not)]')
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("beta"; "i") | not)]')
    TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("rc"; "i") | not)]')

    if [ "$HAS_REPLACE" = "true" ]; then
        # this just extracts the tag prefix
        VERSION_PREFIX=$(echo "$ORIGINAL_TAG" | cut -d"%" -f1)

        # then we strip out the prefix from the new tag, and make that our new git_version
        NEW_GIT_VERSION=${LATEST//$VERSION_PREFIX/}
    fi

    # thanks fmt
    [ "$package" = fmt ] && TAGS=$(echo "$TAGS" | jq '[.[] | select(.name | test("v0.11"; "i") | not)]')

    LATEST=$(echo "$TAGS" | jq -r '.[0].name')

    [ "$LATEST" = "$TAG" ] && [ "$FORCE" != "true" ] && echo "-- * Up-to-date" && continue

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
