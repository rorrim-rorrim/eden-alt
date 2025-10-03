#!/bin/sh -e

# check which file a package is in

JSON=$(find . src -maxdepth 3 -name cpmfile.json -exec grep -l "$1" {} \;)

[ -z "$JSON" ] && echo "!! No cpmfile definition for $1"

echo $JSON