#!/usr/bin/env bash
# An outside yardstick: the working tree against other engines that have a published rating, via fastchess.
#   scripts/outside.sh [engines-file=external/engines/list.txt]
# The file names one opponent a line: name, rating, path to its binary (relative to the file). '#' starts a comment.
# It's played as one match per opponent, each from the same openings, under the baseline's conditions
# (8+0.08, Hash 16, the usual book, no adjudication). The result is the rating at which our score against
# these opponents would be the expected one. A rough guide, not a rating: docs/testing.md says why.
# Results go to matches/<date>-<time>-<sha>-outside/, with a folder per opponent, & a row in matches/README.md.
# Ctrl-C pauses it: finished games are kept, & RESUME=<folder> scripts/outside.sh carries on from there.
# env overrides: GAMES (per opponent, default 300), FASTCHESS, BOOK, MATCHES, CONCURRENCY, SEED, WHAT
set -euo pipefail
cd "$(dirname "$0")/.."

# fastchess, the book, the opponents and the results live in the main checkout, as for match.sh
MAIN=$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")
FASTCHESS=${FASTCHESS:-$MAIN/external/fastchess/fastchess}
BOOK=${BOOK:-$MAIN/books/8moves_v3.pgn}
MATCHES=${MATCHES:-$MAIN/matches}
CONCURRENCY=${CONCURRENCY:-6}
TC=8+0.08
[[ -x $FASTCHESS ]] || { echo "no fastchess at $FASTCHESS (set FASTCHESS=...)" >&2; exit 1; }
[[ -f $BOOK ]] || { echo "no opening book at $BOOK (set BOOK=...)" >&2; exit 1; }

RESUME=${RESUME:-}
if [[ -n $RESUME ]]; then
    NAME=$(basename "$RESUME")
    OUT=$MATCHES/$NAME
    [[ -f $OUT/engines.txt && -f $OUT/info.txt && -x $OUT/maiolica ]] || { echo "no match to resume in $OUT" >&2; exit 1; }
    [[ $(tail -n 1 "$OUT/info.txt") == paused:* ]] || { echo "$NAME isn't paused: it has finished, or it's still running" >&2; exit 1; }
    DEV_SHA=$(sed -n 's/^dev:  *//p' "$OUT/info.txt")
    GAMES=$(sed -n 's/^games: \([0-9]*\) against each.*/\1/p' "$OUT/info.txt")
    SEED=$(sed -n 's/^seed: //p' "$OUT/info.txt")
    WHAT=$(sed -n 's/^what: //p' "$OUT/info.txt")
else
    LIST=${1:-$MAIN/external/engines/list.txt}
    [[ -f $LIST ]] || { echo "no list of opponents at $LIST: see 'Against other engines' in docs/testing.md" >&2; exit 1; }
    GAMES=${GAMES:-300}
    SEED=${SEED:-$(( (RANDOM << 15) | RANDOM ))}
    DEV_SHA=$(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo +dirty)
    NAME=$(date +%Y%m%d-%H%M%S)-$DEV_SHA-outside
    OUT=$MATCHES/$NAME
    mkdir -p "$OUT"
    # the list, with the paths made whole, so the folder still says who played once the list has moved on
    while read -r ENGINE RATING CMD _; do
        [[ -n $ENGINE && $ENGINE != \#* ]] || continue
        [[ $CMD == /* ]] || CMD=$(dirname "$LIST")/$CMD
        [[ -x $CMD && $RATING =~ ^[0-9]+$ ]] || { echo "bad line for '$ENGINE' in $LIST (want: name rating binary)" >&2; exit 1; }
        echo "$ENGINE $RATING $CMD"
    done < "$LIST" > "$OUT/engines.txt"
    [[ -s $OUT/engines.txt ]] || { echo "no opponents in $LIST" >&2; exit 1; }

    cmake --preset release >/dev/null
    cmake --build --preset release
    cp cmake-build-release/maiolica "$OUT/maiolica"
fi

# the same warnings as match.sh: a timed match is only as fair as the machine is steady
NOTES=""
if command -v pmset >/dev/null; then
    POWER=$(pmset -g batt; pmset -g)
    [[ $POWER != *"Battery Power"* ]] || NOTES+="warning: running on battery"$'\n'
    [[ ! $POWER =~ lowpowermode\ +1 ]] || NOTES+="warning: low power mode is on"$'\n'
fi
LOAD=$(uptime | sed 's/.*load averages*: *//; s/[, ].*//')
awk -v load="$LOAD" 'BEGIN {exit load > 2}' || NOTES+="warning: the machine was already busy (load $LOAD)"$'\n'
AWAKE=$(command -v caffeinate || true)

if [[ -z $RESUME ]]; then
    { echo "dev:  $DEV_SHA"
      echo "opponents: $(awk '{printf "%s%s (%s)", (NR > 1 ? ", " : ""), $1, $2}' "$OUT/engines.txt")"
      echo "games: $GAMES against each, tc: $TC, Hash 16, book: $BOOK"
      echo "seed: $SEED"
      [[ -z ${WHAT:-} ]] || echo "what: $WHAT"
      echo "fastchess: $("$FASTCHESS" --version | sed 's/^fastchess //')"
      printf '%s' "$NOTES"; } | tee "$OUT/info.txt"
else
    { echo "resumed: $(date '+%Y-%m-%d %H:%M')"; printf '%s' "$NOTES"; } | tee -a "$OUT/info.txt"
fi

# games counted so far against one opponent: fastchess keeps the score in its config.json
played() {
    [[ -f $1/config.json ]] || { echo 0; return; }
    sed -n '/"stats"/,$p' "$1/config.json" | awk -F'[:,]' '/"(wins|losses|draws)"/ {n += $2} END {print n + 0}'
}

# one match per opponent (fastchess can only pick up a paused match between two engines)
PAUSED=""
trap ':' INT
while read -r ENGINE RATING CMD <&3; do
    D=$OUT/$ENGINE
    mkdir -p "$D"
    (( $(played "$D") < GAMES )) || continue
    if [[ -f $D/config.json ]]; then
        ARGS=(-config file="$D/config.json")
    else
        ARGS=(-engine cmd="$OUT/maiolica" name=maiolica
              -engine cmd="$CMD" name="$ENGINE"
              -each tc="$TC" option.Hash=16
              -openings file="$BOOK" format="${BOOK##*.}" order=random -srand "$SEED"
              -games 2 -repeat -rounds $(( (GAMES + 1) / 2 )) -concurrency "$CONCURRENCY"
              -pgnout file="$D/games.pgn"
              -config outname="$D/config.json"
              -recover -ratinginterval 10)
    fi
    echo "== maiolica vs $ENGINE ($RATING)"
    [[ ! -f $D/log.txt.gz ]] || gunzip -f "$D/log.txt.gz"
    $AWAKE ${AWAKE:+-i} "$FASTCHESS" "${ARGS[@]}" < /dev/null 2>&1 | tee -a -i "$D/log.txt" | grep --line-buffered -E '^(Results|Elo|Games|Error)' || true   # the rest is in the log
    gzip -f "$D/log.txt"
    (( $(played "$D") >= GAMES )) || { PAUSED=1; break; }
done 3< "$OUT/engines.txt"
trap - INT

# result.txt: a line per opponent, then the one rating that explains the whole score.
# A game not decided on the board (time forfeit, illegal move, crash) still counts, but is shown, by who lost it
{ printf '%-18s %6s %6s %-13s %7s %6s %10s   %s\n' Opponent Rating Games W-L-D Score Gap "Puts us at" "Not decided on the board (us/them)"
  while read -r ENGINE RATING CMD; do
      D=$OUT/$ENGINE
      [[ -f $D/config.json ]] || continue
      WLD=$(sed -n '/"stats"/,$p' "$D/config.json" | awk -F'[:,]' '/"wins"/ {w = $2} /"losses"/ {l = $2} /"draws"/ {d = $2} END {print w + 0, l + 0, d + 0}')
      # who lost the games that didn't end normally: the result says who won
      ODD=$(awk -F'"' '/^\[White /  {white = $2}
                       /^\[Result / {result = $2}
                       /^\[Termination / && $2 != "normal" && $2 != "unterminated" {
                           loser = (result == "1-0") ? "black" : (result == "0-1") ? "white" : "nobody"
                           if (loser == "nobody") next
                           ((loser == "white") == (white == "maiolica")) ? us++ : them++ }
                       END {printf "%d/%d", us, them}' "$D/games.pgn" 2>/dev/null || echo "0/0")
      echo "$ENGINE $RATING $WLD $ODD"
  done < "$OUT/engines.txt" | awk '
      function expected(gap) { return 1 / (1 + 10 ^ (-gap / 400)) }
      { n = $3 + $4 + $5; if (n == 0) next
        pts = $3 + $5 / 2; p = pts / n
        gap = (p <= 0) ? "-inf" : (p >= 1) ? "+inf" : sprintf("%+.0f", -400 * log(1 / p - 1) / log(10))
        at = (p <= 0 || p >= 1) ? "-" : sprintf("%.0f", $2 + gap)
        printf "%-18s %6d %6d %-13s %6.1f%% %6s %10s   %s\n", $1, $2, n, $3 "-" $4 "-" $5, 100 * p, gap, at, $6
        rating[++k] = $2; games[k] = n; total += pts; all += n
        variance += n * (($3 + $5 / 4) / n - p * p) }   # the spread of one game, from the results themselves
      END {
        if (!k || total <= 0 || total >= all) { print "\nNo overall figure: no games, or all won or all lost"; exit }
        lo = 0; hi = 4000   # the rating at which the expected score against these opponents is the score we got
        for (i = 0; i < 60; i++) { mid = (lo + hi) / 2; e = 0
            for (j = 1; j <= k; j++) e += games[j] * expected(mid - rating[j])
            if (e < total) lo = mid; else hi = mid }
        for (j = 1; j <= k; j++) { e = expected(mid - rating[j]); slope += games[j] * e * (1 - e) * log(10) / 400 }
        printf "\nOverall: %d games, %.1f%%. Performance %.0f, +/- %.0f from luck alone\n", all, 100 * total / all, mid, 1.96 * sqrt(variance) / slope
      }'
} > "$OUT/result.txt"
cat "$OUT/result.txt"

TOTAL=$(awk '/^Overall:/ {print $2}' "$OUT/result.txt")
PERF=$(awk '/^Overall:/ {sub(/,$/, "", $6); printf "about %s (± %s from luck alone)", $6, $8}' "$OUT/result.txt")
WLD=$(awk '$4 ~ /^[0-9]+-[0-9]+-[0-9]+$/ {split($4, s, "-"); w += s[1]; l += s[2]; d += s[3]} END {printf "%d-%d-%d", w, l, d}' "$OUT/result.txt")
VERDICT="-"
if [[ -n $PAUSED ]]; then
    VERDICT="paused at ${TOTAL:-0} games"
    echo "paused: $(date '+%Y-%m-%d %H:%M'), ${TOTAL:-0} games played" >> "$OUT/info.txt"
fi

# one row in the table of all matches. The Elo column holds the rating, since there's no base to be ahead of
README=$MATCHES/README.md
if [[ -f $README ]]; then
    grep -vF "\`$NAME\`" "$README" > "$README.tmp" && mv "$README.tmp" "$README"
    [[ -z $(tail -c 1 "$README") ]] || echo >> "$README"
    echo "| ${NAME:0:4}-${NAME:4:2}-${NAME:6:2} | ${WHAT:-Outside engines: $(awk '{printf "%s%s", (NR > 1 ? ", " : ""), $1}' "$OUT/engines.txt")} | \`$DEV_SHA\` | other engines | outside | $TC, Hash 16 | ${TOTAL:-0} | $WLD | ${PERF:--} | $VERDICT | \`$NAME\` |" >> "$README"
fi
echo "results are in $OUT"

if [[ -n $PAUSED ]]; then
    echo "paused. To carry on: RESUME=$NAME scripts/outside.sh"
    exit 130
fi
