#!/bin/sh -e

FILES=$(find . src -maxdepth 3 -name cpmfile.json)

for file in $FILES; do
    jq --indent 4 < $file > $file.new
    mv $file.new $file
done
