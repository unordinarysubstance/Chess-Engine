#include "chess/zobrist.hpp"

#include <cstddef>

namespace chess {

bool Zobrist::initialized_ = false;
std::array<std::array<std::uint64_t, 64>, 12> Zobrist::pieceKeys_{};
std::uint64_t Zobrist::sideKey_ = 0;
std::array<std::uint64_t, 16> Zobrist::castlingKeys_{};
std::array<std::uint64_t, 8> Zobrist::enPassantKeys_{};

namespace {

std::uint64_t splitMix64(std::uint64_t& state) {
    std::uint64_t value = (state += 0x9E3779B97F4A7C15ULL);
    value = (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value = (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

}  // namespace

void Zobrist::initialize() {
    if (initialized_) {
        return;
    }

    std::uint64_t state = 0xC0FFEE1234ABCDEFULL;
    for (auto& pieceArray : pieceKeys_) {
        for (auto& key : pieceArray) {
            key = splitMix64(state);
        }
    }
    sideKey_ = splitMix64(state);
    for (auto& key : castlingKeys_) {
        key = splitMix64(state);
    }
    for (auto& key : enPassantKeys_) {
        key = splitMix64(state);
    }
    initialized_ = true;
}

std::uint64_t Zobrist::piece(Piece pieceValue, Square square) {
    return pieceKeys_[static_cast<std::size_t>(pieceValue)]
                     [static_cast<std::size_t>(square)];
}

std::uint64_t Zobrist::side() {
    return sideKey_;
}

std::uint64_t Zobrist::castling(std::uint8_t rights) {
    return castlingKeys_[static_cast<std::size_t>(rights & 0x0FU)];
}

std::uint64_t Zobrist::enPassantFile(int file) {
    return enPassantKeys_[static_cast<std::size_t>(file)];
}

}  // namespace chess

