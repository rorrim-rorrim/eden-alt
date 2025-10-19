#!/bin/sh -e

ANDROID=src/android/app/src/main
STRINGS=$ANDROID/res/values/strings.xml

TMP_DIR=$(mktemp -d)

USED="$TMP_DIR"/used
UNUSED="$TMP_DIR"/unused
FILTERED="$TMP_DIR"/filtered
ALL_STRINGS="$TMP_DIR"/all

find $ANDROID -type f -iname '*.xml' -a -not -iname '*strings.xml' -o -iname '*.kt' | grep -v drawable > "$TMP_DIR"/files
grep -e "string name" $STRINGS | cut -d'"' -f2 > "$ALL_STRINGS"

while IFS= read -r file; do
echo "$file"
	grep -o 'R\.string\.[a-zA-Z0-9_]\+' "$file" >> "$USED" || true
	grep -o '@string/[a-zA-Z0-9_]\+' "$file" >> "$USED" || true
done < "$TMP_DIR"/files

sed 's/R.string.\|@string\///' "$USED" | sort -u | grep -v app_name_suffixed > "$FILTERED"
cat "$FILTERED" "$ALL_STRINGS" | sort | uniq -u > "$UNUSED"

while IFS= read -r string; do
	echo "$string"
	find $ANDROID/res/values -iname '*strings.xml' | while IFS= read -r file; do
		sed "/string name=\"$string\"/d" "$file" > "$file.new"
		mv "$file.new" "$file"
	done
	# grep -re "R.string.$string\|@string/$string" $ANDROID | grep -v strings.xml || true
done < "$UNUSED"

