#!/bin/sh -e

# SPDX-FileCopyrightText: Copyright 2025 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# argparse

usage() {
	cat << EOF
$0: Check license headers compared to this branch's merge base with master.
Usage: $0 [uc]
	-u, --update	Fix license headers, if applicable
	-c, --commit	Commit changes to Git (requires --update)
EOF
}

while true; do
	case "$1" in
		(-uc) UPDATE=true; COMMIT=true ;;
		(-u|--update) UPDATE=true ;;
		(-c|--commit) COMMIT=true ;;
		("$0") break ;;
		("") break ;;
		(*) usage ;;
	esac

	shift
done

# specify full path if dupes may exist
EXCLUDE_FILES="CPM.cmake CPMUtil.cmake GetSCMRev.cmake sse2neon.h"
EXCLUDE_FILES=$(echo "$EXCLUDE_FILES" | sed 's/ /|/g')

# license header constants, please change when needed :))))
YEAR=2025
HOLDER="Eden Emulator Project"
LICENSE="GPL-3.0-or-later"

# human-readable header string
header() {
	header_line1 "$1"
	header_line2 "$1"
}

header_line1() {
	echo "$1 SPDX-FileCopyrightText: Copyright $YEAR $HOLDER"
}

header_line2() {
	echo "$1 SPDX-License-Identifier: $LICENSE"
}

# PCRE header string
pcre_header() {
	begin="$1"
	echo "(?s)$(header_line1 "$begin").*$(header_line2 "$begin")"
}

check_header() {
	begin="$1"
	file="$2"
    content="$(head -n5 < "$2")"

	header="$(pcre_header "$begin")"

	if ! echo "$content" | grep -Pzo "$header" > /dev/null; then
		# SRC_FILES is Kotlin/C++
		# OTHER_FILES is sh, CMake
		case "$begin" in
			"//")
				SRC_FILES="$SRC_FILES $file"
				;;
			"#")
				OTHER_FILES="$OTHER_FILES $file"
				;;
		esac
	fi
}

BASE=$(git merge-base master HEAD)
FILES=$(git diff --name-only "$BASE")

for file in $FILES; do
    [ -f "$file" ] || continue

	case $(basename -- "$file") in
        "$EXCLUDE_FILES")
			# skip files that are third party (crueter's CMake modules, sse2neon, etc)
			continue
            ;;
		*.cmake|*.sh|CMakeLists.txt)
			begin="#"
			;;
		*.kt*|*.cpp|*.h)
			begin="//"
			;;
    esac

	check_header "$begin" "$file"
done

if [ -z "$SRC_FILES" ] && [ -z "$OTHER_FILES" ]; then
    echo "-- All good."

    exit
fi

echo

if [ "$SRC_FILES" != "" ]; then
    echo "-- The following source files have incorrect license headers:"

	HEADER=$(header "//")

    for file in $SRC_FILES; do echo "-- * $file"; done

    cat << EOF

-- The following license header should be added to the start of these offending files:

=== BEGIN ===
$HEADER
===  END  ===

EOF

fi

if [ "$OTHER_FILES" != "" ]; then
    echo "-- The following CMake and shell scripts have incorrect license headers:"

	HEADER=$(header "#")

    for file in $OTHER_FILES; do echo "-- * $file"; done

    cat << EOF

-- The following license header should be added to the start of these offending files:

=== BEGIN ===
$HEADER
===  END  ===

EOF

fi

cat << EOF
    If some of the code in this PR is not being contributed by the original author,
    the files which have been exclusively changed by that code can be ignored.
    If this happens, this PR requirement can be bypassed once all other files are addressed.

EOF

if [ "$UPDATE" = "true" ]; then
	TMP_DIR=$(mktemp -d)
	echo "-- Fixing headers..."

	for file in $SRC_FILES $OTHER_FILES; do
		case $(basename -- "$file") in
			*.cmake|CMakeLists.txt)
				begin="#"
				shell="false"
				;;
			*.sh)
				begin="#"
				shell=true
				;;
			*.kt*|*.cpp|*.h)
				begin="//"
				shell="false"
				;;
		esac

		# This is fun
		match="$begin SPDX-FileCopyrightText.*$HOLDER"

		# basically if the copyright holder is already defined we can just replace the year
		if head -n5 < "$file" | grep -e "$match" >/dev/null; then
			replace=$(header_line1 "$begin")
			sed "s|$match|$replace|" "$file" > "$file".bak
			mv "$file".bak "$file"
		else
			header "$begin" > "$TMP_DIR"/header

			if [ "$shell" = "true" ]; then
				# grab shebang
				head -n1 "$file" > "$TMP_DIR/shebang"
				echo >> "$TMP_DIR/shebang"

				# remove shebang
				sed '1d' "$file" > "$file".bak
				mv "$file".bak "$file"

				# add to header
				cat "$TMP_DIR"/shebang "$TMP_DIR"/header > "$TMP_DIR"/new-header
				mv "$TMP_DIR"/new-header "$TMP_DIR"/header
			else
				echo >> "$TMP_DIR/header"
			fi

			cat "$TMP_DIR"/header "$file" > "$file".bak
			mv "$file".bak "$file"
		fi

		[ "$shell" = "true" ] && chmod a+x "$file"
		[ "$COMMIT" = "true" ] && git add "$file"
	done

	echo "-- Done"
fi

if [ "$COMMIT" = "true" ]; then
	echo "-- Committing changes"

	git commit -m "Fix license headers"

	echo "-- Changes committed. You may now push."
fi

[ -d "$TMP_DIR" ] && rm -rf "$TMP_DIR"