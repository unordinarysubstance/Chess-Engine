#include "chess/evaluator.hpp"

#include "chess/move_generator.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace chess {

namespace {

int centerBonus(int file, int rank) {
    const int fileDistance = std::min(std::abs(file - 3), std::abs(file - 4));
    const int rankDistance = std::min(std::abs(rank - 3), std::abs(rank - 4));
    return 6 - fileDistance - rankDistance;
}

int pieceSquareBonus(PieceType type, Square whiteOrientedSquare) {
    const int file = fileOf(whiteOrientedSquare);
    const int rank = rankOf(whiteOrientedSquare);
    const int center = centerBonus(file, rank);

    switch (type) {
        case PAWN:
            return rank * 8 + (file >= 2 && file <= 5 ? center * 2 : 0);
        case KNIGHT:
            return center * 8 - (rank == 0 ? 12 : 0);
        case BISHOP:
            return center * 5 + (rank > 0 ? 4 : 0);
        case ROOK:
            return rank == 6 ? 22 : rank * 2;
        case QUEEN:
            return center * 2;
        case KING:
            if (rank <= 1) {
                return (file == 6 || file == 2 ? 28 : 0) - center * 2;
            }
            return -center * 5 - rank * 4;
        default:
            return 0;
    }
}

bool isPassedPawn(const Board& board, Color color, Square square) {
    const Color opponent = opposite(color);
    const int pawnFile = fileOf(square);
    const int pawnRank = rankOf(square);
    Bitboard enemyPawns = board.pieces(opponent, PAWN);

    while (enemyPawns != 0) {
        const Square enemySquare = popLeastSignificantBit(enemyPawns);
        const int enemyFile = fileOf(enemySquare);
        const int enemyRank = rankOf(enemySquare);
        if (std::abs(enemyFile - pawnFile) <= 1) {
            if ((color == WHITE && enemyRank > pawnRank) ||
                (color == BLACK && enemyRank < pawnRank)) {
                return false;
            }
        }
    }
    return true;
}

int mobilityFor(const Board& board, Color color) {
    const Bitboard friendly = board.occupancy(color);
    const Bitboard occupancy = board.occupancy();
    int mobility = 0;

    Bitboard knights = board.pieces(color, KNIGHT);
    while (knights != 0) {
        const Square square = popLeastSignificantBit(knights);
        mobility += populationCount(MoveGenerator::knightAttacks(square) & ~friendly) * 2;
    }
    Bitboard bishops = board.pieces(color, BISHOP);
    while (bishops != 0) {
        const Square square = popLeastSignificantBit(bishops);
        mobility += populationCount(MoveGenerator::bishopAttacks(square, occupancy) & ~friendly) * 2;
    }
    Bitboard rooks = board.pieces(color, ROOK);
    while (rooks != 0) {
        const Square square = popLeastSignificantBit(rooks);
        mobility += populationCount(MoveGenerator::rookAttacks(square, occupancy) & ~friendly);
    }
    Bitboard queens = board.pieces(color, QUEEN);
    while (queens != 0) {
        const Square square = popLeastSignificantBit(queens);
        mobility += populationCount(MoveGenerator::queenAttacks(square, occupancy) & ~friendly);
    }
    return mobility;
}

int structuralScore(const Board& board, Color color) {
    std::array<int, 8> pawnCountByFile{};
    Bitboard pawns = board.pieces(color, PAWN);
    Bitboard pawnCopy = pawns;
    while (pawnCopy != 0) {
        const Square square = popLeastSignificantBit(pawnCopy);
        ++pawnCountByFile[static_cast<std::size_t>(fileOf(square))];
    }

    int score = 0;
    for (int file = 0; file < 8; ++file) {
        if (pawnCountByFile[static_cast<std::size_t>(file)] > 1) {
            score -= 12 * (pawnCountByFile[static_cast<std::size_t>(file)] - 1);
        }
    }

    pawnCopy = pawns;
    while (pawnCopy != 0) {
        const Square square = popLeastSignificantBit(pawnCopy);
        const int file = fileOf(square);
        const bool leftPawn = file > 0 &&
            pawnCountByFile[static_cast<std::size_t>(file - 1)] != 0;
        const bool rightPawn = file < 7 &&
            pawnCountByFile[static_cast<std::size_t>(file + 1)] != 0;
        if (!leftPawn && !rightPawn) {
            score -= 10;
        }
        if (isPassedPawn(board, color, square)) {
            const int relativeRank = color == WHITE ? rankOf(square) : 7 - rankOf(square);
            score += 8 + relativeRank * relativeRank * 3;
        }
    }

    Bitboard rooks = board.pieces(color, ROOK);
    const Bitboard allPawns = board.pieces(WHITE, PAWN) | board.pieces(BLACK, PAWN);
    while (rooks != 0) {
        const Square square = popLeastSignificantBit(rooks);
        const Bitboard fileMask = 0x0101010101010101ULL << fileOf(square);
        if ((allPawns & fileMask) == 0) {
            score += 18;
        } else if ((board.pieces(color, PAWN) & fileMask) == 0) {
            score += 10;
        }
    }
    if (populationCount(board.pieces(color, BISHOP)) >= 2) {
        score += 28;
    }
    return score;
}

}  // namespace

int Evaluator::pieceValue(PieceType type) {
    switch (type) {
        case PAWN: return 100;
        case KNIGHT: return 320;
        case BISHOP: return 330;
        case ROOK: return 500;
        case QUEEN: return 900;
        case KING: return 0;
        default: return 0;
    }
}

int Evaluator::evaluateWhitePerspective(const Board& board) {
    int score = 0;
    for (int colorIndex = 0; colorIndex < 2; ++colorIndex) {
        const Color color = static_cast<Color>(colorIndex);
        const int sign = color == WHITE ? 1 : -1;
        for (int typeIndex = 0; typeIndex < 6; ++typeIndex) {
            const PieceType type = static_cast<PieceType>(typeIndex);
            Bitboard pieces = board.pieces(color, type);
            while (pieces != 0) {
                const Square square = popLeastSignificantBit(pieces);
                const Square orientedSquare = color == WHITE ? square : square ^ 56;
                score += sign * (pieceValue(type) + pieceSquareBonus(type, orientedSquare));
            }
        }
        score += sign * structuralScore(board, color);
        score += sign * mobilityFor(board, color);
    }
    return score;
}

int Evaluator::evaluate(const Board& board) const {
    const int whiteScore = evaluateWhitePerspective(board);
    const int perspectiveScore = board.sideToMove() == WHITE ? whiteScore : -whiteScore;
    return perspectiveScore + 8;
}

}  // namespace chess

