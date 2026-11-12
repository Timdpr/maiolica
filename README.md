# Maiolica   
A WIP chess engine  
&nbsp;
> Kublai was a keen chess player; following Marco's movements, he observed that certain pieces implied or excluded the vicinity of other pieces and were shifted along certain lines. Ignoring the objects' variety of form, he could grasp the system of arranging one with respect to the others on the maiolica floor. He thought: "If each city is like a game of chess, the day when I have learned the rules, I shall finally possess my empire, even if I shall never succeed in knowing all the cities it contains."

&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;_— Italo Calvino, Invisible Cities_

## Building
Needs CMake 3.25+ and a C++20 compiler.
```sh
cmake --preset release && cmake --build --preset release
./cmake-build-release/maiolica          # speaks UCI: load it in any UCI GUI
./cmake-build-release/maiolica bench    # quick benchmark
ctest --preset release                  # perft & bench tests
```
## Playing in the terminal
```sh
./cmake-build-release/maiolica console              # play it: moves like e4, Nf3 or e2e4, 'help' for commands
./cmake-build-release/maiolica replay game.pgn 500  # watch the games in a pgn, 500ms a move
scripts/demo.sh                                     # a quick self play game (via fastchess), then replayed
```
Colours & unicode pieces when it's a terminal; plain text otherwise (or with `NO_COLOR` set).

See [docs/uci.md](docs/uci.md) for the protocol & extra commands, [docs/architecture.md](docs/architecture.md) for how it works and [docs/testing.md](docs/testing.md) for testing & measuring.
