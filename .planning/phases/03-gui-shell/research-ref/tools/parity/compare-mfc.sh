#!/usr/bin/env bash
# Informational MFC-vs-port comparison (D-13). NOT a CI gate: Direct2D and ImGui anti-alias differently.
# usage: tools/parity/compare-mfc.sh <ef-cli> [<out-dir>] [<baseline-dir>]
# needs: ImageMagick 7 (`magick`). Prints one CSV row per preset:
#   preset, ae_fuzz10 (pixels that differ by >10%), ink_recall, ink_precision, darkness_ratio (port/MFC total ink, ~1.00)
# ink = pixels darker than 97% grey. recall = share of baseline ink that has port ink within 1 px (dilate);
# precision = share of port ink that has baseline ink within 1 px. Both ~0.95+ means "same picture, different AA".
set -euo pipefail
CLI=${1:?usage: compare-mfc.sh <ef-cli> [out-dir] [baseline-dir]}
OUT=${2:-parity-out}
BASE=${3:-docs/baseline/mfc}
mkdir -p "$OUT"
REPORT="$OUT/mfc-compare.csv"
: > "$REPORT"
"$CLI" --all --out-dir "$OUT" >/dev/null
echo "preset,ae_fuzz10,ink_recall,ink_precision,darkness_ratio" | tee "$REPORT"
# tr -d '\015': the Windows exe writes CRLF
for slug in $("$CLI" --list | tr -d '\015'); do
  A="$BASE/$slug.png"; B="$OUT/$slug.png"
  ae=$(magick compare -metric AE -fuzz 10% "$A" "$B" null: 2>&1 || true); ae=${ae%% *}
  ink() { magick "$1" -alpha off -colorspace Gray -threshold 97% -negate "$2"; }
  dilated() { magick "$1" -alpha off -colorspace Gray -threshold 97% -negate -morphology Dilate Disk:1 "$2"; }
  d=$(mktemp -d)
  ink "$A" "$d/a.png"; ink "$B" "$d/b.png"; dilated "$A" "$d/ad.png"; dilated "$B" "$d/bd.png"
  m_a=$(magick "$d/a.png" -format '%[fx:mean]' info:); m_b=$(magick "$d/b.png" -format '%[fx:mean]' info:)
  rec=$(magick "$d/a.png" "$d/bd.png" -compose Multiply -composite -format "%[fx:mean/$m_a]" info:)
  pre=$(magick "$d/b.png" "$d/ad.png" -compose Multiply -composite -format "%[fx:mean/$m_b]" info:)
  dk_a=$(magick "$A" -alpha off -colorspace Gray -format '%[fx:1-mean]' info:)
  dk_b=$(magick "$B" -alpha off -colorspace Gray -format '%[fx:1-mean]' info:)
  dk=$(magick null: -format "%[fx:$dk_b/$dk_a]" info:)
  rm -rf "$d"
  printf '%s,%s,%.3f,%.3f,%.3f\n' "$slug" "$ae" "$rec" "$pre" "$dk" | tee -a "$REPORT"
done
