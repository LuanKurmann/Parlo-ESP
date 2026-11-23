#!/bin/sh
# Erzeugt die LVGL-Schriften in lib/ui/src/ui/fonts/ aus Barlow und
# Barlow Condensed (SIL Open Font License 1.1).
#
# Nur noetig, wenn Groessen oder Zeichen geaendert werden; die erzeugten
# Dateien liegen im Repository.
#
#   sh scripts/gen_fonts.sh
set -eu

out="$(cd "$(dirname "$0")/.." && pwd)/lib/ui/src/ui/fonts"
work="$(mktemp -d)"
cd "$work"
npm init -y >/dev/null
npm install --silent lv_font_conv@1.5.3 @fontsource/barlow@5.0.13 @fontsource/barlow-condensed@5.0.13

text500="node_modules/@fontsource/barlow/files/barlow-latin-500-normal.woff"
text600="node_modules/@fontsource/barlow/files/barlow-latin-600-normal.woff"
cond600="node_modules/@fontsource/barlow-condensed/files/barlow-condensed-latin-600-normal.woff"
extra="ÄÖÜäöüß·–"

gen() { # Name, Datei, Groesse, weitere Argumente
    name="$1"; font="$2"; size="$3"; shift 3
    npx lv_font_conv --font "$font" "$@" --size "$size" --bpp 4 --format lvgl --no-compress \
        --lv-include lvgl.h --lv-font-name "$name" -o "$out/$name.c"
}

gen golf_font_text_16 "$text500" 16 -r 0x20-0x7E --symbols "$extra"
gen golf_font_text_20 "$text500" 20 -r 0x20-0x7E --symbols "$extra"
gen golf_font_text_26 "$text500" 26 -r 0x20-0x7E --symbols "$extra"
gen golf_font_cap_22  "$text600" 22 -r 0x20-0x7E --symbols "$extra"
gen golf_font_num_30  "$cond600" 30 -r 0x20-0x7E --symbols "$extra"
gen golf_font_num_40  "$cond600" 40 -r 0x20-0x7E --symbols "$extra"
gen golf_font_num_100 "$cond600" 100 --symbols "0123456789-"
gen golf_font_num_154 "$cond600" 154 --symbols "0123456789-"

rm -rf "$work"
echo "Schriften erzeugt in $out"
