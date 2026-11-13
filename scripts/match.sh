#!/usr/bin/env bash
# Match: working tree ("dev") vs a git ref ("base"), via fastchess.
#   scripts/match.sh [base-ref=master] [gain|nonreg|elo] [tc=8+0.08]
# gain & nonreg are SPRTs, which stop as soon as they're sure. elo plays a fixed number of
# games instead, to measure the gap: an SPRT's own Elo figure is biased by stopping early.
# env overrides: FASTCHESS, BOOK, CONCURRENCY, SEED (same seed, same openings), GAMES (elo only)
set -euo pipefail
cd "$(dirname "$0")/.."

BASE_REF=${1:-master}
MODE=${2:-gain}
TC=${3:-8+0.08}
FASTCHESS=${FASTCHESS:-external/fastchess/fastchess}
BOOK=${BOOK:-books/8moves_v3.pgn}
CONCURRENCY=${CONCURRENCY:-6}   # M1 Pro has 8 P-cores; leave a bit of headroom
SEED=${SEED:-$(( (RANDOM << 15) | RANDOM ))}   # picks the openings; recorded so a match can be repeated
GAMES=${GAMES:-2000}

case $MODE in
  gain)   SPRT="elo0=0 elo1=10" ;;    # does the change actually gain?
  nonreg) SPRT="elo0=-10 elo1=0" ;;   # refactors/simplifications: just don't lose
  elo)    SPRT="" ;;                  # no early stop: how big is the gap?
  *) echo "mode must be gain, nonreg or elo, got '$MODE'" >&2; exit 1 ;;
esac

# games come in pairs (same opening, colours swapped), so a round is 2 games
if [[ $MODE == elo ]]; then
    LENGTH="-rounds $(( (GAMES + 1) / 2 ))"
    DESC="$GAMES games, no SPRT"
else
    LENGTH="-rounds 10000 -sprt $SPRT alpha=0.05 beta=0.05 model=logistic"
    DESC=$SPRT
fi

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
  echo "mode: $MODE ($DESC), tc: $TC, book: $BOOK"
  echo "seed: $SEED"; } | tee "$OUT/info.txt"

# book format is just the file extension (pgn or epd)
# no resign or draw adjudication: on 790 of our own games the usual rules got 17-20 results wrong
STATUS=0
"$FASTCHESS" \
  -engine cmd="$OUT/dev" name=dev \
  -engine cmd="$OUT/base" name=base \
  -each tc="$TC" option.Hash=16 \
  -openings file="$BOOK" format="${BOOK##*.}" order=random -srand "$SEED" \
  -games 2 -repeat $LENGTH -concurrency "$CONCURRENCY" \
  -pgnout file="$OUT/games.pgn" \
  -config outname="$OUT/config.json" \
  -recover \
  2>&1 | tee "$OUT/log.txt" || STATUS=$?

# how the games ended: anything but 'normal' (time forfeit, illegal move, crash, disconnect)
# means a result that wasn't decided on the board
TERMS=$(grep '^\[Termination ' "$OUT/games.pgn" | sort | uniq -c)
printf '\nHow the games ended:\n%s\n' "$TERMS"
if grep -qv '"normal"\]$' <<< "$TERMS"; then
    echo "not every game ended normally: look in $OUT/games.pgn" >&2
    exit 1
fi
exit $STATUS
