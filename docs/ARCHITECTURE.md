# Architecture and Invariants

## Coordinates and representation

Squares use `a1 = 0` through `h8 = 63`. A bitboard is a `uint64_t`; bit `s`
represents square `s`. `Board` maintains three synchronized views:

1. Twelve piece bitboards (`white pawn` through `black king`).
2. White, black, and combined occupancy bitboards.
3. A 64-entry mailbox for constant-time `pieceAt(square)` queries.

`Board::isValid` reconstructs the bitboards from the mailbox and verifies the
invariants, king counts, occupancy disjointness, and pawn ranks.

## Move encoding

`Move` is a compact 32-bit value containing source, destination, promotion type,
and flags. Flags distinguish capture, double pawn push, en passant, castling,
and promotion. The default all-zero encoding is the null move; `a1a1` is never
a legal chess move, so the representation is unambiguous.

## Make and unmake

Before a move, `UndoState` records the captured piece, castling rights,
en-passant square, clocks, and full Zobrist key. Piece and occupancy bitboards are
then changed incrementally. Unmake performs the inverse piece operation and
restores all irreversible metadata from `UndoState`.

The hash update xors out old castling/en-passant state, xors every moved or
captured piece, toggles the side key, then xors in new castling/en-passant state.
Unmake restores the saved key exactly. Tests assert FEN and hash identity after
every legal start-position move and after each special move type.

## Legal move generation

Knight, king, and pawn attacks are precomputed for all 64 squares. Bishop, rook,
and queen attacks follow occupancy-blocked rays. Generation has two stages:

1. Generate pseudo-legal pawn, piece, king, promotion, en-passant, and castle moves.
2. Make each candidate and reject it if the moving side's king is attacked.

This second stage naturally handles pins, discovered checks, and the difficult
en-passant discovered-check case. Castling additionally checks the start,
transit, and destination squares before emitting a candidate.

## Search

The engine uses negamax, so every score is from the current side's perspective.
The three benchmark modes share evaluation and ordering:

- `MINIMAX`: explores every child.
- `ALPHA_BETA`: maintains a negated alpha/beta window and stops on beta cutoffs.
- `ALPHA_BETA_TT`: adds TT probes, bound tightening, exact returns, and TT move ordering.

Move-order priority is TT move, captures by MVV-LVA, promotions, killer moves,
history score, then castling. Principal variation arrays avoid allocation during
PV propagation. Mate scores include ply distance so the engine prefers faster
mates and postpones unavoidable losses.

Iterative deepening retains TT entries between completed depths. A deadline or
UCI `stop` sets an atomic flag; an interrupted iteration is discarded and the
last fully completed result is returned.

## Transposition table

The TT is a power-of-two vector indexed by `zobristKey & mask`. Each entry stores:

- Full 64-bit key for collision rejection.
- Search depth and score.
- Exact, lower, or upper bound type.
- Best move and search generation.

Replacement favors an identical key, an old generation, or at least as much
search depth. Mate scores are normalized on store/probe so the same position
keeps the correct mate distance when reached at a different ply.

## Evaluation

The static evaluator combines material with inexpensive classical features:

- Procedural piece-square bonuses.
- Piece mobility from attack bitboards.
- Doubled, isolated, and passed pawn terms.
- Bishop pair bonus.
- Open and semi-open rook file bonuses.
- A small side-to-move tempo bonus.

The returned value is always from the side-to-move perspective, which makes the
negamax sign inversion valid.

## UCI concurrency

`go` copies the current board and searches it on a worker thread. The input loop
remains responsive to `stop`, `isready`, and `quit`. Commands that mutate the
position or hash table first stop and join the worker. Output is protected by a
mutex so search information and command replies do not interleave mid-line.

