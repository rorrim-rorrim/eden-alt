#!/bin/sh -ex

# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

# Updates main icons for eden

which magick || exit
which optipng || exit

VARIATION=${VARIATION:-base}

EDEN_BASE_SVG="dist/icon_variations/${VARIATION}.svg"
EDEN_MONO_SVG="dist/icon_variations/${VARIATION}_monochrome.svg"
EDEN_BG_COLOR="dist/icon_variations/${VARIATION}_bgcolor"

[ -f "$EDEN_BASE_SVG" ] && [ -f "$EDEN_BG_COLOR" ] || { echo "Error: missing ${VARIATION}.svg/${VARIATION}_bgcolor" >&2; exit; }
if [ ! -f "$EDEN_MONO_SVG" ]; then
    EDEN_MONO_SVG="dist/icon_variations/base.svg"
    OPT_MAGICK_MONO="-alpha on -fill white -colorize 100"
fi

# Desktop / Windows / Qt icons

EDEN_DESKTOP_SVG="dist/dev.eden_emu.eden.svg"

cp "$EDEN_BASE_SVG" "$EDEN_DESKTOP_SVG"

magick -monitor -density 256x256 -background transparent "$EDEN_BASE_SVG" -define icon:auto-resize -colors 256 dist/eden.ico || exit
magick -monitor -density 256x256 -background transparent "$EDEN_BASE_SVG" -resize 256x256 dist/eden.bmp || exit

magick -monitor -size 256x256 -background transparent "$EDEN_BASE_SVG" -resize 256x256 dist/qt_themes/default/icons/256x256/eden.png || exit

optipng -o7 dist/qt_themes/default/icons/256x256/eden.png

# Android adaptive icon (API 26+)

EDEN_ANDROID_RES="src/android/app/src/main/res"

EDEN_ANDROID_LAUNCHER_FG="$EDEN_ANDROID_RES/drawable/ic_eden_launcher_foreground.png"
EDEN_ANDROID_LAUNCHER_MONO="$EDEN_ANDROID_RES/drawable/ic_eden_launcher_monochrome.png"
EDEN_ANDROID_MAIN="$EDEN_ANDROID_RES/drawable/ic_eden.png"
EDEN_ANDROID_MONO="$EDEN_ANDROID_RES/drawable/ic_eden_monochrome.png"

EDEN_ANDROID_BG_COLOR=$(cat $EDEN_BG_COLOR)

# Update Icon Background Color
echo "<?xml version='1.0' encoding='utf-8'?><resources><color name='ic_launcher_background'>${EDEN_ANDROID_BG_COLOR}</color></resources>" > "$EDEN_ANDROID_RES/values/colors.xml"

magick -monitor -size 1080x1080 -background transparent "$EDEN_BASE_SVG" -gravity center -resize 660x660 -extent 1080x1080 "$EDEN_ANDROID_LAUNCHER_FG" || exit
magick -monitor -size 1080x1080 -background transparent "$EDEN_MONO_SVG" -gravity center -resize 660x660 -extent 1080x1080 $OPT_MAGICK_MONO "$EDEN_ANDROID_LAUNCHER_MONO" || exit

magick -monitor -background transparent "$EDEN_BASE_SVG" -gravity center -resize 512x512 "$EDEN_ANDROID_MAIN" || exit
magick -monitor -background transparent "$EDEN_MONO_SVG" -gravity center -resize 512x512 $OPT_MAGICK_MONO "$EDEN_ANDROID_MONO" || exit

optipng -o7 "$EDEN_ANDROID_LAUNCHER_FG"
optipng -o7 "$EDEN_ANDROID_LAUNCHER_MONO"
optipng -o7 "$EDEN_ANDROID_MONO"
optipng -o7 "$EDEN_ANDROID_MAIN"

# macOS
# TODO: Update Assets.car too

TMP_PNG="dist/eden-tmp.png"

magick -size 1024x1024 -background none "$EDEN_BASE_SVG" "$TMP_PNG" || exit

png2icns dist/eden.icns "$TMP_PNG" || echo 'non fatal'

rm "$TMP_PNG"
