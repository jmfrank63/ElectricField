#!/usr/bin/env bash
set -euo pipefail

version="${1:?usage: package.sh VERSION [OUTPUT_DIR]}"
if [[ ! "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
  echo "Version must be numeric major.minor.patch: $version" >&2
  exit 2
fi
out_dir="${2:-release-out}"
root="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
stage="$(mktemp -d)"
mount="$stage/mount"
mounted=false
cleanup() {
  if [[ "$mounted" == true ]]; then
    hdiutil detach "$mount" -quiet || true
  fi
  rm -rf "$stage"
}
trap cleanup EXIT

app="$stage/Electric Field.app"
contents="$app/Contents"
resources="$contents/Resources"
mkdir -p "$contents/MacOS" "$resources" "$out_dir"
install -m 755 "$root/zig-out/bin/ElectricField" "$contents/MacOS/ElectricField"
install -m 644 "$root/LICENSE" "$resources/License.txt"
printf 'APPL????' > "$contents/PkgInfo"
sed "s/@VERSION@/$version/g" "$root/packaging/macos/Info.plist" > "$contents/Info.plist"

iconset="$stage/ElectricField.iconset"
mkdir -p "$iconset"
for size in 16 32 128 256 512; do
  sips -z "$size" "$size" "$root/assets/ElectricField-256.png" \
    --out "$iconset/icon_${size}x${size}.png" >/dev/null
done
for entry in "16 32" "32 64" "128 256" "256 512" "512 1024"; do
  read -r size doubled <<< "$entry"
  sips -z "$doubled" "$doubled" "$root/assets/ElectricField-256.png" \
    --out "$iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$iconset" -o "$resources/ElectricField.icns"
chmod +x "$contents/MacOS/ElectricField"

SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software "$contents/MacOS/ElectricField" --smoke

disk="$stage/disk"
mkdir -p "$disk"
cp -R "$app" "$disk/"
ln -s /Applications "$disk/Applications"
hdiutil create -quiet -volname "Electric Field" -srcfolder "$disk" -ov -format UDZO \
  "$out_dir/ElectricField_${version}_macos-arm64.dmg"
hdiutil verify "$out_dir/ElectricField_${version}_macos-arm64.dmg"

mkdir -p "$mount"
hdiutil attach -quiet -nobrowse -readonly -mountpoint "$mount" \
  "$out_dir/ElectricField_${version}_macos-arm64.dmg"
mounted=true
plutil -lint "$mount/Electric Field.app/Contents/Info.plist"
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software \
  "$mount/Electric Field.app/Contents/MacOS/ElectricField" --smoke
hdiutil detach "$mount" -quiet
mounted=false
