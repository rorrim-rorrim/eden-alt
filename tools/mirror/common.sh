#!/bin/sh -e
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
git_clone_mirror_repo() {
    git clone --mirror $1 $(echo $1 | awk -F"/" '{print $4"/"$5".git"}')
}
git_retrieve_file() {
    TMPDIR=$1
    BRANCH=$(cd $TMPDIR && git rev-parse --abbrev-ref HEAD)
    cd $1 && git show $BRANCH:$2 2>/dev/null | git lfs smudge 2>/dev/null
}
git_retrieve_gitmodules() {
    TMP=$1
    git_retrieve_file $TMP '.gitmodules' | awk '{$1=$1};1' \
        | grep "url = " | sed "s,url = ,," \
        | while IFS= read -r line; do
        case "$line" in
            ../*)
                # TODO: maybe one day handle case where its NOT ALL GITHUB - thanks boost
                ORGAN=$(echo "$TMP" | awk -F'/' '{print $2}')
                echo "$line" | sed "s|../|https://github.com/$ORGAN/|"
                ;;
            http*)
                echo "$line"
                ;;
        esac
    done
}
