#!/usr/bin/env bash
# Speed A/B: interleaved runs of two builds, reporting instructions, cycles & wall time.
#   scripts/ab.sh A B [runs=7] [bench|perft]
# A and B are git refs, or '.' for the working tree. e.g. scripts/ab.sh HEAD . 9
# Instructions retired barely move run to run, so for a change with an identical bench
# signature they're the metric to trust; wall time on a busy laptop wobbles by a few %.
# macOS only for now (uses /usr/bin/time -l).
set -euo pipefail
cd "$(dirname "$0")/.."

A=${1:?usage: scripts/ab.sh A B [runs] [bench|perft]}
B=${2:?usage: scripts/ab.sh A B [runs] [bench|perft]}
RUNS=${3:-7}
WHAT=${4:-bench}

# build a ref (or the working tree) and print its build dir
build() {
    if [[ $1 == . ]]; then
        cmake --preset release >/dev/null && cmake --build --preset release >/dev/null
        echo cmake-build-release
        return
    fi
    local sha dir
    sha=$(git rev-parse --short "$1^{commit}")
    dir=cmake-build-ab/$sha
    if [[ ! -x $dir/maiolica ]]; then # cached, since a ref never changes
        rm -rf "$dir" && mkdir -p "$dir/src"
        git archive "$sha" | tar -x -C "$dir/src"
        cmake -S "$dir/src" -B "$dir" -DCMAKE_BUILD_TYPE=Release >/dev/null
        cmake --build "$dir" -j >/dev/null
    fi
    echo "$dir"
}

# run one build once, printing 'instructions cycles seconds signature'
run() {
    local out
    if [[ $WHAT == bench ]]; then
        out=$( { /usr/bin/time -l "$1/maiolica" bench; } 2>&1 )
    else
        out=$( { /usr/bin/time -l "$1/perft_tests" quick; } 2>&1 )
    fi
    awk '/instructions retired/ {i=$1} /cycles elapsed/ {c=$1} / real / {r=$1}
         /^Nodes searched/ {s=$4} /leaf nodes in/ {s=$1}
         END {print i, c, r, s}' <<< "$out"
}

echo "building $A and $B..."
DIR_A=$(build "$A")
DIR_B=$(build "$B")

RES_A=$(mktemp) RES_B=$(mktemp)
trap 'rm -f "$RES_A" "$RES_B"' EXIT
for ((n = 1; n <= RUNS; n++)); do # alternate which goes first, so drift hits both equally
    if ((n % 2)); then run "$DIR_A" >> "$RES_A"; run "$DIR_B" >> "$RES_B"
    else run "$DIR_B" >> "$RES_B"; run "$DIR_A" >> "$RES_A"; fi
    printf '.'
done
echo

SIG_A=$(awk 'NR==1 {print $4}' "$RES_A")
SIG_B=$(awk 'NR==1 {print $4}' "$RES_B")
echo "$WHAT nodes: A=$SIG_A B=$SIG_B $([[ $SIG_A == "$SIG_B" ]] && echo '(identical)' || echo '(DIFFERENT - search changed!)')"

# min & median of a column
stats() { cut -d' ' -f"$2" "$1" | sort -g | awk '{v[NR]=$1} END {printf "%s %s", v[1], v[int((NR+1)/2)]}'; }

printf '%-14s %16s %16s %16s %16s %9s\n' "" "A min" "A median" "B min" "B median" "B vs A"
for col in 1:instructions 2:cycles 3:seconds; do
    read -r a_min a_med <<< "$(stats "$RES_A" "${col%%:*}")"
    read -r b_min b_med <<< "$(stats "$RES_B" "${col%%:*}")"
    printf '%-14s %16s %16s %16s %16s %+8.2f%%\n' "${col#*:}" "$a_min" "$a_med" "$b_min" "$b_med" \
        "$(awk -v a="$a_min" -v b="$b_min" 'BEGIN {print (b - a) * 100 / a}')"
done
