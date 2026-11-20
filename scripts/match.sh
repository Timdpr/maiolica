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
# THREADS=<n> gives dev that many search threads (base keeps one), to test the threads themselves. Fewer games
# are then played at once, so that no more than 6 threads are ever busy: 3 for two threads, 1 for four.
# BASE_THREADS=<n> gives base threads too: THREADS=4 BASE_THREADS=4 tests a change with four threads a side.
# HASH=<MB> is both engines' table size, 16 unless given. A bigger search wants a bigger table (64 for four threads
# a side), & a small one puts the table under pressure on purpose. Leave it alone for a baseline match.
# Ctrl-C pauses a match: the games that have finished are kept, & RESUME=<its folder> scripts/match.sh
# carries on from there, with the same engines, openings & settings (no other arguments needed).
# env overrides: FASTCHESS, BOOK, MATCHES, CONCURRENCY, SEED (same seed, same openings), GAMES (elo only),
#                CAP (gain & nonreg only), NODES (in place of a time control), THREADS (dev's search threads),
#                BASE_THREADS (base's), HASH (MB, both engines)
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
THREADS=${THREADS:-}   # dev's Threads option. Unset, neither engine is given one
[[ -z $THREADS || $THREADS =~ ^[1-9][0-9]*$ ]] || { echo "THREADS must be a number of threads, got '$THREADS'" >&2; exit 1; }
BASE_THREADS=${BASE_THREADS:-}   # base's Threads option. Unset, base isn't given one
[[ -z $BASE_THREADS || $BASE_THREADS =~ ^[1-9][0-9]*$ ]] || { echo "BASE_THREADS must be a number of threads, got '$BASE_THREADS'" >&2; exit 1; }
HASH=${HASH:-16}   # both engines' Hash option, in MB
[[ $HASH =~ ^[1-9][0-9]*$ ]] || { echo "HASH must be a number of megabytes, got '$HASH'" >&2; exit 1; }
# M1 Pro has 8 P-cores; leave a bit of headroom. Only one side thinks at a time, so a game keeps the bigger side's threads busy at most
BUSY=6
MOST=$(( ${THREADS:-1} > ${BASE_THREADS:-1} ? ${THREADS:-1} : ${BASE_THREADS:-1} ))
CONCURRENCY=${CONCURRENCY:-$(( MOST > BUSY ? 1 : BUSY / MOST ))}
SEED=${SEED:-$(( (RANDOM << 15) | RANDOM ))}   # picks the openings; recorded so a match can be repeated
GAMES=${GAMES:-2000}
CAP=${CAP:-}   # gain & nonreg: give up waiting for a verdict after this many games
[[ -z $CAP || $CAP =~ ^[1-9][0-9]*$ ]] || { echo "CAP must be a number of games, got '$CAP'" >&2; exit 1; }
NODES=${NODES:-}   # every move searches this many nodes, & there is no clock
[[ -z $NODES || $NODES =~ ^[1-9][0-9]*$ ]] || { echo "NODES must be a number of nodes a move, got '$NODES'" >&2; exit 1; }
[[ -z $NODES || $# -lt 3 ]] || { echo "NODES takes the place of the time control: give one or the other" >&2; exit 1; }
# with threads the same number of nodes isn't the same search twice, nor shared out the same way
[[ -z $NODES || -z $THREADS$BASE_THREADS ]] || { echo "NODES can't be used with THREADS or BASE_THREADS: threads need the timed test" >&2; exit 1; }
if [[ -n $NODES ]]; then
    LIMIT="nodes=$NODES" LIMIT_NAME=limit LIMIT_DESC="$NODES nodes a move"
else
    LIMIT="tc=$TC" LIMIT_NAME=tc LIMIT_DESC=$TC
fi

# RESUME=<folder>: carry on a paused match. Everything is read back from its folder
RESUME=${RESUME:-}
if [[ -n $RESUME ]]; then
    NAME=$(basename "$RESUME")
    OUT=$MATCHES/$NAME
    INFO=$OUT/info.txt
    [[ -f $OUT/config.json && -f $INFO && -x $OUT/dev && -x $OUT/base ]] || { echo "no match to resume in $OUT" >&2; exit 1; }
    [[ $(tail -n 1 "$INFO") == paused:* ]] || { echo "$NAME isn't paused: it has finished, or it's still running" >&2; exit 1; }
    DEV_SHA=$(sed -n 's/^dev:  *//p' "$INFO")
    BASE_SHA=$(sed -n 's/^base: .*(\(.*\))$/\1/p' "$INFO")
    MODE=$(sed -n 's/^mode: \([a-z]*\) (.*/\1/p' "$INFO")
    LIMIT_DESC=$(sed -n 's/^mode: .*), [a-z]*: \(.*\), book: .*/\1/p' "$INFO")
    CAP=$(sed -n 's/^mode: .*capped at \([0-9]*\) games.*/\1/p' "$INFO")
    GAMES=$(sed -n 's/^mode: elo (\([0-9]*\) games.*/\1/p' "$INFO")
    WHAT=$(sed -n 's/^what: //p' "$INFO")
    THREADS=$(sed -n 's/^threads: dev \([0-9]*\),.*/\1/p' "$INFO")
    BASE_THREADS=$(sed -n 's/^threads: dev [0-9]*, base \([0-9]*\),.*/\1/p' "$INFO")
    HASH=$(sed -n 's/^hash: \([0-9]*\) MB$/\1/p' "$INFO")
    HASH=${HASH:-16}   # only written down when it isn't 16
    NODES=$([[ $LIMIT_DESC != *"nodes a move" ]] || echo "${LIMIT_DESC%% *}")
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
[[ -n $RESUME ]] || git rev-parse --verify --quiet "$BASE_REF^{commit}" >/dev/null || { echo "unknown ref '$BASE_REF'" >&2; exit 1; }

# a local branch only moves when it's pulled, so say so if its remote copy has gone on ahead
NOTES=""
if [[ -z $RESUME ]]; then
    git fetch --quiet 2>/dev/null || true
    UPSTREAM=$(git rev-parse --abbrev-ref "$BASE_REF@{upstream}" 2>/dev/null || true)
    if [[ -n $UPSTREAM ]]; then
        BEHIND=$(git rev-list --count "$BASE_REF..$UPSTREAM")
        ((BEHIND == 0)) || NOTES+="warning: $BASE_REF is $BEHIND commits behind $UPSTREAM. Pull it, or use $UPSTREAM as the base"$'\n'
    fi
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
# macOS: keeps the laptop from sleeping mid-match. -i stops it dozing off when idle; -s holds it awake on mains
# even after it has already dozed once (it then only half wakes, and -i alone no longer holds it)
AWAKE=$(command -v caffeinate || true)

if [[ -z $RESUME ]]; then
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
      [[ -z $THREADS$BASE_THREADS ]] || echo "threads: dev ${THREADS:-1}, base ${BASE_THREADS:-1}, $CONCURRENCY games at once"
      [[ $HASH == 16 ]] || echo "hash: $HASH MB"
      [[ -z ${WHAT:-} ]] || echo "what: $WHAT"
      [[ -z ${CAPNOTE:-} ]] || echo "$CAPNOTE"
      echo "fastchess: $("$FASTCHESS" --version | sed 's/^fastchess //')"
      printf '%s' "$NOTES"; } | tee "$OUT/info.txt"
else
    # the engines, the openings & the score so far are all in the folder already
    [[ ! -f $OUT/log.txt.gz ]] || gunzip -f "$OUT/log.txt.gz"
    { echo "resumed: $(date '+%Y-%m-%d %H:%M')"; printf '%s' "$NOTES"; } | tee -a "$OUT/info.txt"
fi

# book format is just the file extension (pgn or epd)
# no resign or draw adjudication: on 790 of our own games the usual rules got 17-20 results wrong
# Ctrl-C: fastchess winds the match down & saves where it got to in config.json. What was played
# still gets written up, as 'paused' (it's asked for the score after every pair, so there's always a recent one)
if [[ -z $RESUME ]]; then
    ARGS=(-engine cmd="$OUT/dev" name=dev ${THREADS:+option.Threads=$THREADS}
          -engine cmd="$OUT/base" name=base ${BASE_THREADS:+option.Threads=$BASE_THREADS}
          -each "$LIMIT" option.Hash="$HASH"
          -openings file="$BOOK" format="${BOOK##*.}" order=random -srand "$SEED"
          -games 2 -repeat $LENGTH -concurrency "$CONCURRENCY"
          -pgnout file="$OUT/games.pgn"
          -config outname="$OUT/config.json"
          -recover -ratinginterval 1)
else
    ARGS=(-config file="$OUT/config.json")
fi
# On screen: one line every 100 games, with the score so far and a guess at the time left. The rest is in the log
progress() {
    trap '' INT   # a Ctrl-C is for fastchess: we keep reading until it has wound up
    local total=$1 line n first="" elo="" llr="" due=""
    while IFS= read -r line; do
        case $line in
            Elo:*)   elo=${line%%,*} ;;
            LLR:*)   llr=", ${line%% (*}" ;;
            Games:*) n=${line#Games: }; n=${n%%,*}
                     [[ -n $first ]] || { first=$(( n - 2 )); SECONDS=0; }
                     (( n % 100 )) || due=$(sed -E 's/Games: [0-9]+, Wins: ([0-9]+), Losses: ([0-9]+), Draws: ([0-9]+).*/\1-\2-\3/' <<< "$line") ;;
            -----*)  [[ -n $due ]] || continue   # the end of a results block
                     local left=$(( (total - n) * SECONDS / (n - first) / 60 ))
                     echo "$(date +%H:%M)  $n of $total games  $due  $elo$llr  ($([[ $MODE == elo ]] && echo about || echo "at most") ${left} min left)"
                     due="" ;;
            SPRT*|"Finished match"|"Total Time"*|Error*|Fatal*) echo "$line" ;;
        esac
    done
}
trap ':' INT
STATUS=0
$AWAKE ${AWAKE:+-is} "$FASTCHESS" "${ARGS[@]}" 2>&1 | tee -a -i "$OUT/log.txt" \
    | progress "$([[ $MODE == elo ]] && echo "$GAMES" || echo "${CAP:-20000}")" || STATUS=$?
trap - INT

[[ -s $OUT/games.pgn ]] || { echo "no games were played" >&2; exit 1; }

# result.txt: fastchess's last results block, and how the games ended
TERMS=$(grep '^\[Termination ' "$OUT/games.pgn" | sort | uniq -c)
{ awk '/^Results of /{on=1; buf=""} on && NF{buf=buf $0 "\n"} END{printf "%s", buf}' "$OUT/log.txt" \
    | grep -Ev '^(Warning|Info|Position|Moves);|^(Started|Finished) game' || true
  printf '\nHow the games ended:\n%s\n' "$TERMS"; } > "$OUT/result.txt"
cat "$OUT/result.txt"
gzip -f "$OUT/log.txt"   # it runs to tens of MB otherwise

# anything but 'normal' (time forfeit, illegal move, crash, disconnect) means a result
# that wasn't decided on the board
ODD=$(grep -v '"normal"\]$' <<< "$TERMS" || true)
ODD=$(grep -v '"unterminated"\]$' <<< "$ODD" || true)   # the games a pause cut short: they get played again

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
# no verdict, & games still to play: it was paused (or fastchess fell over), & RESUME picks it up
PLAYED=${SCORE%% *}
PAUSED=""
[[ $VERDICT != unfinished* && $MODE != elo ]] || (( PLAYED >= $([[ $MODE == elo ]] && echo "$GAMES" || echo "${CAP:-20000}") )) || PAUSED=1
if [[ -n $PAUSED ]]; then
    VERDICT="paused at $PLAYED games${LLR:+ (LLR $LLR)}"
    echo "paused: $(date '+%Y-%m-%d %H:%M'), $PLAYED games played" >> "$OUT/info.txt"
elif [[ $VERDICT == unfinished* ]]; then
    # ran out of games before the SPRT made up its mind. LLR above 0 leans towards a pass, below 0 towards a fail
    VERDICT="capped at ${CAP:-20000} games (LLR $LLR)"
fi
[[ -z $ODD ]] || VERDICT="$VERDICT, but not every game ended normally"
# the threads, as the row says them: nothing when neither was set, "dev 4 threads" against one, "4 threads a side" when level
SIDES=${THREADS:+, dev $THREADS threads}
if [[ -n $BASE_THREADS && ${THREADS:-1} == "$BASE_THREADS" ]]; then
    SIDES=", $BASE_THREADS threads a side"
elif [[ -n $BASE_THREADS ]]; then
    SIDES=", dev ${THREADS:-1} threads, base $BASE_THREADS"
fi
[[ -z $(tail -c 1 "$README") ]] || echo >> "$README"   # the last row might lack its newline
grep -vF "\`$NAME\`" "$README" > "$README.tmp" && mv "$README.tmp" "$README"   # a resumed match keeps its one row
echo "| ${NAME:0:4}-${NAME:4:2}-${NAME:6:2} | ${WHAT:-} | \`$DEV_SHA\` | \`$BASE_SHA\` | $MODE$BOUNDS | $LIMIT_DESC, Hash $HASH$SIDES | $SCORE | $ELO | $VERDICT | \`$NAME\` |" >> "$README"
echo "results are in $OUT"

if [[ -n $ODD ]]; then
    echo "not every game ended normally: look in $OUT/games.pgn" >&2
    exit 1
fi
if [[ -n $PAUSED ]]; then
    echo "paused. To carry on: RESUME=$NAME scripts/match.sh"
    exit 130
fi
exit $STATUS
