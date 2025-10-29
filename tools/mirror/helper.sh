#!/bin/sh -e
# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later
# You must run this at the root of the eden git repo
CPM_DIR=$PWD
NPROC=$(nproc || echo 8)
. ./tools/mirror/common.sh
. ./tools/cpm/common.sh
die() {
	echo "$@" >&2
	exit 1
}
help() {
    cat << EOF
--path <path>       Specify the given path (must be the root of your SCM folder)
--initial           Initial cloning (no submodules) - fetched from cpmfiles
--clone-submodules  Clones submodules, can also be used multiple times to clone
                    newly referenced submodules
--remote-update     Update all remotes (of the repos) - aka. sync with remote
--mirror            Perform a mirror clone; URL must be specified before,
                    if name or organisation are not specified, it is derived from URL
--url               Set URL of clone
--org               Set organisation folder of clone
--name              Set name of clone
EOF
}
[ -z "$SCM_ROOT_DIR" ] && SCM_ROOT_DIR="/usr/local/jails/containers/cgit-www/srv/git/repos"

op_initial() {
    sudo chmod 777 $SCM_ROOT_DIR
    # stuff to parse the cpmfile json and then spit out full repo path
    REPOS=$(echo "$PACKAGES" \
        | jq -r 'reduce .[] as $i (""; . + (if $i.git_host == null then "https://github.com" else "https://" + $i.git_host end) + "/" + $i.repo + " ")' \
        | tr ' ' '\n' | xargs -I {} echo {})
    # clone the stuff
    cd $SCM_ROOT_DIR && echo "$REPOS" \
        | xargs -P $NPROC -I {} sh -c ". $CPM_DIR/tools/mirror/common.sh && git_clone_mirror_repo {}"
    sudo chmod 755 $SCM_ROOT_DIR
}
op_clone_submodules() {
    sudo chmod 777 $SCM_ROOT_DIR
    cd $SCM_ROOT_DIR && find . -maxdepth 2 -type d -name '*.git' -print0 \
        | xargs -0 -I {} sh -c ". $CPM_DIR/tools/mirror/common.sh && git_retrieve_gitmodules {}" \
        | while IFS= read -r url; do
        git_clone_mirror_repo $url || echo "skipped $url"
    done
    sudo chmod 755 $SCM_ROOT_DIR
}
op_remote_update() {
    sudo chmod 777 $SCM_ROOT_DIR
    cd $SCM_ROOT_DIR && find . -maxdepth 3 -type d -name '*.git' -print0 \
        | xargs -0 -P $NPROC -I {} sh -c 'cd {} && git remote update && echo {}'
    sudo chmod 755 $SCM_ROOT_DIR
}
op_mirror() {
    sudo chmod 777 $SCM_ROOT_DIR
    [ -z "$URL" ] && die "Specify repo --url"
    [ -z "$NAME" ] && NAME=$(echo "$URL" | cut -d '/' -f 5)
    [ -z "$ORG" ] && ORG=$(echo "$URL" | cut -d '/' -f 4)
    cd $SCM_ROOT_DIR && git clone --mirror $URL $ORG/$NAME || echo "skipped $URL"
    sudo chmod 755 $SCM_ROOT_DIR
}

while true; do
	case "$1" in
    --path) shift; SCM_ROOT_DIR=$1; [ -z "$SCM_ROOT_DIR" ] && die "Empty target root dir";;
	--initial) OP_INITIAL=1;;
    --clone-submodules) OP_CLONE_SUBMODULES=1;;
    --remote-update) OP_REMOTE_UPDATE=1;;
    --mirror) OP_MIRROR=1;;
    --url) shift; URL=$1; [ -z "$URL" ] && die "Expected url";;
    --name) shift; NAME=$1; [ -z "$NAME" ] && die "Expected name";;
    --org) shift; ORG=$1; [ -z "$ORG" ] && die "Expected organisation";;
	--help) help "$@";;
	--*) die "Invalid option $1" ;;
	"$0" | "") break;;
	esac
	shift
done

[ "$OP_INITIAL" = 1 ] && op_initial
[ "$OP_CLONE_SUBMODULES" = 1 ] && op_clone_submodules
[ "$OP_REMOTE_UPDATE" = 1 ] && op_remote_update
[ "$OP_MIRROR" = 1 ] && op_mirror
