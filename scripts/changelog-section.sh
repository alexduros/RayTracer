#!/usr/bin/env bash
# Print the CHANGELOG.md section of one version, without its heading: the
# release notes. Used by .github/workflows/release.yml.
#
#   scripts/changelog-section.sh 0.1.0
set -euo pipefail
cd "$(dirname "$0")/.."
version=${1:?usage: changelog-section.sh <version>}

awk -v want="## [$version]" '
  index($0, want) == 1 { inside = 1; next }
  inside && (/^## / || /^\[[^]]+\]: /) { exit }   # the next version, or the link definitions
  inside { lines[++n] = $0 }
  END {
    first = 1
    while (first <= n && lines[first] ~ /^[[:space:]]*$/) first++
    while (n > 0 && lines[n] ~ /^[[:space:]]*$/) n--
    for (i = first; i <= n; i++) print lines[i]
  }
' CHANGELOG.md
