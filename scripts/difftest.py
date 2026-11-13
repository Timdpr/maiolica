#!/usr/bin/env python3
# Differential test: the same searches through two builds over UCI, comparing every 'info'
# and 'bestmove' line (minus time & nps). Identical search => identical output.
#   scripts/difftest.py A B [games=60]
# A and B are git refs, or '.' for the working tree. e.g. scripts/difftest.py master .
# The bench signature is 9 positions on a fresh table; this is thousands of them, most with
# a table carried through a whole game. 60 games is ~8,500 searches and takes ~2.5 minutes.
# Exits non-zero on any mismatch.
import os, random, re, shutil, subprocess, sys, threading, time

USAGE = "usage: scripts/difftest.py A B [games]"
SEED = 20261113  # fixed, so every run plays the same games
TIMEOUT = 120    # seconds an engine gets to answer before it's taken to have hung

# name, Hash in MB, go command, fresh table for every search?, every Nth position, first N games only
MODES = [
    ("fresh table, depth 6",                 16, "go depth 6",     True,  3, None),
    ("1MB table kept all game, 30000 nodes",  1, "go nodes 30000", False, 1, None),  # a full table: lots of replacement
    ("64MB table kept all game, depth 7",    64, "go depth 7",     False, 2, 20),    # the default table, a bit deeper
]

# quiet endgames, started with the fifty-move clock nearly run out: a game from the start
# position never gets near it
ENDGAMES = [
    "2r5/5pk1/6p1/7p/7P/6P1/5PK1/3R4 w - -",                  # rooks
    "8/4kp2/4p1p1/3bP2p/5P1P/4B1P1/5K2/8 w - -",              # opposite bishops, all blocked up
    "8/pp3k2/2p2np1/8/3P4/3N1P2/PP4K1/8 b - -",               # knights
    "8/5pk1/4q1p1/7p/7P/3Q2P1/5PK1/8 b - -",                  # queens
    "2r3k1/pp3pp1/2n1p2p/3pP3/3P4/2P2N1P/P4PP1/2R3K1 w - -",  # a rook & a knight each
    "r7/8/4k3/8/8/2B1K3/8/7R b - -",                          # rook & bishop v rook, no pawns
]


def sh(*cmd):
    subprocess.run(cmd, check=True, stdout=subprocess.DEVNULL)


# build a ref (or the working tree) and return the path of its engine, the way ab.sh does
def build(ref):
    if ref == ".":
        sh("cmake", "--preset", "release")
        sh("cmake", "--build", "--preset", "release")
        return "cmake-build-release/maiolica"
    sha = subprocess.run(["git", "rev-parse", "--short", ref + "^{commit}"],
                         check=True, capture_output=True, text=True).stdout.strip()
    out = "cmake-build-ab/" + sha
    if not os.access(out + "/maiolica", os.X_OK):  # cached, since a ref never changes
        shutil.rmtree(out, ignore_errors=True)
        os.makedirs(out + "/src")
        subprocess.run(f"git archive {sha} | tar -x -C {out}/src", shell=True, check=True)
        sh("cmake", "-S", out + "/src", "-B", out, "-DCMAKE_BUILD_TYPE=Release")
        sh("cmake", "--build", out, "-j")
    return out + "/maiolica"


class Engine:
    def __init__(self, path):
        self.path = path
        self.p = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.cmd("uci")
        self.until("uciok")

    def cmd(self, s):
        try:
            self.p.stdin.write(s + "\n")
            self.p.stdin.flush()
        except BrokenPipeError:
            self.fail(f"before '{s}'")

    def fail(self, why):
        sys.exit(f"FAILED: {self.path} crashed or hung ({why})")

    # read lines up to & including the one starting with token. A hung engine is killed
    # by the timer, which then reads the same as one that crashed: no more output
    def until(self, token):
        lines = []
        timer = threading.Timer(TIMEOUT, self.p.kill)
        timer.start()
        try:
            while True:
                line = self.p.stdout.readline()
                if not line:
                    self.fail(f"waiting for '{token}'")
                lines.append(line.strip())
                if lines[-1].startswith(token):
                    return lines
        finally:
            timer.cancel()

    # the legal moves, from 'perft 1'
    def legal(self, pos):
        self.cmd(pos)
        self.cmd("perft 1")
        return [l.split(":")[0] for l in self.until("Nodes searched") if re.match(r"^[a-h][1-8][a-h][1-8][qrbn]?: ", l)]

    # start a search...
    def go(self, pos, go, fresh):
        if fresh:
            self.cmd("ucinewgame")
        self.cmd(pos)
        self.cmd(go)

    # ...and wait for its info & bestmove lines, without the parts that depend on the clock
    def result(self):
        return [re.sub(r" (time|nps) \d+", "", l) for l in self.until("bestmove") if l.startswith(("info depth", "bestmove"))]

    def search(self, pos, go, fresh):
        self.go(pos, go, fresh)
        return self.result()

    def quit(self):
        self.cmd("quit")
        self.p.wait()


def position(start, moves):
    return f"position {start} moves " + " ".join(moves)


# n games, each a start & a move list: a random opening, then the engine plays both sides at
# depth 4. Every 5th game is an endgame instead, 1 to 10 plies short of a fifty-move draw
def makeGames(engine, n, rng):
    games = []
    for g in range(n):
        start, moves = "startpos", []
        if g % 5 == 4:
            start = f"fen {rng.choice(ENDGAMES)} {rng.randrange(90, 100)} 1"
        for _ in range(0 if start != "startpos" else rng.choice([4, 6, 8, 10])):
            legal = engine.legal(position(start, moves))
            if not legal:
                break
            moves.append(rng.choice(legal))
        for ply in range(110):
            if ply % 9 == 8:  # the odd random move, to get off the beaten track
                legal = engine.legal(position(start, moves))
                if not legal:
                    break
                moves.append(rng.choice(legal))
                continue
            best = engine.search(position(start, moves), "go depth 4", True)[-1].split()[1]
            if best == "0000":  # mate or stalemate
                break
            moves.append(best)
        games.append((start, moves))
    return games


def main():
    if len(sys.argv) < 3:
        sys.exit(USAGE)
    os.chdir(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
    n = int(sys.argv[3]) if len(sys.argv) > 3 else 60

    print(f"building {sys.argv[1]} and {sys.argv[2]}...", flush=True)
    a, b = Engine(build(sys.argv[1])), Engine(build(sys.argv[2]))

    t = time.time()
    games = makeGames(a, n, random.Random(SEED))
    print(f"{n} games, {sum(len(g) for _, g in games)} plies, made in {time.time() - t:.0f}s", flush=True)

    failed = False
    for name, mb, go, fresh, step, limit in MODES:
        searches = lines = bad = 0
        t = time.time()
        for e in (a, b):
            e.cmd(f"setoption name Hash value {mb}")
        for start, g in games[:limit]:
            for e in (a, b):
                e.cmd("ucinewgame")
            for i in range(0, len(g) + 1, step):
                pos = position(start, g[:i])
                for e in (a, b):  # both search at once
                    e.go(pos, go, fresh)
                outA, outB = a.result(), b.result()
                searches += 1
                lines += len(outA)
                if outA != outB:
                    bad += 1
                    if bad <= 3:  # the first few are enough to go on
                        print(f"MISMATCH [{name}] {pos}\n  {go}\n  A: {outA[-2:]}\n  B: {outB[-2:]}", flush=True)
        print(f"{name}: {searches} searches, {lines} lines compared, {bad} mismatches ({time.time() - t:.0f}s)", flush=True)
        failed |= bad > 0

    a.quit()
    b.quit()
    print("DIFFERENT - search changed!" if failed else "identical")
    sys.exit(1 if failed else 0)


main()
