#!/usr/bin/env bash
# SPRT match: working tree ("dev") vs a git ref ("base"), via fastchess.
#   scripts/match.sh [base-ref=master] [gain|nonreg] [tc=8+0.08]
# env overrides: FASTCHESS, BOOK, CONCURRENCY
set -euo pipefail
cd "$(dirname "$0")/.."

BASE_REF=${1:-master}
MODE=${2:-gain}
TC=${3:-8+0.08}
FASTCHESS=${FASTCHESS:-external/fastchess/fastchess}
BOOK=${BOOK:-books/8moves_v3.pgn}
CONCURRENCY=${CONCURRENCY:-6}   # M1 Pro has 8 P-cores; leave a bit of headroom

case $MODE in
  gain)   SPRT="elo0=0 elo1=10" ;;    # does the change actually gain?
  nonreg) SPRT="elo0=-10 elo1=0" ;;   # refactors/simplifications: just don't lose
  *) echo "mode must be gain or nonreg, got '$MODE'" >&2; exit 1 ;;
esac

[[ -x $FASTCHESS ]] || { echo "no fastchess at $FASTCHESS (set FASTCHESS=...)" >&2; exit 1; }
[[ -f $BOOK ]] || { echo "no opening book at $BOOK (set BOOK=...)" >&2; exit 1; }
git rev-parse --verify --quiet "$BASE_REF^{commit}" >/dev/null || { echo "unknown ref '$BASE_REF'" >&2; exit 1; }

OUT=matches/$(date +%Y%m%d-%H%M%S)-${BASE_REF//\//_}
mkdir -p "$OUT"

# build dev from the working tree with the normal release preset
cmake --preset release >/dev/null
cmake --build --preset release
cp cmake-build-release/maiolica "$OUT/dev"

# build the base ref from a clean export, so old CMakeLists still work
TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT
git archive "$BASE_REF" | tar -x -C "$TMP"
cmake -S "$TMP" -B "$TMP/build" -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$TMP/build" -j
cp "$TMP/build/maiolica" "$OUT/base"

# record what we're comparing, so the folder makes sense later
{ echo "dev:  $(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo ' + uncommitted changes')"
  echo "base: $BASE_REF ($(git rev-parse --short "$BASE_REF"))"
  echo "mode: $MODE ($SPRT), tc: $TC, book: $BOOK"; } | tee "$OUT/info.txt"

# book format is just the file extension (pgn or epd)
"$FASTCHESS" \
  -engine cmd="$OUT/dev" name=dev \
  -engine cmd="$OUT/base" name=base \
  -each tc="$TC" option.Hash=16 \
  -openings file="$BOOK" format="${BOOK##*.}" order=random \
  -games 2 -repeat -rounds 10000 -concurrency "$CONCURRENCY" \
  -sprt $SPRT alpha=0.05 beta=0.05 model=logistic \
  -pgnout file="$OUT/games.pgn" \
  -config outname="$OUT/config.json" \
  -recover \
  2>&1 | tee "$OUT/log.txt"
