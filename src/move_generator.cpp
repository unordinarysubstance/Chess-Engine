#include "chess/move_generator.hpp"

#include <array>
#include <cstddef>

namespace chess {

bool MoveGenerator::initialized_ = false;
std::array<Bitboard, 64> MoveGenerator::knightAttackTable_{};
std::array<Bitboard, 64> MoveGenerator::kingAttackTable_{};
std::array<std::array<Bitboard, 64>, 2> MoveGenerator::pawnAttackTable_{};

namespace {

constexpr std::array<PieceType, 4> PROMOTION_PIECES = {
    QUEEN, ROOK, BISHOP, KNIGHT
};

void addPromotions(std::vector<Move>& moves, Square from, Square to,
                   std::uint8_t baseFlags) {
    for (PieceType promotion : PROMOTION_PIECES) {
        moves.emplace_back(from, to,
                           static_cast<std::uint8_t>(baseFlags | PROMOTION),
                           promotion);
    }
}

Bitboard slidingAttacks(Square square, Bitboard occupancy,
                        const int directions[][2], std::size_t count) {
    Bitboard attacks = 0;
    const int startRank = rankOf(square);
    const int startFile = fileOf(square);
    for (std::size_t directionIndex = 0; directionIndex < count; ++directionIndex) {
        int rank = startRank + directions[directionIndex][0];
        int file = startFile + directions[directionIndex][1];
        while (rank >= 0 && rank < 8 && file >= 0 && file < 8) {
            const Square target = rank * 8 + file;
            attacks |= squareBit(target);
            if ((occupancy & squareBit(target)) != 0) {
                break;
            }
            rank += directions[directionIndex][0];
            file += directions[directionIndex][1];
        }
    }
    return attacks;
}

}  // namespace

void MoveGenerator::initialize() {
    if (initialized_) {
        return;
    }

    static constexpr int KNIGHT_OFFSETS[][2] = {
        {2, 1}, {2, -1}, {-2, 1}, {-2, -1},
        {1, 2}, {1, -2}, {-1, 2}, {-1, -2}
    };
    static constexpr int KING_OFFSETS[][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1},
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };

    for (Square square = 0; square < 64; ++square) {
        const int rank = rankOf(square);
        const int file = fileOf(square);

        for (const auto& offset : KNIGHT_OFFSETS) {
            const int targetRank = rank + offset[0];
            const int targetFile = file + offset[1];
            if (targetRank >= 0 && targetRank < 8 &&
                targetFile >= 0 && targetFile < 8) {
                knightAttackTable_[static_cast<std::size_t>(square)] |=
                    squareBit(targetRank * 8 + targetFile);
            }
        }
        for (const auto& offset : KING_OFFSETS) {
            const int targetRank = rank + offset[0];
            const int targetFile = file + offset[1];
            if (targetRank >= 0 && targetRank < 8 &&
                targetFile >= 0 && targetFile < 8) {
                kingAttackTable_[static_cast<std::size_t>(square)] |=
                    squareBit(targetRank * 8 + targetFile);
            }
        }

        if (rank < 7) {
            if (file > 0) {
                pawnAttackTable_[WHITE][static_cast<std::size_t>(square)] |=
                    squareBit(square + 7);
            }
            if (file < 7) {
                pawnAttackTable_[WHITE][static_cast<std::size_t>(square)] |=
                    squareBit(square + 9);
            }
        }
        if (rank > 0) {
            if (file > 0) {
                pawnAttackTable_[BLACK][static_cast<std::size_t>(square)] |=
                    squareBit(square - 9);
            }
            if (file < 7) {
                pawnAttackTable_[BLACK][static_cast<std::size_t>(square)] |=
                    squareBit(square - 7);
            }
        }
    }
    initialized_ = true;
}

Bitboard MoveGenerator::knightAttacks(Square square) {
    initialize();
    return knightAttackTable_[static_cast<std::size_t>(square)];
}

Bitboard MoveGenerator::kingAttacks(Square square) {
    initialize();
    return kingAttackTable_[static_cast<std::size_t>(square)];
}

Bitboard MoveGenerator::pawnAttacks(Color color, Square square) {
    initialize();
    return pawnAttackTable_[static_cast<std::size_t>(color)]
                           [static_cast<std::size_t>(square)];
}

Bitboard MoveGenerator::bishopAttacks(Square square, Bitboard occupancy) {
    static constexpr int DIRECTIONS[][2] = {
        {1, 1}, {1, -1}, {-1, 1}, {-1, -1}
    };
    return slidingAttacks(square, occupancy, DIRECTIONS, 4);
}

Bitboard MoveGenerator::rookAttacks(Square square, Bitboard occupancy) {
    static constexpr int DIRECTIONS[][2] = {
        {1, 0}, {-1, 0}, {0, 1}, {0, -1}
    };
    return slidingAttacks(square, occupancy, DIRECTIONS, 4);
}

Bitboard MoveGenerator::queenAttacks(Square square, Bitboard occupancy) {
    return bishopAttacks(square, occupancy) | rookAttacks(square, occupancy);
}

bool MoveGenerator::isSquareAttacked(const Board& board, Square square,
                                     Color byColor) {
    initialize();
    if (!isValidSquare(square)) {
        return false;
    }

    if ((pawnAttackTable_[opposite(byColor)][static_cast<std::size_t>(square)] &
         board.pieces(byColor, PAWN)) != 0) {
        return true;
    }
    if ((knightAttackTable_[static_cast<std::size_t>(square)] &
         board.pieces(byColor, KNIGHT)) != 0) {
        return true;
    }
    if ((kingAttackTable_[static_cast<std::size_t>(square)] &
         board.pieces(byColor, KING)) != 0) {
        return true;
    }
    if ((bishopAttacks(square, board.occupancy()) &
         (board.pieces(byColor, BISHOP) | board.pieces(byColor, QUEEN))) != 0) {
        return true;
    }
    return (rookAttacks(square, board.occupancy()) &
            (board.pieces(byColor, ROOK) | board.pieces(byColor, QUEEN))) != 0;
}

bool MoveGenerator::isInCheck(const Board& board, Color color) {
    const Square king = board.kingSquare(color);
    return king != NO_SQUARE && isSquareAttacked(board, king, opposite(color));
}

void MoveGenerator::generatePawnMoves(const Board& board, std::vector<Move>& moves,
                                      bool capturesOnly) {
    const Color us = board.sideToMove();
    const Color them = opposite(us);
    const int forward = us == WHITE ? 8 : -8;
    const int startingRank = us == WHITE ? 1 : 6;
    const int promotionRank = us == WHITE ? 7 : 0;
    const Bitboard enemyTargets = board.occupancy(them) & ~board.pieces(them, KING);

    Bitboard pawns = board.pieces(us, PAWN);
    while (pawns != 0) {
        const Square from = popLeastSignificantBit(pawns);

        if (!capturesOnly) {
            const Square oneForward = from + forward;
            if (isValidSquare(oneForward) && board.pieceAt(oneForward) == NO_PIECE) {
                if (rankOf(oneForward) == promotionRank) {
                    addPromotions(moves, from, oneForward, QUIET);
                } else {
                    moves.emplace_back(from, oneForward, QUIET);
                    const Square twoForward = from + 2 * forward;
                    if (rankOf(from) == startingRank &&
                        board.pieceAt(twoForward) == NO_PIECE) {
                        moves.emplace_back(from, twoForward, DOUBLE_PAWN_PUSH);
                    }
                }
            }
        }

        Bitboard captures = pawnAttacks(us, from) & enemyTargets;
        while (captures != 0) {
            const Square to = popLeastSignificantBit(captures);
            if (rankOf(to) == promotionRank) {
                addPromotions(moves, from, to, CAPTURE);
            } else {
                moves.emplace_back(from, to, CAPTURE);
            }
        }

        const Square enPassant = board.enPassantSquare();
        if (enPassant != NO_SQUARE &&
            (pawnAttacks(us, from) & squareBit(enPassant)) != 0) {
            moves.emplace_back(from, enPassant,
                               static_cast<std::uint8_t>(CAPTURE | EN_PASSANT));
        }
    }
}

void MoveGenerator::generatePieceMoves(const Board& board, std::vector<Move>& moves,
                                       PieceType type, bool capturesOnly) {
    const Color us = board.sideToMove();
    const Color them = opposite(us);
    const Bitboard allowedTargets = ~board.occupancy(us) & ~board.pieces(them, KING);
    const Bitboard captureTargets = board.occupancy(them) & ~board.pieces(them, KING);

    Bitboard pieces = board.pieces(us, type);
    while (pieces != 0) {
        const Square from = popLeastSignificantBit(pieces);
        Bitboard attacks = 0;
        switch (type) {
            case KNIGHT: attacks = knightAttacks(from); break;
            case BISHOP: attacks = bishopAttacks(from, board.occupancy()); break;
            case ROOK: attacks = rookAttacks(from, board.occupancy()); break;
            case QUEEN: attacks = queenAttacks(from, board.occupancy()); break;
            default: break;
        }
        attacks &= capturesOnly ? captureTargets : allowedTargets;
        while (attacks != 0) {
            const Square to = popLeastSignificantBit(attacks);
            const std::uint8_t flag = board.pieceAt(to) == NO_PIECE ? QUIET : CAPTURE;
            moves.emplace_back(from, to, flag);
        }
    }
}

void MoveGenerator::generateKingMoves(const Board& board, std::vector<Move>& moves,
                                      bool capturesOnly) {
    const Color us = board.sideToMove();
    const Color them = opposite(us);
    const Square from = board.kingSquare(us);
    if (from == NO_SQUARE) {
        return;
    }

    const Bitboard captureTargets = board.occupancy(them) & ~board.pieces(them, KING);
    Bitboard attacks = kingAttacks(from) & ~board.occupancy(us) & ~board.pieces(them, KING);
    if (capturesOnly) {
        attacks &= captureTargets;
    }
    while (attacks != 0) {
        const Square to = popLeastSignificantBit(attacks);
        const std::uint8_t flag = board.pieceAt(to) == NO_PIECE ? QUIET : CAPTURE;
        moves.emplace_back(from, to, flag);
    }

    if (capturesOnly || isSquareAttacked(board, from, them)) {
        return;
    }

    if (us == WHITE && from == 4) {
        if ((board.castlingRights() & WHITE_KINGSIDE) != 0 &&
            board.pieceAt(7) == WHITE_ROOK &&
            board.pieceAt(5) == NO_PIECE && board.pieceAt(6) == NO_PIECE &&
            !isSquareAttacked(board, 5, BLACK) &&
            !isSquareAttacked(board, 6, BLACK)) {
            moves.emplace_back(4, 6, CASTLING);
        }
        if ((board.castlingRights() & WHITE_QUEENSIDE) != 0 &&
            board.pieceAt(0) == WHITE_ROOK &&
            board.pieceAt(1) == NO_PIECE && board.pieceAt(2) == NO_PIECE &&
            board.pieceAt(3) == NO_PIECE &&
            !isSquareAttacked(board, 3, BLACK) &&
            !isSquareAttacked(board, 2, BLACK)) {
            moves.emplace_back(4, 2, CASTLING);
        }
    } else if (us == BLACK && from == 60) {
        if ((board.castlingRights() & BLACK_KINGSIDE) != 0 &&
            board.pieceAt(63) == BLACK_ROOK &&
            board.pieceAt(61) == NO_PIECE && board.pieceAt(62) == NO_PIECE &&
            !isSquareAttacked(board, 61, WHITE) &&
            !isSquareAttacked(board, 62, WHITE)) {
            moves.emplace_back(60, 62, CASTLING);
        }
        if ((board.castlingRights() & BLACK_QUEENSIDE) != 0 &&
            board.pieceAt(56) == BLACK_ROOK &&
            board.pieceAt(57) == NO_PIECE && board.pieceAt(58) == NO_PIECE &&
            board.pieceAt(59) == NO_PIECE &&
            !isSquareAttacked(board, 59, WHITE) &&
            !isSquareAttacked(board, 58, WHITE)) {
            moves.emplace_back(60, 58, CASTLING);
        }
    }
}

void MoveGenerator::generatePseudoLegalMoves(const Board& board,
                                             std::vector<Move>& moves,
                                             bool capturesOnly) {
    initialize();
    moves.clear();
    moves.reserve(128);
    generatePawnMoves(board, moves, capturesOnly);
    generatePieceMoves(board, moves, KNIGHT, capturesOnly);
    generatePieceMoves(board, moves, BISHOP, capturesOnly);
    generatePieceMoves(board, moves, ROOK, capturesOnly);
    generatePieceMoves(board, moves, QUEEN, capturesOnly);
    generateKingMoves(board, moves, capturesOnly);
}

void MoveGenerator::generateLegalMoves(Board& board, std::vector<Move>& moves,
                                       bool capturesOnly) {
    std::vector<Move> pseudoLegal;
    generatePseudoLegalMoves(board, pseudoLegal, capturesOnly);
    moves.clear();
    moves.reserve(pseudoLegal.size());

    const Color movingSide = board.sideToMove();
    for (Move move : pseudoLegal) {
        UndoState undo;
        if (!board.makeMove(move, undo)) {
            continue;
        }
        const Square king = board.kingSquare(movingSide);
        const bool legal = king != NO_SQUARE &&
            !isSquareAttacked(board, king, board.sideToMove());
        board.unmakeMove(move, undo);
        if (legal) {
            moves.push_back(move);
        }
    }
}

std::vector<Move> MoveGenerator::generateLegalMoves(Board& board,
                                                    bool capturesOnly) {
    std::vector<Move> moves;
    generateLegalMoves(board, moves, capturesOnly);
    return moves;
}

}  // namespace chess
