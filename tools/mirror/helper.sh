#!/bin/sh -e
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
# You must run this at the root of the eden git repo
CPM_DIR=$PWD
NPROC=`nproc`
. ./tools/mirror/common.sh
#read -p "Absolute path of target directory (i.e /srv/git/repos): " TARGET_ROOT_DIR
[ -z "$TARGET_ROOT_DIR" ] && TARGET_ROOT_DIR="/usr/local/jails/containers/classic/srv/git/repos"
echo "Path: " $TARGET_ROOT_DIR
echo "Path will be modified with perms 777"
echo "1  - Do initial clone/world"
echo "2  - Clone submodules (update)"
echo "3  - Force manual remotes update (please use crontab instead)"
read -p "Selection? [none]: " ANSWER
if [ "$ANSWER" = "1" ]; then
    echo Cloning to $TARGET_ROOT_DIR...
    sudo chmod 777 $TARGET_ROOT_DIR
    # stuff to parse the cpmfile json and then spit out full repo path
    REPOS=$(find "$CPM_DIR" "$CPM_DIR/src" -maxdepth 3 -type f -name 'cpmfile.json' -not -path 'build' \
        | sort | uniq | xargs jq -s 'reduce .[] as $item ({}; . * $item)' \
        | jq -r 'reduce .[] as $i (""; . + (if $i.git_host == null then "https://github.com" else "https://" + $i.git_host end) + "/" + $i.repo + " ")' \
        | tr ' ' '\n' | xargs -I {} echo {})
    # clone the stuff
    cd $TARGET_ROOT_DIR && echo "$REPOS" \
        | xargs -P $NPROC -I {} sh -c ". $CPM_DIR/tools/mirror/common.sh && git_clone_mirror_repo {}"
    sudo chmod 755 $TARGET_ROOT_DIR
elif [ "$ANSWER" = "2" ]; then
    echo Updating submodules of $TARGET_ROOT_DIR...
    sudo chmod 777 $TARGET_ROOT_DIR
    cd $TARGET_ROOT_DIR && find . -maxdepth 2 -type d -name '*.git' -print0 \
        | xargs -0 -I {} sh -c ". $CPM_DIR/tools/mirror/common.sh && git_retrieve_gitmodules {}" \
        | while IFS= read -r line; do
        NAME=$(echo "$line" | awk -F'/' '{print $5}')
        ORG=$(echo "$line" | awk -F'/' '{print $4}')
        URL=$(echo "$line")
        git clone --mirror $URL $ORG/$NAME || echo "skipped $ORG/$NAME"
    done
    sudo chmod 755 $TARGET_ROOT_DIR
elif [ "$ANSWER" = "3" ]; then
    echo Forcing git updates
    sudo chmod 777 $TARGET_ROOT_DIR
    cd $TARGET_ROOT_DIR && find . -maxdepth 3 -type d -name '*.git' -print0 \
        | xargs -0 -P $NPROC -I {} sh -c 'cd {} && git remote update && echo {}'
    sudo chmod 755 $TARGET_ROOT_DIR
else
    echo Aborting
fi

