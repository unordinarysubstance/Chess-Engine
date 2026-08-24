#pragma once

#include <cstdint>
#include <string>

namespace chess {

using Bitboard = std::uint64_t;
using Square = int;

constexpr Square NO_SQUARE = -1;

enum Color : std::uint8_t {
    WHITE = 0,
    BLACK = 1,
    NO_COLOR = 2
};

constexpr Color opposite(Color color) {
    return color == WHITE ? BLACK : WHITE;
}

enum PieceType : std::uint8_t {
    PAWN = 0,
    KNIGHT = 1,
    BISHOP = 2,
    ROOK = 3,
    QUEEN = 4,
    KING = 5,
    NO_PIECE_TYPE = 6
};

enum Piece : std::int8_t {
    WHITE_PAWN = 0,
    WHITE_KNIGHT = 1,
    WHITE_BISHOP = 2,
    WHITE_ROOK = 3,
    WHITE_QUEEN = 4,
    WHITE_KING = 5,
    BLACK_PAWN = 6,
    BLACK_KNIGHT = 7,
    BLACK_BISHOP = 8,
    BLACK_ROOK = 9,
    BLACK_QUEEN = 10,
    BLACK_KING = 11,
    NO_PIECE = -1
};

constexpr Piece makePiece(Color color, PieceType type) {
    return static_cast<Piece>(static_cast<int>(color) * 6 + static_cast<int>(type));
}

constexpr Color colorOf(Piece piece) {
    return piece == NO_PIECE ? NO_COLOR :
        (static_cast<int>(piece) < 6 ? WHITE : BLACK);
}

constexpr PieceType typeOf(Piece piece) {
    return piece == NO_PIECE ? NO_PIECE_TYPE :
        static_cast<PieceType>(static_cast<int>(piece) % 6);
}

constexpr int fileOf(Square square) {
    return square & 7;
}

constexpr int rankOf(Square square) {
    return square >> 3;
}

constexpr bool isValidSquare(Square square) {
    return square >= 0 && square < 64;
}

constexpr Bitboard squareBit(Square square) {
    return Bitboard{1} << square;
}

inline int popLeastSignificantBit(Bitboard& board) {
    const int square = static_cast<int>(__builtin_ctzll(board));
    board &= board - 1;
    return square;
}

inline int populationCount(Bitboard board) {
    return static_cast<int>(__builtin_popcountll(board));
}

inline std::string squareToString(Square square) {
    if (!isValidSquare(square)) {
        return "--";
    }
    std::string result(2, ' ');
    result[0] = static_cast<char>('a' + fileOf(square));
    result[1] = static_cast<char>('1' + rankOf(square));
    return result;
}

inline Square squareFromString(const std::string& text) {
    if (text.size() != 2 || text[0] < 'a' || text[0] > 'h' ||
        text[1] < '1' || text[1] > '8') {
        return NO_SQUARE;
    }
    return (text[1] - '1') * 8 + (text[0] - 'a');
}

enum CastlingRight : std::uint8_t {
    WHITE_KINGSIDE = 1,
    WHITE_QUEENSIDE = 2,
    BLACK_KINGSIDE = 4,
    BLACK_QUEENSIDE = 8
};

enum MoveFlag : std::uint8_t {
    QUIET = 0,
    CAPTURE = 1,
    DOUBLE_PAWN_PUSH = 2,
    EN_PASSANT = 4,
    CASTLING = 8,
    PROMOTION = 16
};

constexpr int MATE_SCORE = 30000;
constexpr int INFINITY_SCORE = 32000;
constexpr int MAX_SEARCH_PLY = 128;

}  // namespace chess

