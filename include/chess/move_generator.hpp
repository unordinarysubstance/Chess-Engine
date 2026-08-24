#pragma once

#include "chess/board.hpp"

#include <array>
#include <vector>

namespace chess {

class MoveGenerator {
public:
    static void initialize();

    static void generateLegalMoves(Board& board, std::vector<Move>& moves,
                                   bool capturesOnly = false);
    static std::vector<Move> generateLegalMoves(Board& board,
                                                bool capturesOnly = false);

    [[nodiscard]] static bool isSquareAttacked(const Board& board, Square square,
                                               Color byColor);
    [[nodiscard]] static bool isInCheck(const Board& board, Color color);

    [[nodiscard]] static Bitboard knightAttacks(Square square);
    [[nodiscard]] static Bitboard kingAttacks(Square square);
    [[nodiscard]] static Bitboard pawnAttacks(Color color, Square square);
    [[nodiscard]] static Bitboard bishopAttacks(Square square, Bitboard occupancy);
    [[nodiscard]] static Bitboard rookAttacks(Square square, Bitboard occupancy);
    [[nodiscard]] static Bitboard queenAttacks(Square square, Bitboard occupancy);

private:
    static bool initialized_;
    static std::array<Bitboard, 64> knightAttackTable_;
    static std::array<Bitboard, 64> kingAttackTable_;
    static std::array<std::array<Bitboard, 64>, 2> pawnAttackTable_;

    static void generatePseudoLegalMoves(const Board& board, std::vector<Move>& moves,
                                         bool capturesOnly);
    static void generatePawnMoves(const Board& board, std::vector<Move>& moves,
                                  bool capturesOnly);
    static void generatePieceMoves(const Board& board, std::vector<Move>& moves,
                                   PieceType type, bool capturesOnly);
    static void generateKingMoves(const Board& board, std::vector<Move>& moves,
                                  bool capturesOnly);
};

}  // namespace chess

