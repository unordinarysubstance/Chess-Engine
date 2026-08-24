#pragma once

#include "chess/types.hpp"

#include <cstdint>
#include <iosfwd>
#include <string>

namespace chess {

class Move {
public:
    constexpr Move() = default;

    constexpr Move(Square from, Square to, std::uint8_t flags = QUIET,
                   PieceType promotion = NO_PIECE_TYPE)
        : code_(static_cast<std::uint32_t>(from) |
                (static_cast<std::uint32_t>(to) << 6U) |
                (static_cast<std::uint32_t>(promotion == NO_PIECE_TYPE ? 0 :
                     static_cast<int>(promotion) + 1) << 12U) |
                (static_cast<std::uint32_t>(flags) << 16U)) {}

    [[nodiscard]] constexpr Square from() const {
        return static_cast<Square>(code_ & 0x3FU);
    }

    [[nodiscard]] constexpr Square to() const {
        return static_cast<Square>((code_ >> 6U) & 0x3FU);
    }

    [[nodiscard]] constexpr PieceType promotionType() const {
        const auto value = static_cast<int>((code_ >> 12U) & 0xFU);
        return value == 0 ? NO_PIECE_TYPE : static_cast<PieceType>(value - 1);
    }

    [[nodiscard]] constexpr std::uint8_t flags() const {
        return static_cast<std::uint8_t>((code_ >> 16U) & 0xFFU);
    }

    [[nodiscard]] constexpr bool isCapture() const {
        return (flags() & (CAPTURE | EN_PASSANT)) != 0;
    }

    [[nodiscard]] constexpr bool isPromotion() const {
        return (flags() & PROMOTION) != 0;
    }

    [[nodiscard]] constexpr bool isNull() const {
        return code_ == 0;
    }

    [[nodiscard]] constexpr std::uint32_t raw() const {
        return code_;
    }

    [[nodiscard]] std::string toUci() const;

    friend constexpr bool operator==(Move lhs, Move rhs) {
        return lhs.code_ == rhs.code_;
    }

    friend constexpr bool operator!=(Move lhs, Move rhs) {
        return !(lhs == rhs);
    }

private:
    std::uint32_t code_ = 0;
};

std::ostream& operator<<(std::ostream& output, Move move);

}  // namespace chess

