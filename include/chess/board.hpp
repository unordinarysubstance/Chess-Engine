#pragma once

#include "chess/move.hpp"
#include "chess/types.hpp"

#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

namespace chess {

struct UndoState {
    Piece capturedPiece = NO_PIECE;
    std::uint8_t castlingRights = 0;
    Square enPassantSquare = NO_SQUARE;
    int halfmoveClock = 0;
    int fullmoveNumber = 1;
    std::uint64_t zobristKey = 0;
};

class Board {
public:
    static constexpr const char* START_FEN =
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

    Board();

    void setStartPosition();
    bool setFEN(const std::string& fen, std::string* error = nullptr);
    [[nodiscard]] std::string toFEN() const;
    [[nodiscard]] std::string pretty() const;

    [[nodiscard]] Piece pieceAt(Square square) const { return board_[square]; }
    [[nodiscard]] Bitboard pieces(Piece piece) const {
        return piece == NO_PIECE ? 0 : pieceBitboards_[static_cast<std::size_t>(piece)];
    }
    [[nodiscard]] Bitboard pieces(Color color, PieceType type) const {
        return pieces(makePiece(color, type));
    }
    [[nodiscard]] Bitboard occupancy(Color color) const {
        return colorOccupancy_[static_cast<std::size_t>(color)];
    }
    [[nodiscard]] Bitboard occupancy() const { return allOccupancy_; }
    [[nodiscard]] Color sideToMove() const { return sideToMove_; }
    [[nodiscard]] std::uint8_t castlingRights() const { return castlingRights_; }
    [[nodiscard]] Square enPassantSquare() const { return enPassantSquare_; }
    [[nodiscard]] int halfmoveClock() const { return halfmoveClock_; }
    [[nodiscard]] int fullmoveNumber() const { return fullmoveNumber_; }
    [[nodiscard]] std::uint64_t zobristKey() const { return zobristKey_; }
    [[nodiscard]] Square kingSquare(Color color) const;

    bool makeMove(Move move, UndoState& undo);
    void unmakeMove(Move move, const UndoState& undo);

    [[nodiscard]] bool isFiftyMoveDraw() const { return halfmoveClock_ >= 100; }
    [[nodiscard]] bool isThreefoldRepetition() const;
    [[nodiscard]] bool isValid(std::string* error = nullptr) const;

private:
    std::array<Bitboard, 12> pieceBitboards_{};
    std::array<Bitboard, 2> colorOccupancy_{};
    Bitboard allOccupancy_ = 0;
    std::array<Piece, 64> board_{};
    Color sideToMove_ = WHITE;
    std::uint8_t castlingRights_ = 0;
    Square enPassantSquare_ = NO_SQUARE;
    int halfmoveClock_ = 0;
    int fullmoveNumber_ = 1;
    std::uint64_t zobristKey_ = 0;
    std::vector<std::uint64_t> positionHistory_;

    void clear();
    void addPiece(Piece piece, Square square, bool updateHash);
    void removePiece(Piece piece, Square square, bool updateHash);
    void movePiece(Piece piece, Square from, Square to, bool updateHash);
    void computeZobristKey();
    void updateCastlingRights(Piece movedPiece, Square from, Square to,
                              Piece capturedPiece);
};

std::ostream& operator<<(std::ostream& output, const Board& board);

}  // namespace chess

