#!/usr/bin/env bash
# Regenerate the README's step-by-step gallery (docs/evolution/): the ram
# rendered by today's raytracer, each picture switching on the next experiment
# of claudedocs/EXPERIMENTS.md, same camera throughout. Steps 3 (BVH) and 4
# (threads) change the time, not the picture: they are timed instead, and the
# script checks that their pixels are unchanged.
#
#   cmake --build build && scripts/render-evolution.sh
set -euo pipefail
cd "$(dirname "$0")/.."

cli=build/raymini-cli
out=docs/evolution
view=(ram --yaw -35 --pitch 15 --size 384x256)
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
mkdir -p "$out"

render() {
    local name=$1
    shift
    printf '%-18s ' "$name"
    "$cli" "${view[@]}" "$@" --out "$out/$name.png" | sed -n 's/^render  //p'
}

render 00-start          --no-shadows --no-specular
render 01-ground-shadows --ground --no-specular
render 02-specular       --ground
render 05-antialiasing   --ground --aa 2
render 06-soft-shadows   --ground --aa 2 --shadow-samples 8
render 07-reflections    --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4
render 08-occlusion      --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4 --ao 8
render 08-occlusion-ao   --ground --aa 2 --mode ao --ao 8
# The stretch of experiment 7, done after 8: the model as clear glass.
render 07b-refraction    --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4 --ao 8 --transparency 1
printf '%-18s ' 07b-teapot
"$cli" teapot --yaw 25 --pitch 20 --size 384x256 --ground --aa 2 --shadow-samples 8 --transparency 1 \
    --out "$out/07b-teapot.png" | sed -n 's/^render  //p'

timed() {
    local label=$1 png=$2
    shift 2
    printf '%-18s ' "$label"
    "$cli" "${view[@]}" "$@" --out "$tmp/$png" | sed -n 's/^render  .* in \([0-9.]*s\).*/\1/p'
}

# The three files must hold the same bytes.
same() {
    if cmp -s "$1" "$2" && cmp -s "$2" "$3"; then
        echo "   same pixels"
    else
        echo "   the pixels differ: $*" >&2
        exit 1
    fi
}

echo "3. BVH, picture of step 2 on one thread:"
timed "   brute force" brute.png --ground --threads 1 --no-bvh
timed "   bvh" bvh.png --ground --threads 1
same "$tmp/brute.png" "$tmp/bvh.png" "$out/02-specular.png"

echo "4. Threads, picture of step 6:"
timed "   1 thread" one.png --ground --aa 2 --shadow-samples 8 --threads 1
timed "   one per core" all.png --ground --aa 2 --shadow-samples 8
same "$tmp/one.png" "$tmp/all.png" "$out/06-soft-shadows.png"
