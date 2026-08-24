#include "chess/perft.hpp"

#include "chess/move_generator.hpp"

namespace chess {

std::uint64_t perft(Board& board, int depth) {
    if (depth <= 0) {
        return 1;
    }

    std::vector<Move> moves;
    MoveGenerator::generateLegalMoves(board, moves);
    if (depth == 1) {
        return static_cast<std::uint64_t>(moves.size());
    }

    std::uint64_t nodes = 0;
    for (Move move : moves) {
        UndoState undo;
        if (!board.makeMove(move, undo)) {
            continue;
        }
        nodes += perft(board, depth - 1);
        board.unmakeMove(move, undo);
    }
    return nodes;
}

std::vector<std::pair<Move, std::uint64_t>> perftDivide(Board& board, int depth) {
    std::vector<std::pair<Move, std::uint64_t>> result;
    if (depth <= 0) {
        return result;
    }

    std::vector<Move> moves;
    MoveGenerator::generateLegalMoves(board, moves);
    result.reserve(moves.size());
    for (Move move : moves) {
        UndoState undo;
        if (!board.makeMove(move, undo)) {
            continue;
        }
        result.emplace_back(move, perft(board, depth - 1));
        board.unmakeMove(move, undo);
    }
    return result;
}

}  // namespace chess

