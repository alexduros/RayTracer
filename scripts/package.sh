#!/usr/bin/env bash
# Build the release archive for this machine: the two binaries, the models
# they resolve by name, and the docs. Used by .github/workflows/ci.yml on
# every pull request (so packaging is proven before a tag) and by
# release.yml on a tag. Runnable locally:
#
#   cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build -j
#   scripts/package.sh build dist
#
# Prints the path of the archive it wrote.
set -euo pipefail
cd "$(dirname "$0")/.."

build=${1:-build}
out=${2:-dist}
version=$(sed -n 's/^project(raymini VERSION \([0-9][0-9.]*\).*/\1/p' CMakeLists.txt)
[ -n "$version" ] || { echo "no version in CMakeLists.txt" >&2; exit 1; }

case "$(uname -s)" in
    Darwin) os=macos ;;
    Linux) os=linux ;;
    *) os=$(uname -s | tr '[:upper:]' '[:lower:]') ;;
esac
name="raymini-$version-$os-$(uname -m)"
staging="$out/$name"

# The binary must be the one this version claims to be.
built=$("$build/raymini-cli" --version)
[ "$built" = "raymini $version" ] || { echo "$build/raymini-cli says '$built', expected 'raymini $version'" >&2; exit 1; }

rm -rf "$staging"
mkdir -p "$staging/bin" "$staging/models"
cp "$build/raymini-cli" "$staging/bin/"
# The viewer is optional: a build with -DRAYMINI_BUILD_GUI=OFF has none.
[ -x "$build/raymini" ] && cp "$build/raymini" "$staging/bin/"
cp models/*.off models/*.obj models/*.mtl models/orientation.txt "$staging/models/"
cp README.md CHANGELOG.md "$staging/"

cat > "$staging/RUNNING.txt" <<TXT
raymini $version — $os $(uname -m)

  bin/raymini-cli teapot --ground --aa 2 --out teapot.png
  bin/raymini-cli --help

raymini-cli needs nothing else: it renders to a PNG without a display, and a
bare model name (teapot, ram, minion, cube.obj) resolves inside models/ when
you run it from this directory.

bin/raymini, the viewer, is here only when the archive was built with it, and
it needs GLFW and GLM installed (brew install glfw glm, or apt-get install
libglfw3-dev libglm-dev) plus a display.

On macOS, an archive downloaded from a browser is quarantined, and these
binaries carry no Apple Developer ID: the first run is killed (exit 137) and
the file may be moved out of the way. Clear the quarantine flag once, after
unpacking:

  xattr -dr com.apple.quarantine .

Nothing else is needed; the binaries are ad-hoc signed, which is what arm64
requires to run at all, but not what Gatekeeper wants to see.
TXT

tar -czf "$out/$name.tar.gz" -C "$out" "$name"
rm -rf "$staging"
echo "$out/$name.tar.gz"
