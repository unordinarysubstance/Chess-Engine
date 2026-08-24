#pragma once

#include "chess/types.hpp"

#include <array>
#include <cstdint>

namespace chess {

class Zobrist {
public:
    static void initialize();
    static std::uint64_t piece(Piece piece, Square square);
    static std::uint64_t side();
    static std::uint64_t castling(std::uint8_t rights);
    static std::uint64_t enPassantFile(int file);

private:
    static bool initialized_;
    static std::array<std::array<std::uint64_t, 64>, 12> pieceKeys_;
    static std::uint64_t sideKey_;
    static std::array<std::uint64_t, 16> castlingKeys_;
    static std::array<std::uint64_t, 8> enPassantKeys_;
};

}  // namespace chess

