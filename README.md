# High-Performance Chess Engine

A dependency-free C++17 chess engine built around 64-bit bitboards, incremental
make/unmake, alpha-beta pruning, deterministic Zobrist hashing, and a fixed-size
transposition table. It includes a UCI interface, correctness tests, perft tools,
and a benchmark that compares identical searches in three modes.

## Implemented features

- Modular `Board`, `MoveGenerator`, `Evaluator`, `Engine`, and `UciInterface` classes.
- Twelve piece bitboards, two color occupancies, and a 64-entry mailbox for fast lookup.
- Complete legal move generation: checks, pins, castling, en passant, and four promotions.
- Reversible incremental state updates with castling, en-passant, clock, and hash restoration.
- Material, procedural piece-square, mobility, pawn-structure, bishop-pair, and rook-file terms.
- Plain negamax minimax, alpha-beta, and alpha-beta with a transposition table.
- MVV-LVA capture ordering, promotion ordering, killer moves, and a history heuristic.
- Iterative deepening, mate-distance scores, fifty-move and threefold-repetition detection.
- Exact/lower/upper TT bounds, depth-aware replacement, and mate-score normalization.
- Asynchronous UCI search with `stop`, depth, move-time, clock, increment, and hash options.
- Deterministic benchmark executable and a self-contained test executable.

## Project layout

| Path | Purpose |
| --- | --- |
| `include/chess/` | Public interfaces and core types |
| `src/board.cpp` | FEN, bitboards, incremental make/unmake, repetition state |
| `src/move_generator.cpp` | Attack tables, pseudo-legal generation, legality filtering |
| `src/evaluator.cpp` | Static evaluation from the side-to-move perspective |
| `src/engine.cpp` | All three searches, ordering, iterative deepening, PV tracking |
| `src/transposition_table.cpp` | Power-of-two fixed-size Zobrist table |
| `src/uci.cpp` | UCI command parser and asynchronous search worker |
| `tests/tests.cpp` | Perft, state restoration, special rules, hashing, and search tests |
| `tools/benchmark.cpp` | Reproducible pruning and TT comparisons |
| `docs/ARCHITECTURE.md` | Design and invariants |
| `BENCHMARKS.md` | Measured results and interpretation |

## Build

Requirements: a C++17 compiler, CMake 3.16 or newer, and POSIX threads. There are
no third-party libraries.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

This creates:

- `build/chess_engine` — UCI engine and perft CLI.
- `build/chess_tests` — correctness suite.
- `build/chess_bench` — benchmark harness.

For CLion, open this directory as the project, allow CMake to load, select the
`chess_engine`, `chess_tests`, or `chess_bench` target, and press Run.

## Verify correctness

```bash
ctest --test-dir build --output-on-failure
./build/chess_engine --perft 5
```

The test suite checks:

- Start position through perft depth 5: `4,865,609` leaf nodes.
- Kiwipete through perft depth 4: `4,085,603` leaf nodes.
- The standard en-passant stress position through depth 4: `43,238` leaf nodes.
- FEN validation, make/unmake identity, and exact Zobrist restoration.
- Castling, en passant, promotion, pins, mate, and threefold repetition.
- Equal scores across minimax, alpha-beta, and alpha-beta plus TT.

Sanitizer build:

```bash
cmake -S . -B build-sanitize -DCHESS_ENABLE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build-sanitize --parallel
ctest --test-dir build-sanitize --output-on-failure
```

## Use the engine

The default executable starts a UCI loop:

```text
uci
isready
position startpos moves e2e4 e7e5 g1f3
go depth 6
stop
quit
```

Useful nonstandard debugging commands:

```text
d
perft 4
go depth 4 minimax
go depth 6 alphabeta
go depth 7 tt
```

The default `go` mode is alpha-beta plus TT. The supported UCI options are
`Hash` (1–2048 MiB) and `Clear Hash`.

## Run the benchmarks

```bash
./build/chess_bench --repeat 3
./build/chess_bench --tt-only-depth 6 --repeat 3
```

The first command compares all three modes. The second isolates hashing at a
deeper depth, where repeated positions are common enough to measure clearly.
See [BENCHMARKS.md](BENCHMARKS.md) for the recorded results.

On the recorded run, alpha-beta reduced the three shallow benchmark searches
from a combined `2,223,931` nodes to `22,720` nodes. At depth 6, adding the TT
reduced combined alpha-beta nodes from `990,968` to `737,542` (25.6% fewer).

## Scope

This is a complete classical project engine and UCI program, not a Stockfish
replacement. It deliberately uses readable ray-generated sliding attacks and a
handcrafted evaluator. Natural next upgrades are magic bitboards, quiescence,
null-move pruning, late-move reductions, aspiration windows, and NNUE.

## License

MIT — see [LICENSE](LICENSE).

