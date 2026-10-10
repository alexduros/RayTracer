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

# Before experiment 9 the renderer wrote linear radiance straight into bytes:
# those steps keep that look (--display linear). From 9 on, the default display.
legacy() {
    local name=$1
    shift
    render "$name" --display linear "$@"
}

legacy 00-start          --no-shadows --no-specular
legacy 01-ground-shadows --ground --no-specular
legacy 02-specular       --ground
legacy 05-antialiasing   --ground --aa 2
legacy 06-soft-shadows   --ground --aa 2 --shadow-samples 8
legacy 07-reflections    --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4
legacy 08-occlusion      --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4 --ao 8
legacy 08-occlusion-ao   --ground --aa 2 --mode ao --ao 8
# The stretch of experiment 7, done after 8: the model as clear glass. Until
# step 15 glass stopped shadow rays: the pictures before it keep that
# (--opaque-shadows).
legacy 07b-refraction    --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4 --ao 8 --transparency 1 \
    --opaque-shadows
printf '%-18s ' 07b-teapot
"$cli" teapot --yaw 25 --pitch 20 --size 384x256 --display linear --ground --aa 2 --shadow-samples 8 --transparency 1 \
    --opaque-shadows \
    --out "$out/07b-teapot.png" | sed -n 's/^render  //p'

# Experiment 9: the same picture as step 8, now through the display (auto
# exposure, a tone curve, sRGB): ACES, and Reinhard et al.'s photographic
# operator (the metered exposure and their curve).
render 09-display        --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4 --ao 8
render 09-reinhard       --ground --aa 2 --shadow-samples 8 --ground-reflectivity 0.4 --ao 8 --tonemap reinhard

# Experiment 11: two spheres given by their equation stand next to the mesh.
render 11-primitives     --ground --aa 2 --shadow-samples 8 --ao 8 --opaque-shadows \
    --sphere -1.05 -0.62 0.45 0.38 mirror --sphere 1.15 -0.66 0.3 0.34 glass

# Experiment 12: the only model that carries texture coordinates, with its
# texture, and the coordinates themselves.
printf '%-18s ' 12-texture
"$cli" spot --yaw -150 --pitch 12 --size 384x256 --ground --aa 2 --shadow-samples 8 --ao 8 \
    --out "$out/12-texture.png" | sed -n 's/^render  //p'
printf '%-18s ' 12-uv
"$cli" spot --yaw -150 --pitch 12 --size 384x256 --mode uv --aa 2 --out "$out/12-uv.png" | sed -n 's/^render  //p'

# Experiment 13: the composition of step 11 without its floor, in a world.
# Where the mirror and the glass showed a black sky, they show a place. Then
# the ram as a perfect mirror, and the chrome teapot of Blinn & Newell's
# figure 8.
world=(--aa 2 --environment venice_sunset)
render 13-environment    "${world[@]}" --shadow-samples 8 --ao 8 \
    --sphere -1.05 -0.62 0.45 0.38 mirror --sphere 1.15 -0.66 0.3 0.34 glass
render 13-chrome         "${world[@]}" --reflectivity 1
printf '%-18s ' 13-teapot
"$cli" teapot --yaw 25 --pitch 20 --size 384x256 "${world[@]}" --reflectivity 1 \
    --out "$out/13-teapot.png" | sed -n 's/^render  //p'

# Step 14: Spot again, the model with coordinates. The picture of step 12
# with a height map over her skin, the tilted normals themselves, the same
# dimples in chrome in the Venice panorama, and a close-up with the height
# doubled, where the smooth silhouette gives the trick away.
spot=(spot --yaw -150 --pitch 12 --size 384x256 --aa 2 --bump dimples)
printf '%-18s ' 14-bump
"$cli" "${spot[@]}" --ground --shadow-samples 8 --ao 8 --out "$out/14-bump.png" | sed -n 's/^render  //p'
printf '%-18s ' 14-normals
"$cli" "${spot[@]}" --mode normals --out "$out/14-normals.png" | sed -n 's/^render  //p'
printf '%-18s ' 14-chrome
"$cli" "${spot[@]}" --distance 1.5 --reflectivity 1 --environment venice_sunset --out "$out/14-chrome.png" |
    sed -n 's/^render  //p'
printf '%-18s ' 14-silhouette
"$cli" spot --yaw -115 --pitch 5 --distance 0.9 --size 384x256 --aa 3 --bump dimples --bump-scale 0.02 --ground \
    --shadow-samples 8 --out "$out/14-silhouette.png" | sed -n 's/^render  //p'

timed() {
    local label=$1 png=$2
    shift 2
    printf '%-18s ' "$label"
    "$cli" "${view[@]}" --display linear "$@" --out "$tmp/$png" | sed -n 's/^render  .* in \([0-9.]*s\).*/\1/p'
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
