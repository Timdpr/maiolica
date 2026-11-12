#!/usr/bin/env bash
# UCI conformance: feeds the engine command scripts & checks what comes back.
#   tests/uci_test.sh path/to/maiolica
# A '~N' line in a script sleeps N seconds, so commands can arrive mid search.
set -uo pipefail
ENGINE=${1:?usage: tests/uci_test.sh path/to/maiolica}
failures=0

# run the engine on a script, printing its output (and failing on a crash/hang)
run() {
    local input=$1 out status
    out=$( { while IFS= read -r line; do
                 if [[ $line == ~* ]]; then sleep "${line#\~}"; else printf '%s\n' "$line"; fi
             done <<< "$input"; } | perl -e 'alarm 20; exec @ARGV' "$ENGINE" 2>&1 )
    status=$?
    ((status == 0)) || out+=$'\n'"EXIT STATUS $status"
    printf '%s\n' "$out"
}

# check <name> <script> <grep -E pattern that must match> [pattern that mustn't]
check() {
    local out
    out=$(run "$2")
    if ! grep -Eq -- "$3" <<< "$out" || { [[ -n ${4:-} ]] && grep -Eq -- "$4" <<< "$out"; } \
        || grep -q "EXIT STATUS" <<< "$out"; then
        echo "FAIL $1"; sed 's/^/    /' <<< "$out" | tail -15
        ((failures++))
    else
        echo "ok   $1"
    fi
}

# count <name> <script> <pattern> <expected number of matching lines>
count() {
    local n
    n=$(run "$2" | grep -Ec -- "$3")
    if [[ $n == "$4" ]]; then echo "ok   $1"; else echo "FAIL $1: $n lines match '$3', expected $4"; ((failures++)); fi
}

# order <name> <script> <pattern> <pattern that must come later>
order() {
    local out a b
    out=$(run "$2")
    a=$(grep -En -- "$3" <<< "$out" | head -1 | cut -d: -f1)
    b=$(grep -En -- "$4" <<< "$out" | head -1 | cut -d: -f1)
    if [[ -n $a && -n $b ]] && ((a < b)); then echo "ok   $1"; else
        echo "FAIL $1: wanted '$3' before '$4'"; sed 's/^/    /' <<< "$out" | tail -15; ((failures++)); fi
}

check "uci handshake" $'uci\nisready\nquit' 'option name Hash type spin'
order "uciok before readyok" $'uci\nisready\nquit' '^uciok$' '^readyok$'
count "only uci lines (and the banner)" $'uci\nisready\nposition startpos moves e2e4\ngo depth 4\n~0.5\nquit' \
    '^(id|option|uciok|readyok|info|bestmove)( |$)|by Tim Russell$' 15
check "no stray output" $'uci\nucinewgame\nposition startpos moves e2e4\ngo depth 3\n~0.5\nquit' 'bestmove' \
    '^(Board|side|time:|HashTable|Set Hash)'
count "two gos without stop (normal game flow)" $'position startpos\ngo depth 4\n~0.5\nposition startpos moves e2e4 e7e5\ngo depth 4\n~0.5\nquit' \
    '^bestmove [a-h][1-8][a-h][1-8]' 2
check "idle stop is harmless" $'stop\nstop\nisready\nquit' 'readyok'
count "go infinite then instant stop" $'position startpos\ngo infinite\nstop\nisready\nquit' '^bestmove [a-h][1-8]' 1
order "infinite waits for stop" $'position fen 8/8/4k3/8/8/4K3/8/8 w - - 0 1\ngo infinite\n~1\nisready\n~0.2\nstop\nquit' \
    '^readyok' '^bestmove'
count "infinite sends nothing early" $'position fen 8/8/4k3/8/8/4K3/8/8 w - - 0 1\ngo infinite\n~1\nquit' '^bestmove' 1
order "ponder waits for ponderhit" $'position startpos moves e2e4\ngo ponder wtime 2000 btime 2000\n~0.5\nisready\nponderhit\n~1\nquit' \
    '^readyok' '^bestmove'
check "mate scores" $'position fen 6k1/5ppp/8/8/8/8/8/R5K1 w - - 0 1\ngo depth 4\n~0.5\nquit' 'score mate 1 .*pv a1a8'
check "checkmated: bestmove 0000" $'position fen 6k1/5ppp/8/8/8/8/5PPP/r5K1 w - - 0 1\ngo depth 3\n~0.3\nquit' '^bestmove 0000'
check "stalemate: bestmove 0000" $'position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1\ngo depth 3\n~0.3\nquit' '^bestmove 0000'
check "root repetition still plays a move" $'position startpos moves g1f3 g8f6 f3g1 f6g8 g1f3 g8f6 f3g1 f6g8\ngo depth 4\n~0.5\nquit' \
    '^bestmove [a-h][1-8][a-h][1-8]'
check "tiny clock still plays a legal move" $'position startpos\ngo wtime 30 btime 30\n~0.5\nquit' '^bestmove [a-h][1-8][a-h][1-8]'
check "big increment is capped" $'debug on\nposition startpos\ngo wtime 500 btime 500 winc 5000 binc 5000\n~1\nquit' \
    'time budget 337 ms'
check "negative clock still moves at once" $'debug on\nposition startpos\ngo wtime -30 btime 5000 winc 100 binc 100\n~0.5\nquit' \
    'time budget 1 ms'
check "go nodes" $'position startpos\ngo nodes 3000\n~0.5\nquit' '^bestmove'
check "messy whitespace" $'position   fen  rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1   moves\te7e5  g1f3\nd\nquit' \
    'side character: b'
check "invalid fen reported" $'position fen xyz w - -\nisready\nquit' 'info string invalid fen'
check "illegal move reported" $'position startpos moves e2e4 e2e4\nquit' 'info string illegal move: e2e4'
check "option names ignore case" $'setoption name hash value 8\nsetoption name CLEAR HASH\nisready\nquit' 'readyok'
check "debug on/off keeps the loop going" $'debug on\ndebug off\nisready\nquit' 'readyok'
check "eof without quit exits" $'uci' 'uciok'
check "go perft" $'go perft 3\nquit' 'Nodes searched: 8902'
check "bench" $'bench\nquit' 'nodes [0-9]+ nps'
epd=$(mktemp) && echo "8/8/4k3/8/8/4K3/8/8 w - - 0 1" > "$epd"
check "epd doesn't inherit go infinite" $'go infinite\nstop\nepd '"$epd"$'\nisready\nquit' 'readyok'
rm -f "$epd"

echo "$failures failure(s)"
exit $((failures > 0))
