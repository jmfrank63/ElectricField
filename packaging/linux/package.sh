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
trap 'rm -rf "$stage"' EXIT

package="$stage/package"
install -D "$root/zig-out/bin/ElectricField" "$package/usr/bin/ElectricField"
install -D "$root/zig-out/bin/ef-mcp" "$package/usr/bin/ef-mcp"
install -D "$root/assets/ElectricField-256.png" "$package/usr/share/icons/hicolor/256x256/apps/electricfield.png"
install -D "$root/packaging/linux/electricfield.desktop" "$package/usr/share/applications/electricfield.desktop"
install -D "$root/LICENSE" "$package/usr/share/doc/electricfield/copyright"
mkdir -p "$package/DEBIAN" "$stage/debian" "$out_dir"

cat > "$stage/debian/control" <<'EOF'
Source: electricfield
Section: science
Priority: optional
Maintainer: Johannes Maria Frank
Standards-Version: 4.7.0

Package: electricfield
Architecture: amd64
Depends: ${shlibs:Depends}, ${misc:Depends}
Description: electric field and equipotential visualizer
 Visualize electric field lines and equipotentials of point charges.
EOF
dependencies="$(cd "$stage" && dpkg-shlibdeps -O -e"$package/usr/bin/ElectricField" -e"$package/usr/bin/ef-mcp" | sed -n 's/^shlibs:Depends=//p')"
if [[ -z "$dependencies" ]]; then
  echo "dpkg-shlibdeps did not produce runtime dependencies" >&2
  exit 1
fi

cat > "$package/DEBIAN/control" <<EOF
Package: electricfield
Version: $version
Section: science
Priority: optional
Architecture: amd64
Maintainer: Johannes Maria Frank
Depends: $dependencies
Description: electric field and equipotential visualizer
 Visualize electric field lines and equipotentials of point charges.
EOF

dpkg-deb --build --root-owner-group "$package" "$out_dir/ElectricField_${version}_amd64.deb"
dpkg-deb --info "$out_dir/ElectricField_${version}_amd64.deb"

extracted="$stage/extracted"
dpkg-deb --extract "$out_dir/ElectricField_${version}_amd64.deb" "$extracted"
SDL_VIDEODRIVER=dummy SDL_RENDER_DRIVER=software "$extracted/usr/bin/ElectricField" --smoke
"$extracted/usr/bin/ef-mcp" --version
