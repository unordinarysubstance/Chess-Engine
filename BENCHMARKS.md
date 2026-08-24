# Benchmark Report

## Method

Every position is searched from a fresh board and a cleared 64 MiB table. Each
row is the median of three single-threaded runs. All modes use the same legal
move generator, evaluator, terminal rules, and base move ordering. The harness
exits with an error if the modes disagree on the minimax score.

Recorded environment:

- Linux 6.18, x86-64, AMD EPYC 9V74 host.
- GCC 13.3, C++17, `-O3 -DNDEBUG -march=native`.
- One search thread; 64 MiB transposition table.
- Node count includes root, internal, and leaf negamax calls.

Wall-clock times depend on the machine. Node counts are deterministic for this
version and are the primary algorithmic comparison.

## Minimax versus alpha-beta versus alpha-beta plus TT

### Starting position, depth 4

| Mode | Nodes | Median time | Reduction vs minimax | TT hits | Best move |
| --- | ---: | ---: | ---: | ---: | --- |
| Minimax | 206,604 | 252.04 ms | 1.00× | 0 | `b1c3` |
| Alpha-beta | 4,532 | 5.75 ms | 45.59× | 0 | `b1c3` |
| Alpha-beta + TT | 4,483 | 6.28 ms | 46.09× | 50 | `b1c3` |

### Kiwipete, depth 3

| Mode | Nodes | Median time | Reduction vs minimax | TT hits | Best move |
| --- | ---: | ---: | ---: | ---: | --- |
| Minimax | 99,950 | 197.02 ms | 1.00× | 0 | `e2a6` |
| Alpha-beta | 2,565 | 5.59 ms | 38.97× | 0 | `e2a6` |
| Alpha-beta + TT | 2,565 | 5.81 ms | 38.97× | 0 | `e2a6` |

### Open middlegame, depth 4

| Mode | Nodes | Median time | Reduction vs minimax | TT hits | Best move |
| --- | ---: | ---: | ---: | ---: | --- |
| Minimax | 1,917,377 | 3,360.49 ms | 1.00× | 0 | `c1g5` |
| Alpha-beta | 15,623 | 29.24 ms | 122.73× | 0 | `c1g5` |
| Alpha-beta + TT | 14,983 | 28.80 ms | 127.97× | 579 | `c1g5` |

Across these searches, alpha-beta used 98.98% fewer nodes than plain minimax.
The TT has little opportunity to help at depth 3, and its lookup overhead can
slightly outweigh its savings in very shallow searches.

## Isolated Zobrist/TT effect at depth 6

| Position | Alpha-beta nodes | With TT nodes | Node reduction | AB time | TT time | Time speedup | TT hits |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Starting position | 132,249 | 104,779 | 20.8% | 207.64 ms | 172.47 ms | 1.20× | 3,549 |
| Kiwipete | 207,285 | 139,959 | 32.5% | 522.93 ms | 341.48 ms | 1.53× | 13,161 |
| Open middlegame | 651,434 | 492,804 | 24.4% | 1,274.56 ms | 981.07 ms | 1.30× | 34,132 |
| **Combined** | **990,968** | **737,542** | **25.6%** | **2,005.13 ms** | **1,495.02 ms** | **1.34×** | **50,842** |

The deeper comparison is the useful hashing result: the Zobrist-keyed TT removes
about one quarter of node visits overall, with the largest measured reduction
on Kiwipete. The gain comes from exact-score reuse, bound cutoffs, and cached
best moves improving ordering.

## Reproduce

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/chess_bench --repeat 3
./build/chess_bench --tt-only-depth 6 --repeat 3
```

Options:

- `--repeat N` changes the number of timed repetitions.
- `--depth N` forces the same depth for the full three-mode suite.
- `--tt-only-depth N` compares only alpha-beta without and with the TT.
