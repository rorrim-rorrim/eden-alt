#!/bin/sh -ex

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Updates main icons for eden

which magick || exit
which optipng || exit

VARIATION=${VARIATION:-base}

EDEN_BASE_SVG="dist/icon_variations/${VARIATION}.svg"
EDEN_NAMED_SVG="dist/icon_variations/${VARIATION}_named.svg"
EDEN_BG_COLOR="dist/icon_variations/${VARIATION}_bgcolor"
# TODO: EDEN_MONOCHROME_SVG Variation

[ -f "$EDEN_BASE_SVG" ] && [ -f "$EDEN_NAMED_SVG" ] && [ -f "$EDEN_BG_COLOR" ] || { echo "Error: missing ${VARIATION}.svg/${VARIATION}_named.svg/${VARIATION}_bgcolor" >&2; exit; }

# Desktop / Windows / Qt icons

EDEN_DESKTOP_SVG="dist/dev.eden_emu.eden.svg"

cp "$EDEN_BASE_SVG" "$EDEN_DESKTOP_SVG"

magick -density 256x256 -background transparent "$EDEN_BASE_SVG" -define icon:auto-resize -colors 256 dist/eden.ico || exit
magick -density 256x256 -background transparent "$EDEN_BASE_SVG" -resize 256x256 dist/eden.bmp || exit

magick -size 256x256 -background transparent "$EDEN_BASE_SVG" -resize 256x256 dist/qt_themes/default/icons/256x256/eden.png || exit
magick -size 256x256 -background transparent "$EDEN_NAMED_SVG" -resize 256x256 dist/qt_themes/default/icons/256x256/eden_named.png || exit

optipng -o7 dist/qt_themes/default/icons/256x256/eden.png
optipng -o7 dist/qt_themes/default/icons/256x256/eden_named.png

png2icns dist/eden.icns $TMP_PNG || echo 'non fatal'
rm $TMP_PNG
