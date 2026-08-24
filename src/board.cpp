#include "chess/board.hpp"

#include "chess/zobrist.hpp"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <ostream>
#include <sstream>
#include <unordered_map>

namespace chess {

namespace {

Piece pieceFromFenCharacter(char character) {
    switch (character) {
        case 'P': return WHITE_PAWN;
        case 'N': return WHITE_KNIGHT;
        case 'B': return WHITE_BISHOP;
        case 'R': return WHITE_ROOK;
        case 'Q': return WHITE_QUEEN;
        case 'K': return WHITE_KING;
        case 'p': return BLACK_PAWN;
        case 'n': return BLACK_KNIGHT;
        case 'b': return BLACK_BISHOP;
        case 'r': return BLACK_ROOK;
        case 'q': return BLACK_QUEEN;
        case 'k': return BLACK_KING;
        default: return NO_PIECE;
    }
}

char fenCharacterFromPiece(Piece piece) {
    static constexpr char CHARACTERS[] = "PNBRQKpnbrqk";
    return piece == NO_PIECE ? ' ' : CHARACTERS[static_cast<int>(piece)];
}

bool setError(std::string* error, const std::string& message) {
    if (error != nullptr) {
        *error = message;
    }
    return false;
}

}  // namespace

Board::Board() {
    Zobrist::initialize();
    setStartPosition();
}

void Board::clear() {
    pieceBitboards_.fill(0);
    colorOccupancy_.fill(0);
    allOccupancy_ = 0;
    board_.fill(NO_PIECE);
    sideToMove_ = WHITE;
    castlingRights_ = 0;
    enPassantSquare_ = NO_SQUARE;
    halfmoveClock_ = 0;
    fullmoveNumber_ = 1;
    zobristKey_ = 0;
    positionHistory_.clear();
}

void Board::setStartPosition() {
    std::string ignored;
    setFEN(START_FEN, &ignored);
}

bool Board::setFEN(const std::string& fen, std::string* error) {
    std::istringstream stream(fen);
    std::string placement;
    std::string side;
    std::string castling;
    std::string enPassant;
    int halfmove = 0;
    int fullmove = 1;

    if (!(stream >> placement >> side >> castling >> enPassant)) {
        return setError(error, "FEN must contain placement, side, castling, and en-passant fields");
    }
    if (!(stream >> halfmove)) {
        halfmove = 0;
        stream.clear();
    }
    if (!(stream >> fullmove)) {
        fullmove = 1;
        stream.clear();
    }
    if (halfmove < 0 || fullmove < 1) {
        return setError(error, "Invalid FEN move counters");
    }

    clear();

    int rank = 7;
    int file = 0;
    for (char character : placement) {
        if (character == '/') {
            if (file != 8 || rank == 0) {
                return setError(error, "Invalid FEN rank width");
            }
            --rank;
            file = 0;
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(character)) != 0) {
            const int emptyCount = character - '0';
            if (emptyCount < 1 || emptyCount > 8 || file + emptyCount > 8) {
                return setError(error, "Invalid FEN empty-square count");
            }
            file += emptyCount;
            continue;
        }

        const Piece piece = pieceFromFenCharacter(character);
        if (piece == NO_PIECE || file >= 8 || rank < 0) {
            return setError(error, "Invalid FEN piece placement");
        }
        addPiece(piece, rank * 8 + file, false);
        ++file;
    }
    if (rank != 0 || file != 8) {
        return setError(error, "FEN must describe exactly eight ranks");
    }

    if (side == "w") {
        sideToMove_ = WHITE;
    } else if (side == "b") {
        sideToMove_ = BLACK;
    } else {
        return setError(error, "Invalid FEN side-to-move field");
    }

    castlingRights_ = 0;
    if (castling != "-") {
        for (char character : castling) {
            switch (character) {
                case 'K': castlingRights_ |= WHITE_KINGSIDE; break;
                case 'Q': castlingRights_ |= WHITE_QUEENSIDE; break;
                case 'k': castlingRights_ |= BLACK_KINGSIDE; break;
                case 'q': castlingRights_ |= BLACK_QUEENSIDE; break;
                default: return setError(error, "Invalid FEN castling field");
            }
        }
    }

    if (enPassant == "-") {
        enPassantSquare_ = NO_SQUARE;
    } else {
        enPassantSquare_ = squareFromString(enPassant);
        if (!isValidSquare(enPassantSquare_)) {
            return setError(error, "Invalid FEN en-passant square");
        }
        const int expectedRank = sideToMove_ == WHITE ? 5 : 2;
        if (rankOf(enPassantSquare_) != expectedRank) {
            return setError(error, "FEN en-passant square has an impossible rank");
        }
    }

    halfmoveClock_ = halfmove;
    fullmoveNumber_ = fullmove;

    std::string validationError;
    if (!isValid(&validationError)) {
        return setError(error, validationError);
    }

    computeZobristKey();
    positionHistory_.push_back(zobristKey_);
    return true;
}

std::string Board::toFEN() const {
    std::ostringstream output;
    for (int rank = 7; rank >= 0; --rank) {
        int emptyCount = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece piece = board_[static_cast<std::size_t>(rank * 8 + file)];
            if (piece == NO_PIECE) {
                ++emptyCount;
            } else {
                if (emptyCount != 0) {
                    output << emptyCount;
                    emptyCount = 0;
                }
                output << fenCharacterFromPiece(piece);
            }
        }
        if (emptyCount != 0) {
            output << emptyCount;
        }
        if (rank != 0) {
            output << '/';
        }
    }

    output << ' ' << (sideToMove_ == WHITE ? 'w' : 'b') << ' ';
    if (castlingRights_ == 0) {
        output << '-';
    } else {
        if ((castlingRights_ & WHITE_KINGSIDE) != 0) output << 'K';
        if ((castlingRights_ & WHITE_QUEENSIDE) != 0) output << 'Q';
        if ((castlingRights_ & BLACK_KINGSIDE) != 0) output << 'k';
        if ((castlingRights_ & BLACK_QUEENSIDE) != 0) output << 'q';
    }
    output << ' ';
    if (enPassantSquare_ == NO_SQUARE) {
        output << '-';
    } else {
        output << squareToString(enPassantSquare_);
    }
    output << ' ' << halfmoveClock_ << ' ' << fullmoveNumber_;
    return output.str();
}

std::string Board::pretty() const {
    std::ostringstream output;
    output << "\n";
    for (int rank = 7; rank >= 0; --rank) {
        output << ' ' << rank + 1 << "  ";
        for (int file = 0; file < 8; ++file) {
            const Piece piece = board_[static_cast<std::size_t>(rank * 8 + file)];
            output << (piece == NO_PIECE ? '.' : fenCharacterFromPiece(piece)) << ' ';
        }
        output << '\n';
    }
    output << "\n    a b c d e f g h\n\n"
           << "Side: " << (sideToMove_ == WHITE ? "white" : "black") << '\n'
           << "FEN:  " << toFEN() << '\n'
           << "Hash: 0x" << std::hex << std::uppercase << zobristKey_ << std::dec << '\n';
    return output.str();
}

Square Board::kingSquare(Color color) const {
    Bitboard king = pieces(color, KING);
    if (king == 0) {
        return NO_SQUARE;
    }
    return static_cast<Square>(__builtin_ctzll(king));
}

void Board::addPiece(Piece piece, Square square, bool updateHash) {
    const Bitboard bit = squareBit(square);
    pieceBitboards_[static_cast<std::size_t>(piece)] |= bit;
    colorOccupancy_[static_cast<std::size_t>(colorOf(piece))] |= bit;
    allOccupancy_ |= bit;
    board_[static_cast<std::size_t>(square)] = piece;
    if (updateHash) {
        zobristKey_ ^= Zobrist::piece(piece, square);
    }
}

void Board::removePiece(Piece piece, Square square, bool updateHash) {
    const Bitboard bit = squareBit(square);
    pieceBitboards_[static_cast<std::size_t>(piece)] &= ~bit;
    colorOccupancy_[static_cast<std::size_t>(colorOf(piece))] &= ~bit;
    allOccupancy_ &= ~bit;
    board_[static_cast<std::size_t>(square)] = NO_PIECE;
    if (updateHash) {
        zobristKey_ ^= Zobrist::piece(piece, square);
    }
}

void Board::movePiece(Piece piece, Square from, Square to, bool updateHash) {
    removePiece(piece, from, updateHash);
    addPiece(piece, to, updateHash);
}

void Board::computeZobristKey() {
    zobristKey_ = 0;
    for (int pieceIndex = 0; pieceIndex < 12; ++pieceIndex) {
        Bitboard pieceBoard = pieceBitboards_[static_cast<std::size_t>(pieceIndex)];
        while (pieceBoard != 0) {
            const Square square = popLeastSignificantBit(pieceBoard);
            zobristKey_ ^= Zobrist::piece(static_cast<Piece>(pieceIndex), square);
        }
    }
    if (sideToMove_ == BLACK) {
        zobristKey_ ^= Zobrist::side();
    }
    zobristKey_ ^= Zobrist::castling(castlingRights_);
    if (enPassantSquare_ != NO_SQUARE) {
        zobristKey_ ^= Zobrist::enPassantFile(fileOf(enPassantSquare_));
    }
}

void Board::updateCastlingRights(Piece movedPiece, Square from, Square to,
                                 Piece capturedPiece) {
    if (movedPiece == WHITE_KING) {
        castlingRights_ &= static_cast<std::uint8_t>(~(WHITE_KINGSIDE | WHITE_QUEENSIDE));
    } else if (movedPiece == BLACK_KING) {
        castlingRights_ &= static_cast<std::uint8_t>(~(BLACK_KINGSIDE | BLACK_QUEENSIDE));
    } else if (movedPiece == WHITE_ROOK) {
        if (from == 0) castlingRights_ &= static_cast<std::uint8_t>(~WHITE_QUEENSIDE);
        if (from == 7) castlingRights_ &= static_cast<std::uint8_t>(~WHITE_KINGSIDE);
    } else if (movedPiece == BLACK_ROOK) {
        if (from == 56) castlingRights_ &= static_cast<std::uint8_t>(~BLACK_QUEENSIDE);
        if (from == 63) castlingRights_ &= static_cast<std::uint8_t>(~BLACK_KINGSIDE);
    }

    if (capturedPiece == WHITE_ROOK) {
        if (to == 0) castlingRights_ &= static_cast<std::uint8_t>(~WHITE_QUEENSIDE);
        if (to == 7) castlingRights_ &= static_cast<std::uint8_t>(~WHITE_KINGSIDE);
    } else if (capturedPiece == BLACK_ROOK) {
        if (to == 56) castlingRights_ &= static_cast<std::uint8_t>(~BLACK_QUEENSIDE);
        if (to == 63) castlingRights_ &= static_cast<std::uint8_t>(~BLACK_KINGSIDE);
    }
}

bool Board::makeMove(Move move, UndoState& undo) {
    const Square from = move.from();
    const Square to = move.to();
    if (!isValidSquare(from) || !isValidSquare(to) || from == to) {
        return false;
    }

    const Piece movedPiece = pieceAt(from);
    if (movedPiece == NO_PIECE || colorOf(movedPiece) != sideToMove_) {
        return false;
    }
    if (pieceAt(to) != NO_PIECE && colorOf(pieceAt(to)) == sideToMove_) {
        return false;
    }

    Square capturedSquare = to;
    if ((move.flags() & EN_PASSANT) != 0) {
        capturedSquare = sideToMove_ == WHITE ? to - 8 : to + 8;
        if (typeOf(movedPiece) != PAWN || pieceAt(to) != NO_PIECE ||
            !isValidSquare(capturedSquare)) {
            return false;
        }
    }
    const Piece capturedPiece = pieceAt(capturedSquare);
    if ((move.flags() & EN_PASSANT) != 0 &&
        capturedPiece != makePiece(opposite(sideToMove_), PAWN)) {
        return false;
    }
    if (capturedPiece != NO_PIECE && typeOf(capturedPiece) == KING) {
        return false;
    }

    if ((move.flags() & PROMOTION) != 0) {
        const PieceType promotion = move.promotionType();
        const int destinationRank = rankOf(to);
        if (typeOf(movedPiece) != PAWN ||
            (destinationRank != 0 && destinationRank != 7) ||
            (promotion != KNIGHT && promotion != BISHOP &&
             promotion != ROOK && promotion != QUEEN)) {
            return false;
        }
    }

    if ((move.flags() & CASTLING) != 0) {
        if (typeOf(movedPiece) != KING ||
            !((from == 4 && (to == 2 || to == 6)) ||
              (from == 60 && (to == 58 || to == 62)))) {
            return false;
        }
        const Square rookFrom = to > from ? from + 3 : from - 4;
        if (pieceAt(rookFrom) != makePiece(sideToMove_, ROOK)) {
            return false;
        }
    }

    undo.capturedPiece = capturedPiece;
    undo.castlingRights = castlingRights_;
    undo.enPassantSquare = enPassantSquare_;
    undo.halfmoveClock = halfmoveClock_;
    undo.fullmoveNumber = fullmoveNumber_;
    undo.zobristKey = zobristKey_;

    zobristKey_ ^= Zobrist::castling(castlingRights_);
    if (enPassantSquare_ != NO_SQUARE) {
        zobristKey_ ^= Zobrist::enPassantFile(fileOf(enPassantSquare_));
    }

    updateCastlingRights(movedPiece, from, to, capturedPiece);
    enPassantSquare_ = NO_SQUARE;

    if (typeOf(movedPiece) == PAWN || capturedPiece != NO_PIECE) {
        halfmoveClock_ = 0;
    } else {
        ++halfmoveClock_;
    }
    if (sideToMove_ == BLACK) {
        ++fullmoveNumber_;
    }

    if (capturedPiece != NO_PIECE) {
        removePiece(capturedPiece, capturedSquare, true);
    }

    if ((move.flags() & CASTLING) != 0) {
        movePiece(movedPiece, from, to, true);
        const Square rookFrom = to > from ? from + 3 : from - 4;
        const Square rookTo = to > from ? from + 1 : from - 1;
        movePiece(makePiece(sideToMove_, ROOK), rookFrom, rookTo, true);
    } else if ((move.flags() & PROMOTION) != 0) {
        removePiece(movedPiece, from, true);
        addPiece(makePiece(sideToMove_, move.promotionType()), to, true);
    } else {
        movePiece(movedPiece, from, to, true);
    }

    if ((move.flags() & DOUBLE_PAWN_PUSH) != 0) {
        enPassantSquare_ = (from + to) / 2;
    }

    sideToMove_ = opposite(sideToMove_);
    zobristKey_ ^= Zobrist::side();
    zobristKey_ ^= Zobrist::castling(castlingRights_);
    if (enPassantSquare_ != NO_SQUARE) {
        zobristKey_ ^= Zobrist::enPassantFile(fileOf(enPassantSquare_));
    }
    positionHistory_.push_back(zobristKey_);
    return true;
}

void Board::unmakeMove(Move move, const UndoState& undo) {
    if (!positionHistory_.empty()) {
        positionHistory_.pop_back();
    }

    sideToMove_ = opposite(sideToMove_);
    const Square from = move.from();
    const Square to = move.to();

    if ((move.flags() & CASTLING) != 0) {
        movePiece(makePiece(sideToMove_, KING), to, from, false);
        const Square rookFrom = to > from ? from + 3 : from - 4;
        const Square rookTo = to > from ? from + 1 : from - 1;
        movePiece(makePiece(sideToMove_, ROOK), rookTo, rookFrom, false);
    } else if ((move.flags() & PROMOTION) != 0) {
        removePiece(makePiece(sideToMove_, move.promotionType()), to, false);
        addPiece(makePiece(sideToMove_, PAWN), from, false);
    } else {
        const Piece movedPiece = pieceAt(to);
        movePiece(movedPiece, to, from, false);
    }

    if (undo.capturedPiece != NO_PIECE) {
        const Square capturedSquare = (move.flags() & EN_PASSANT) != 0
            ? (sideToMove_ == WHITE ? to - 8 : to + 8)
            : to;
        addPiece(undo.capturedPiece, capturedSquare, false);
    }

    castlingRights_ = undo.castlingRights;
    enPassantSquare_ = undo.enPassantSquare;
    halfmoveClock_ = undo.halfmoveClock;
    fullmoveNumber_ = undo.fullmoveNumber;
    zobristKey_ = undo.zobristKey;

    if (positionHistory_.empty()) {
        positionHistory_.push_back(zobristKey_);
    }
}

bool Board::isThreefoldRepetition() const {
    int matches = 0;
    const std::size_t positionsToInspect = std::min(
        positionHistory_.size(), static_cast<std::size_t>(halfmoveClock_ + 1));
    for (std::size_t offset = 0; offset < positionsToInspect; ++offset) {
        const auto index = positionHistory_.size() - 1 - offset;
        if (positionHistory_[index] == zobristKey_) {
            ++matches;
            if (matches >= 3) {
                return true;
            }
        }
    }
    return false;
}

bool Board::isValid(std::string* error) const {
    Bitboard reconstructedWhite = 0;
    Bitboard reconstructedBlack = 0;
    Bitboard reconstructedAll = 0;
    std::array<Bitboard, 12> reconstructedPieces{};

    for (Square square = 0; square < 64; ++square) {
        const Piece piece = board_[static_cast<std::size_t>(square)];
        if (piece == NO_PIECE) {
            continue;
        }
        const Bitboard bit = squareBit(square);
        reconstructedPieces[static_cast<std::size_t>(piece)] |= bit;
        reconstructedAll |= bit;
        if (colorOf(piece) == WHITE) reconstructedWhite |= bit;
        else reconstructedBlack |= bit;
    }

    if (reconstructedPieces != pieceBitboards_ ||
        reconstructedWhite != colorOccupancy_[WHITE] ||
        reconstructedBlack != colorOccupancy_[BLACK] ||
        reconstructedAll != allOccupancy_ ||
        (reconstructedWhite & reconstructedBlack) != 0) {
        return setError(error, "Board arrays and bitboards are inconsistent");
    }
    if (populationCount(pieces(WHITE, KING)) != 1 ||
        populationCount(pieces(BLACK, KING)) != 1) {
        return setError(error, "A legal board representation requires exactly one king per side");
    }
    if ((pieces(WHITE, PAWN) | pieces(BLACK, PAWN)) &
        (0xFFULL | (0xFFULL << 56U))) {
        return setError(error, "Pawns cannot occupy the first or eighth rank");
    }
    return true;
}

std::ostream& operator<<(std::ostream& output, const Board& board) {
    output << board.pretty();
    return output;
}

}  // namespace chess
