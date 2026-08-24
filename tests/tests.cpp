#include "chess/board.hpp"
#include "chess/engine.hpp"
#include "chess/move_generator.hpp"
#include "chess/perft.hpp"

#include <cstdint>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

class TestFailure : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw TestFailure(message);
    }
}

chess::Move legalMove(chess::Board& board, const std::string& uci) {
    std::vector<chess::Move> moves;
    chess::MoveGenerator::generateLegalMoves(board, moves);
    for (chess::Move move : moves) {
        if (move.toUci() == uci) {
            return move;
        }
    }
    return chess::Move{};
}

void play(chess::Board& board, const std::string& uci) {
    const chess::Move move = legalMove(board, uci);
    require(!move.isNull(), "Expected legal move " + uci);
    chess::UndoState undo;
    require(board.makeMove(move, undo), "makeMove rejected " + uci);
}

void testFenRoundTrip() {
    const std::string fen =
        "r3k2r/p1ppqpb1/bn2pnp1/2pP4/1p2P3/2N2N2/PPQBBPPP/R3K2R w KQkq - 3 17";
    chess::Board board;
    std::string error;
    require(board.setFEN(fen, &error), "FEN parse failed: " + error);
    require(board.toFEN() == fen, "FEN round-trip changed the position");
    require(board.isValid(&error), "Parsed board is invalid: " + error);
}

void testInvalidFen() {
    chess::Board board;
    std::string error;
    require(!board.setFEN("8/8/8/8/8/8/8/8 w - - 0 1", &error),
            "Kingless FEN should be rejected");
    require(!error.empty(), "Invalid FEN should report an error");
}

void testStartPositionPerft() {
    chess::Board board;
    const std::uint64_t expected[] = {1, 20, 400, 8902, 197281, 4865609};
    for (int depth = 0; depth <= 5; ++depth) {
        require(chess::perft(board, depth) == expected[depth],
                "Start-position perft mismatch at depth " + std::to_string(depth));
    }
}

void testKiwipetePerft() {
    chess::Board board;
    std::string error;
    require(board.setFEN(
        "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
        &error), error);
    const std::uint64_t expected[] = {1, 48, 2039, 97862, 4085603};
    for (int depth = 1; depth <= 4; ++depth) {
        require(chess::perft(board, depth) == expected[depth],
                "Kiwipete perft mismatch at depth " + std::to_string(depth));
    }
}

void testEnPassantPerftPosition() {
    chess::Board board;
    std::string error;
    require(board.setFEN(
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", &error), error);
    const std::uint64_t expected[] = {1, 14, 191, 2812, 43238};
    for (int depth = 1; depth <= 4; ++depth) {
        require(chess::perft(board, depth) == expected[depth],
                "Perft position 3 mismatch at depth " + std::to_string(depth));
    }
}

void testMakeUnmakeRestoresState() {
    chess::Board board;
    const std::string initialFen = board.toFEN();
    const std::uint64_t initialHash = board.zobristKey();
    std::vector<chess::Move> moves;
    chess::MoveGenerator::generateLegalMoves(board, moves);

    for (chess::Move move : moves) {
        chess::UndoState undo;
        require(board.makeMove(move, undo), "Generated move could not be made");
        std::string error;
        require(board.isValid(&error), "Board invalid after " + move.toUci() + ": " + error);
        board.unmakeMove(move, undo);
        require(board.toFEN() == initialFen, "FEN not restored after " + move.toUci());
        require(board.zobristKey() == initialHash, "Hash not restored after " + move.toUci());
    }
}

void testCastling() {
    chess::Board board;
    std::string error;
    require(board.setFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", &error), error);
    const chess::Move kingSide = legalMove(board, "e1g1");
    const chess::Move queenSide = legalMove(board, "e1c1");
    require(!kingSide.isNull() && !queenSide.isNull(), "Both white castles should be legal");

    const std::string before = board.toFEN();
    const auto hash = board.zobristKey();
    chess::UndoState undo;
    board.makeMove(kingSide, undo);
    require(board.pieceAt(chess::squareFromString("g1")) == chess::WHITE_KING,
            "King did not land on g1");
    require(board.pieceAt(chess::squareFromString("f1")) == chess::WHITE_ROOK,
            "Rook did not land on f1");
    require((board.castlingRights() &
             (chess::WHITE_KINGSIDE | chess::WHITE_QUEENSIDE)) == 0,
            "White castling rights were not cleared");
    board.unmakeMove(kingSide, undo);
    require(board.toFEN() == before && board.zobristKey() == hash,
            "Castling unmake did not restore state");
}

void testEnPassantMove() {
    chess::Board board;
    std::string error;
    require(board.setFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1", &error), error);
    const chess::Move move = legalMove(board, "e5d6");
    require(!move.isNull() && (move.flags() & chess::EN_PASSANT) != 0,
            "En-passant move was not generated");
    const std::string before = board.toFEN();
    chess::UndoState undo;
    board.makeMove(move, undo);
    require(board.pieceAt(chess::squareFromString("d6")) == chess::WHITE_PAWN,
            "Capturing pawn did not land on d6");
    require(board.pieceAt(chess::squareFromString("d5")) == chess::NO_PIECE,
            "Captured en-passant pawn remains on d5");
    board.unmakeMove(move, undo);
    require(board.toFEN() == before, "En-passant unmake did not restore FEN");
}

void testPromotion() {
    chess::Board board;
    std::string error;
    require(board.setFEN("4k3/P7/8/8/8/8/8/4K3 w - - 0 1", &error), error);
    const chess::Move move = legalMove(board, "a7a8q");
    require(!move.isNull() && move.isPromotion(), "Queen promotion was not generated");
    const auto hash = board.zobristKey();
    chess::UndoState undo;
    board.makeMove(move, undo);
    require(board.pieceAt(chess::squareFromString("a8")) == chess::WHITE_QUEEN,
            "Promotion did not create a queen");
    board.unmakeMove(move, undo);
    require(board.pieceAt(chess::squareFromString("a7")) == chess::WHITE_PAWN,
            "Promotion unmake did not restore pawn");
    require(board.zobristKey() == hash, "Promotion unmake did not restore hash");
}

void testPinnedMoveRejected() {
    chess::Board board;
    std::string error;
    require(board.setFEN("4r1k1/8/8/8/8/8/4R3/4K3 w - - 0 1", &error), error);
    require(legalMove(board, "e2a2").isNull(), "Pinned rook was allowed to expose its king");
    require(!legalMove(board, "e2e8").isNull(), "Pinned rook should be allowed to capture pinner");
}

void testRepetitionAndHashing() {
    chess::Board board;
    const std::uint64_t initialHash = board.zobristKey();
    for (int cycle = 0; cycle < 2; ++cycle) {
        play(board, "g1f3");
        play(board, "g8f6");
        play(board, "f3g1");
        play(board, "f6g8");
    }
    require(board.zobristKey() == initialHash,
            "Equivalent repeated position has a different Zobrist key");
    require(board.isThreefoldRepetition(), "Threefold repetition was not detected");
}

void testTerminalPositions() {
    chess::Board board;
    std::string error;
    require(board.setFEN("7k/6Q1/6K1/8/8/8/8/8 b - - 0 1", &error), error);
    std::vector<chess::Move> moves;
    chess::MoveGenerator::generateLegalMoves(board, moves);
    require(moves.empty(), "Checkmate position has legal moves");
    require(chess::MoveGenerator::isInCheck(board, chess::BLACK),
            "Checkmate position is not recognized as check");

    chess::Engine engine(8);
    const auto result = engine.searchFixedDepth(board, 2, chess::SearchMode::ALPHA_BETA_TT);
    require(result.score <= -chess::MATE_SCORE + 1,
            "Search did not score checkmate correctly");
    require(result.bestMove.isNull(), "Checkmated side should have no best move");
}

void testSearchModesAgreeAndPrune() {
    chess::Board board;
    chess::Engine engine(32);
    const auto minimax = engine.searchFixedDepth(board, 3, chess::SearchMode::MINIMAX);
    const auto alphaBeta = engine.searchFixedDepth(board, 3, chess::SearchMode::ALPHA_BETA);
    const auto withTable = engine.searchFixedDepth(board, 3, chess::SearchMode::ALPHA_BETA_TT);

    require(minimax.score == alphaBeta.score && alphaBeta.score == withTable.score,
            "Search modes returned different minimax scores");
    require(alphaBeta.nodes < minimax.nodes,
            "Alpha-beta did not visit fewer nodes than minimax");
    require(withTable.nodes <= alphaBeta.nodes,
            "Transposition table increased the node count");
    require(!withTable.bestMove.isNull(), "Search returned no best move");
}

}  // namespace

int main() {
    const std::vector<std::pair<std::string, std::function<void()>>> tests = {
        {"FEN round trip", testFenRoundTrip},
        {"invalid FEN", testInvalidFen},
        {"start-position perft", testStartPositionPerft},
        {"Kiwipete perft", testKiwipetePerft},
        {"en-passant perft position", testEnPassantPerftPosition},
        {"make/unmake restoration", testMakeUnmakeRestoresState},
        {"castling", testCastling},
        {"en passant", testEnPassantMove},
        {"promotion", testPromotion},
        {"pinned move", testPinnedMoveRejected},
        {"repetition and hashing", testRepetitionAndHashing},
        {"terminal positions", testTerminalPositions},
        {"search consistency and pruning", testSearchModesAgreeAndPrune}
    };

    int failures = 0;
    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& exception) {
            ++failures;
            std::cerr << "[FAIL] " << name << ": " << exception.what() << '\n';
        }
    }
    std::cout << "\n" << tests.size() - static_cast<std::size_t>(failures)
              << "/" << tests.size() << " tests passed\n";
    return failures == 0 ? 0 : 1;
}
