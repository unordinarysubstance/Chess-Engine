#pragma once

#include "chess/board.hpp"

namespace chess {

class Evaluator {
public:
    [[nodiscard]] int evaluate(const Board& board) const;
    [[nodiscard]] static int pieceValue(PieceType type);

private:
    [[nodiscard]] static int evaluateWhitePerspective(const Board& board);
};

}  // namespace chess

