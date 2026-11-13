#!/usr/bin/env python3
# Differential test: the same searches through two builds over UCI, comparing every 'info'
# and 'bestmove' line (minus time & nps). Identical search => identical output.
#   scripts/difftest.py A B [games=60]
# A and B are git refs, or '.' for the working tree. e.g. scripts/difftest.py master .
# The bench signature is 9 positions on a fresh table; this is thousands of them, most with
# a table carried through a whole game. 60 games is ~8,700 searches and takes ~6 minutes.
# Exits non-zero on any mismatch.
import os, random, re, shutil, subprocess, sys, time

USAGE = "usage: scripts/difftest.py A B [games]"
SEED = 20261113  # fixed, so every run plays the same games

# name, Hash in MB, go command, fresh table for every search?, every Nth position, first N games only
MODES = [
    ("fresh table, depth 6",                 16, "go depth 6",     True,  3, None),
    ("1MB table kept all game, 30000 nodes",  1, "go nodes 30000", False, 1, None),  # a full table: lots of replacement
    ("64MB table kept all game, depth 7",    64, "go depth 7",     False, 2, 20),    # the default table, a bit deeper
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
        self.p = subprocess.Popen([path], stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True, bufsize=1)
        self.cmd("uci")
        self.until("uciok")

    def cmd(self, s):
        self.p.stdin.write(s + "\n")
        self.p.stdin.flush()

    # read lines up to & including the one starting with token
    def until(self, token):
        lines = []
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise RuntimeError("engine died")
            lines.append(line.strip())
            if lines[-1].startswith(token):
                return lines

    # the legal moves, from 'perft 1'
    def legal(self, pos):
        self.cmd(pos)
        self.cmd("perft 1")
        return [l.split(":")[0] for l in self.until("Nodes searched") if re.match(r"^[a-h][1-8][a-h][1-8][qrbn]?: ", l)]

    # one search: its info & bestmove lines, without the parts that depend on the clock
    def search(self, pos, go, fresh):
        if fresh:
            self.cmd("ucinewgame")
        self.cmd(pos)
        self.cmd(go)
        return [re.sub(r" (time|nps) \d+", "", l) for l in self.until("bestmove") if l.startswith(("info depth", "bestmove"))]

    def quit(self):
        self.cmd("quit")
        self.p.wait()


# n games as move lists: a random opening, then the engine plays both sides at depth 4
def makeGames(engine, n, rng):
    games = []
    for _ in range(n):
        moves = []
        for _ in range(rng.choice([4, 6, 8, 10])):
            legal = engine.legal("position startpos moves " + " ".join(moves))
            if not legal:
                break
            moves.append(rng.choice(legal))
        for ply in range(110):
            if ply % 9 == 8:  # the odd random move, to get off the beaten track
                legal = engine.legal("position startpos moves " + " ".join(moves))
                if not legal:
                    break
                moves.append(rng.choice(legal))
                continue
            best = engine.search("position startpos moves " + " ".join(moves), "go depth 4", True)[-1].split()[1]
            if best == "0000":  # mate or stalemate
                break
            moves.append(best)
        games.append(moves)
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
    print(f"{n} games, {sum(map(len, games))} plies, made in {time.time() - t:.0f}s", flush=True)

    failed = False
    for name, mb, go, fresh, step, limit in MODES:
        searches = lines = bad = 0
        t = time.time()
        for e in (a, b):
            e.cmd(f"setoption name Hash value {mb}")
        for g in games[:limit]:
            for e in (a, b):
                e.cmd("ucinewgame")
            for i in range(0, len(g) + 1, step):
                pos = "position startpos moves " + " ".join(g[:i])
                outA, outB = a.search(pos, go, fresh), b.search(pos, go, fresh)
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
