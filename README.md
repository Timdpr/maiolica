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
See [docs/architecture.md](docs/architecture.md) for how it works and [docs/testing.md](docs/testing.md) for testing & measuring.
