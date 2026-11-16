#!/usr/bin/env bash
# Match: working tree ("dev") vs a git ref ("base"), via fastchess.
#   scripts/match.sh [base-ref=master] [gain|nonreg|elo] [tc=8+0.08]
# gain & nonreg are SPRTs, which stop as soon as they're sure. elo plays a fixed number of
# games instead, to measure the gap: an SPRT's own Elo figure is biased by stopping early.
# Results go to matches/<date>-<time>-<dev sha>-vs-<base ref>-<mode>/, and a row is added to
# the table in matches/README.md. WHAT="..." fills in that row's description.
# An SPRT has no fixed length: a change worth about 0 Elo needs ~2,650 games on average and can
# take far more. CAP=<games> ends it there if it hasn't decided. Pick one before you start (or
# decide not to): docs/testing.md has the lengths to expect and what a cap costs.
# NODES=<n> swaps the clock for a fixed number of nodes a move: a quick screen that comes out the same
# however busy the machine is, but is blind to speed & time management. It doesn't replace the timed test.
# env overrides: FASTCHESS, BOOK, MATCHES, CONCURRENCY, SEED (same seed, same openings), GAMES (elo only),
#                CAP (gain & nonreg only), NODES (in place of a time control)
set -euo pipefail
cd "$(dirname "$0")/.."

BASE_REF=${1:-master}
MODE=${2:-gain}
TC=${3:-8+0.08}
# fastchess, the book and the results live in the main checkout, so every worktree shares them
MAIN=$(dirname "$(git rev-parse --path-format=absolute --git-common-dir)")
FASTCHESS=${FASTCHESS:-$MAIN/external/fastchess/fastchess}
BOOK=${BOOK:-$MAIN/books/8moves_v3.pgn}
MATCHES=${MATCHES:-$MAIN/matches}
CONCURRENCY=${CONCURRENCY:-6}   # M1 Pro has 8 P-cores; leave a bit of headroom
SEED=${SEED:-$(( (RANDOM << 15) | RANDOM ))}   # picks the openings; recorded so a match can be repeated
GAMES=${GAMES:-2000}
CAP=${CAP:-}   # gain & nonreg: give up waiting for a verdict after this many games
[[ -z $CAP || $CAP =~ ^[1-9][0-9]*$ ]] || { echo "CAP must be a number of games, got '$CAP'" >&2; exit 1; }
NODES=${NODES:-}   # every move searches this many nodes, & there is no clock
[[ -z $NODES || $NODES =~ ^[1-9][0-9]*$ ]] || { echo "NODES must be a number of nodes a move, got '$NODES'" >&2; exit 1; }
[[ -z $NODES || $# -lt 3 ]] || { echo "NODES takes the place of the time control: give one or the other" >&2; exit 1; }
if [[ -n $NODES ]]; then
    LIMIT="nodes=$NODES" LIMIT_NAME=limit LIMIT_DESC="$NODES nodes a move"
else
    LIMIT="tc=$TC" LIMIT_NAME=tc LIMIT_DESC=$TC
fi

case $MODE in
  gain)   SPRT="elo0=0 elo1=10"  BOUNDS=" [0, 10]" ;;    # does the change actually gain?
  nonreg) SPRT="elo0=-10 elo1=0" BOUNDS=" [-10, 0]" ;;   # refactors/simplifications: just don't lose
  elo)    SPRT="" BOUNDS="" ;;                           # no early stop: how big is the gap?
  *) echo "mode must be gain, nonreg or elo, got '$MODE'" >&2; exit 1 ;;
esac

# games come in pairs (same opening, colours swapped), so a round is 2 games
if [[ $MODE == elo ]]; then
    LENGTH="-rounds $(( (GAMES + 1) / 2 ))"
    DESC="$GAMES games, no SPRT"
else
    LENGTH="-rounds $(( (${CAP:-20000} + 1) / 2 )) -sprt $SPRT alpha=0.05 beta=0.05 model=logistic"
    DESC="$SPRT, ${CAP:+capped at $CAP games}"
    DESC=${DESC%, }
    # said every time, so that going without a cap is a choice and not an oversight
    [[ -n $CAP ]] || CAPNOTE="cap:  none. A change worth ~0 Elo runs ~2,650 games (about 3 hours) & can go on much longer: see CAP in docs/testing.md"
fi

[[ -x $FASTCHESS ]] || { echo "no fastchess at $FASTCHESS (set FASTCHESS=...)" >&2; exit 1; }
[[ -f $BOOK ]] || { echo "no opening book at $BOOK (set BOOK=...)" >&2; exit 1; }
git rev-parse --verify --quiet "$BASE_REF^{commit}" >/dev/null || { echo "unknown ref '$BASE_REF'" >&2; exit 1; }

# a local branch only moves when it's pulled, so say so if its remote copy has gone on ahead
NOTES=""
git fetch --quiet 2>/dev/null || true
UPSTREAM=$(git rev-parse --abbrev-ref "$BASE_REF@{upstream}" 2>/dev/null || true)
if [[ -n $UPSTREAM ]]; then
    BEHIND=$(git rev-list --count "$BASE_REF..$UPSTREAM")
    ((BEHIND == 0)) || NOTES+="warning: $BASE_REF is $BEHIND commits behind $UPSTREAM. Pull it, or use $UPSTREAM as the base"$'\n'
fi

# a match is only as fair as the machine is steady: say so if it's on battery, saving power,
# or busy before we've even started (6 games at once leave 2 fast cores spare)
# (none of that touches a fixed-node game: the same nodes get searched, just more slowly)
if [[ -n $NODES ]]; then
    NOTES+="fixed nodes: a screen, blind to speed & time management. The timed test still decides"$'\n'
elif command -v pmset >/dev/null; then
    POWER=$(pmset -g batt; pmset -g)
    [[ $POWER != *"Battery Power"* ]] || NOTES+="warning: running on battery"$'\n'
    [[ ! $POWER =~ lowpowermode\ +1 ]] || NOTES+="warning: low power mode is on"$'\n'
fi
LOAD=$(uptime | sed 's/.*load averages*: *//; s/[, ].*//')
[[ -n $NODES ]] || awk -v load="$LOAD" 'BEGIN {exit load > 2}' || NOTES+="warning: the machine was already busy (load $LOAD)"$'\n'
AWAKE=$(command -v caffeinate || true)   # macOS: keeps the laptop from sleeping mid-match

DEV_SHA=$(git rev-parse --short HEAD)$(git diff --quiet HEAD || echo +dirty)   # +dirty: uncommitted changes
BASE_SHA=$(git rev-parse --short "$BASE_REF^{commit}")
NAME=$(date +%Y%m%d-%H%M%S)-$DEV_SHA-vs-${BASE_REF//\//_}-$MODE${NODES:+-nodes}
OUT=$MATCHES/$NAME
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
{ echo "dev:  $DEV_SHA"
  echo "base: $BASE_REF ($BASE_SHA)"
  echo "mode: $MODE ($DESC), $LIMIT_NAME: $LIMIT_DESC, book: $BOOK"
  echo "seed: $SEED"
  [[ -z ${CAPNOTE:-} ]] || echo "$CAPNOTE"
  echo "fastchess: $("$FASTCHESS" --version | sed 's/^fastchess //')"
  printf '%s' "$NOTES"; } | tee "$OUT/info.txt"

# book format is just the file extension (pgn or epd)
# no resign or draw adjudication: on 790 of our own games the usual rules got 17-20 results wrong
# Ctrl-C: fastchess winds the match down, and what was played still gets written up
# (it's asked for the score after every pair, so there's always a recent one to keep)
STOPPED=""
trap 'STOPPED=1' INT
STATUS=0
$AWAKE ${AWAKE:+-i} "$FASTCHESS" \
  -engine cmd="$OUT/dev" name=dev \
  -engine cmd="$OUT/base" name=base \
  -each "$LIMIT" option.Hash=16 \
  -openings file="$BOOK" format="${BOOK##*.}" order=random -srand "$SEED" \
  -games 2 -repeat $LENGTH -concurrency "$CONCURRENCY" \
  -pgnout file="$OUT/games.pgn" \
  -config outname="$OUT/config.json" \
  -recover -ratinginterval 1 \
  2>&1 | tee -i "$OUT/log.txt" || STATUS=$?
trap - INT

[[ -s $OUT/games.pgn ]] || { echo "no games were played" >&2; exit 1; }

# result.txt: fastchess's last results block, and how the games ended
TERMS=$(grep '^\[Termination ' "$OUT/games.pgn" | sort | uniq -c)
{ awk '/^Results of /{on=1; buf=""} on && NF{buf=buf $0 "\n"} END{printf "%s", buf}' "$OUT/log.txt" \
    | grep -Ev '^(Warning|Info|Position|Moves);|^(Started|Finished) game' || true
  printf '\nHow the games ended:\n%s\n' "$TERMS"; } > "$OUT/result.txt"
printf '\nHow the games ended:\n%s\n' "$TERMS"
gzip -f "$OUT/log.txt"   # it runs to tens of MB otherwise

# anything but 'normal' (time forfeit, illegal move, crash, disconnect) means a result
# that wasn't decided on the board
ODD=$(grep -v '"normal"\]$' <<< "$TERMS" || true)
[[ -z $STOPPED ]] || ODD=$(grep -v '"unterminated"\]$' <<< "$ODD" || true)   # the games Ctrl-C cut short

# one row per match in the table of them all, written from result.txt
README=$MATCHES/README.md
[[ -f $README ]] || cat > "$README" <<'EOF'
# Match results

Local records of engine-vs-engine matches (this folder is gitignored). One folder per match, plus the table below.

## Layout
`<date>-<time>-<dev commit>-vs-<base ref>-<mode>/`
| File | What it is |
|---|---|
| `info.txt` | What was compared, and the conditions |
| `result.txt` | The final score, Elo and SPRT verdict, and how the games ended |
| `games.pgn` | Every game, with each move's score, depth and time |
| `config.json` | fastchess's full settings, including the opening seed |
| `log.txt.gz` | fastchess's running output (compressed) |
| `dev`, `base` | The two binaries that played |

Modes: `nonreg` asks "is dev no weaker than base?", `gain` asks "is dev stronger?" (both SPRTs, which stop as soon as the answer is clear), and `elo` plays a fixed number of games to measure the gap. An SPRT started with `CAP=<games>` stops there if the answer still isn't clear, and its verdict reads `capped`.

## Results
Elo is dev minus base. An SPRT's Elo is only a rough guide, since it stops early.

| Date | What | Dev | Base | Mode | Time control | Games | W-L-D | Elo | Verdict | Folder |
|---|---|---|---|---|---|---|---|---|---|---|
EOF
SCORE=$(awk -F'[:,]' '/^Games:/ {printf "%d | %d-%d-%d", $2, $4, $6, $8}' "$OUT/result.txt")
SCORE=${SCORE:-0 | 0-0-0}   # stopped before fastchess had a score to show
ELO=$(awk '/^Elo:/ {printf "%+.1f ± %.1f", $2, $4}' "$OUT/result.txt")
LLR=$(awk '/^LLR:/ {print $2}' "$OUT/result.txt")
case $(grep -o 'H[01] was accepted' "$OUT/result.txt" || true) in
  H1*) VERDICT="passed (LLR $LLR)" ;;
  H0*) VERDICT="failed (LLR $LLR)" ;;
  *)   VERDICT=$([[ $MODE == elo ]] && echo "-" || echo "unfinished (LLR $LLR)") ;;
esac
# ran out of games before the SPRT made up its mind. LLR above 0 leans towards a pass, below 0 towards a fail
# (fewer games than the cap means fastchess gave up for some other reason: that stays 'unfinished')
[[ $MODE == elo || -z $CAP || -n $STOPPED || $VERDICT != unfinished* ]] || (( ${SCORE%% *} < CAP )) || VERDICT="capped at $CAP games (LLR $LLR)"
[[ -z $STOPPED ]] || VERDICT="stopped early${LLR:+ (LLR $LLR)}"
[[ -z $ODD ]] || VERDICT="$VERDICT, but not every game ended normally"
[[ -z $(tail -c 1 "$README") ]] || echo >> "$README"   # the last row might lack its newline
echo "| ${NAME:0:4}-${NAME:4:2}-${NAME:6:2} | ${WHAT:-} | \`$DEV_SHA\` | \`$BASE_SHA\` | $MODE$BOUNDS | $LIMIT_DESC, Hash 16 | $SCORE | $ELO | $VERDICT | \`$NAME\` |" >> "$README"
echo "results are in $OUT"

if [[ -n $ODD ]]; then
    echo "not every game ended normally: look in $OUT/games.pgn" >&2
    exit 1
fi
[[ -z $STOPPED ]] || exit 130
exit $STATUS
