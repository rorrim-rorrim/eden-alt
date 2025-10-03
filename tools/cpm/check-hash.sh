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

    CI=$(jq -r ".ci" <<< "$JSON")
    if [ "$CI" != null ]; then
        continue
    fi

    HASH_URL=$(jq -r ".hash_url" <<< "$JSON")
    [ "$HASH_URL" != null ] && continue

    REPO=$(jq -r ".repo" <<< "$JSON")
    TAG=$(jq -r ".tag" <<< "$JSON")

    URL=$(jq -r ".url" <<< "$JSON")
    SHA=$(jq -r ".sha" <<< "$JSON")

    echo "-- Package $package"

    GIT_VERSION=$(jq -r ".git_version" <<< "$JSON")

    VERSION=$(jq -r ".version" <<< "$JSON")
    GIT_VERSION=$(jq -r ".git_version" <<< "$JSON")

    if [ "$GIT_VERSION" != null ]; then
        VERSION_REPLACE="$GIT_VERSION"
    else
        VERSION_REPLACE="$VERSION"
    fi

    TAG=${TAG//%VERSION%/$VERSION_REPLACE}

    ARTIFACT=$(jq -r ".artifact" <<< "$JSON")
    ARTIFACT=${ARTIFACT//%VERSION%/$VERSION_REPLACE}
    ARTIFACT=${ARTIFACT//%TAG%/$TAG}

    if [ "$URL" != "null" ]; then
        DOWNLOAD="$URL"
    elif [ "$REPO" != "null" ]; then
        GIT_URL="https://$GIT_HOST/$REPO"

        BRANCH=$(jq -r ".branch" <<< "$JSON")

        if [ "$TAG" != "null" ]; then
            if [ "$ARTIFACT" != "null" ]; then
                DOWNLOAD="${GIT_URL}/releases/download/${TAG}/${ARTIFACT}"
            else
                DOWNLOAD="${GIT_URL}/archive/refs/tags/${TAG}.tar.gz"
            fi
        elif [ "$SHA" != "null" ]; then
            DOWNLOAD="${GIT_URL}/archive/${SHA}.zip"
        else
            if [ "$BRANCH" = null ]; then
                BRANCH=master
            fi

            DOWNLOAD="${GIT_URL}/archive/refs/heads/${BRANCH}.zip"
        fi
    else
        echo "!! No repo or URL defined for $package"
        continue
    fi

    # hash parsing
    HASH_ALGO=$(jq -r ".hash_algo" <<< "$JSON")
    [ "$HASH_ALGO" = null ] && HASH_ALGO=sha512

    HASH=$(jq -r ".hash" <<< "$JSON")

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