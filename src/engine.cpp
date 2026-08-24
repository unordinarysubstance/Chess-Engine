#include "chess/engine.hpp"

#include "chess/move_generator.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace chess {

std::string searchModeName(SearchMode mode) {
    switch (mode) {
        case SearchMode::MINIMAX: return "Minimax";
        case SearchMode::ALPHA_BETA: return "Alpha-Beta";
        case SearchMode::ALPHA_BETA_TT: return "Alpha-Beta + TT";
    }
    return "Unknown";
}

double SearchResult::nodesPerSecond() const {
    if (elapsedMilliseconds <= 0.0) {
        return 0.0;
    }
    return static_cast<double>(nodes) * 1000.0 / elapsedMilliseconds;
}

Engine::Engine(std::size_t transpositionTableMegabytes)
    : transpositionTable_(transpositionTableMegabytes) {
    MoveGenerator::initialize();
}

void Engine::stop() {
    stopRequested_.store(true, std::memory_order_relaxed);
}

void Engine::clearTranspositionTable() {
    transpositionTable_.clear();
}

void Engine::resizeTranspositionTable(std::size_t megabytes) {
    transpositionTable_.resize(megabytes);
}

bool Engine::shouldStop() {
    if (stopRequested_.load(std::memory_order_relaxed)) {
        iterationAborted_ = true;
        return true;
    }
    if (useDeadline_ && (nodes_ & 2047ULL) == 0 &&
        std::chrono::steady_clock::now() >= deadline_) {
        stopRequested_.store(true, std::memory_order_relaxed);
        iterationAborted_ = true;
        return true;
    }
    return false;
}

int Engine::scoreToTranspositionTable(int score, int ply) {
    if (score > MATE_SCORE - MAX_SEARCH_PLY) {
        return score + ply;
    }
    if (score < -MATE_SCORE + MAX_SEARCH_PLY) {
        return score - ply;
    }
    return score;
}

int Engine::scoreFromTranspositionTable(int score, int ply) {
    if (score > MATE_SCORE - MAX_SEARCH_PLY) {
        return score - ply;
    }
    if (score < -MATE_SCORE + MAX_SEARCH_PLY) {
        return score + ply;
    }
    return score;
}

void Engine::updatePrincipalVariation(int ply, Move move) {
    pvTable_[static_cast<std::size_t>(ply)][static_cast<std::size_t>(ply)] = move;
    const int childLength = pvLength_[static_cast<std::size_t>(ply + 1)];
    for (int index = ply + 1; index < childLength; ++index) {
        pvTable_[static_cast<std::size_t>(ply)][static_cast<std::size_t>(index)] =
            pvTable_[static_cast<std::size_t>(ply + 1)][static_cast<std::size_t>(index)];
    }
    pvLength_[static_cast<std::size_t>(ply)] = childLength;
}

std::vector<Move> Engine::currentPrincipalVariation() const {
    std::vector<Move> variation;
    const int length = pvLength_[0];
    variation.reserve(static_cast<std::size_t>(length));
    for (int index = 0; index < length; ++index) {
        variation.push_back(pvTable_[0][static_cast<std::size_t>(index)]);
    }
    return variation;
}

int Engine::moveOrderingScore(const Board& board, Move move, Move ttMove,
                              int ply) const {
    if (!ttMove.isNull() && move == ttMove) {
        return 2'000'000;
    }

    int score = 0;
    const Piece movingPiece = board.pieceAt(move.from());
    if (move.isCapture()) {
        const Piece capturedPiece = (move.flags() & EN_PASSANT) != 0
            ? makePiece(opposite(board.sideToMove()), PAWN)
            : board.pieceAt(move.to());
        score += 1'000'000;
        if (capturedPiece != NO_PIECE) {
            score += 16 * Evaluator::pieceValue(typeOf(capturedPiece));
        }
        score -= Evaluator::pieceValue(typeOf(movingPiece));
    }
    if (move.isPromotion()) {
        score += 800'000 + Evaluator::pieceValue(move.promotionType());
    }
    if (!move.isCapture() && !move.isPromotion() && ply < MAX_SEARCH_PLY) {
        if (move == killerMoves_[static_cast<std::size_t>(ply)][0]) {
            score += 700'000;
        } else if (move == killerMoves_[static_cast<std::size_t>(ply)][1]) {
            score += 690'000;
        }
        if (movingPiece != NO_PIECE) {
            score += historyHeuristic_[static_cast<std::size_t>(movingPiece)]
                                      [static_cast<std::size_t>(move.to())];
        }
    }
    if ((move.flags() & CASTLING) != 0) {
        score += 5'000;
    }
    return score;
}

void Engine::orderMoves(const Board& board, std::vector<Move>& moves, Move ttMove,
                        int ply) const {
    std::stable_sort(moves.begin(), moves.end(),
        [&](Move left, Move right) {
            return moveOrderingScore(board, left, ttMove, ply) >
                   moveOrderingScore(board, right, ttMove, ply);
        });
}

int Engine::minimax(Board& board, int depth, int ply) {
    ++nodes_;
    pvLength_[static_cast<std::size_t>(ply)] = ply;
    if (shouldStop()) {
        return 0;
    }
    if (ply >= MAX_SEARCH_PLY - 1) {
        return evaluator_.evaluate(board);
    }
    if (ply > 0 && (board.isFiftyMoveDraw() || board.isThreefoldRepetition())) {
        return 0;
    }

    std::vector<Move> moves;
    MoveGenerator::generateLegalMoves(board, moves);
    if (moves.empty()) {
        return MoveGenerator::isInCheck(board, board.sideToMove())
            ? -MATE_SCORE + ply
            : 0;
    }
    if (depth <= 0) {
        return evaluator_.evaluate(board);
    }

    orderMoves(board, moves, Move{}, ply);
    int bestScore = -INFINITY_SCORE;
    for (Move move : moves) {
        UndoState undo;
        board.makeMove(move, undo);
        const int score = -minimax(board, depth - 1, ply + 1);
        board.unmakeMove(move, undo);
        if (iterationAborted_) {
            return 0;
        }
        if (score > bestScore) {
            bestScore = score;
            updatePrincipalVariation(ply, move);
        }
    }
    return bestScore;
}

int Engine::alphaBeta(Board& board, int depth, int alpha, int beta, int ply,
                      bool useTranspositionTable) {
    ++nodes_;
    pvLength_[static_cast<std::size_t>(ply)] = ply;
    if (shouldStop()) {
        return 0;
    }
    if (ply >= MAX_SEARCH_PLY - 1) {
        return evaluator_.evaluate(board);
    }
    if (ply > 0 && (board.isFiftyMoveDraw() || board.isThreefoldRepetition())) {
        return 0;
    }

    const int originalAlpha = alpha;
    const int originalBeta = beta;
    Move ttMove;
    TranspositionEntry ttEntry;
    if (useTranspositionTable) {
        ++ttProbes_;
        if (transpositionTable_.probe(board.zobristKey(), ttEntry)) {
            ++ttHits_;
            ttMove = ttEntry.bestMove;
            if (ttEntry.depth >= depth) {
                const int ttScore = scoreFromTranspositionTable(ttEntry.score, ply);
                if (ttEntry.bound == BoundType::EXACT) {
                    ++ttCutoffs_;
                    return ttScore;
                }
                if (ttEntry.bound == BoundType::LOWER) {
                    alpha = std::max(alpha, ttScore);
                } else if (ttEntry.bound == BoundType::UPPER) {
                    beta = std::min(beta, ttScore);
                }
                if (alpha >= beta) {
                    ++ttCutoffs_;
                    return ttScore;
                }
            }
        }
    }

    std::vector<Move> moves;
    MoveGenerator::generateLegalMoves(board, moves);
    if (moves.empty()) {
        const int terminalScore = MoveGenerator::isInCheck(board, board.sideToMove())
            ? -MATE_SCORE + ply
            : 0;
        if (useTranspositionTable) {
            transpositionTable_.store(
                board.zobristKey(), depth,
                scoreToTranspositionTable(terminalScore, ply),
                BoundType::EXACT, Move{});
        }
        return terminalScore;
    }
    if (depth <= 0) {
        return evaluator_.evaluate(board);
    }

    orderMoves(board, moves, ttMove, ply);
    int bestScore = -INFINITY_SCORE;
    Move bestMove;

    for (Move move : moves) {
        UndoState undo;
        board.makeMove(move, undo);
        const int score = -alphaBeta(board, depth - 1, -beta, -alpha, ply + 1,
                                     useTranspositionTable);
        board.unmakeMove(move, undo);
        if (iterationAborted_) {
            return 0;
        }

        if (score > bestScore) {
            bestScore = score;
            bestMove = move;
        }
        if (score > alpha) {
            alpha = score;
            updatePrincipalVariation(ply, move);
        }
        if (alpha >= beta) {
            ++betaCutoffs_;
            if (!move.isCapture() && !move.isPromotion()) {
                auto& killers = killerMoves_[static_cast<std::size_t>(ply)];
                if (move != killers[0]) {
                    killers[1] = killers[0];
                    killers[0] = move;
                }
                const Piece movedPiece = board.pieceAt(move.from());
                if (movedPiece != NO_PIECE) {
                    historyHeuristic_[static_cast<std::size_t>(movedPiece)]
                                     [static_cast<std::size_t>(move.to())] += depth * depth;
                }
            }
            break;
        }
    }

    if (useTranspositionTable) {
        BoundType bound = BoundType::EXACT;
        if (bestScore <= originalAlpha) {
            bound = BoundType::UPPER;
        } else if (bestScore >= originalBeta) {
            bound = BoundType::LOWER;
        }
        transpositionTable_.store(
            board.zobristKey(), depth,
            scoreToTranspositionTable(bestScore, ply), bound, bestMove);
    }
    return bestScore;
}

SearchResult Engine::runSingleDepth(Board& board, int depth, SearchMode mode) {
    nodes_ = 0;
    ttProbes_ = 0;
    ttHits_ = 0;
    ttCutoffs_ = 0;
    betaCutoffs_ = 0;
    iterationAborted_ = false;
    pvLength_.fill(0);

    const auto start = std::chrono::steady_clock::now();
    int score = 0;
    if (mode == SearchMode::MINIMAX) {
        score = minimax(board, depth, 0);
    } else {
        score = alphaBeta(board, depth, -INFINITY_SCORE, INFINITY_SCORE, 0,
                          mode == SearchMode::ALPHA_BETA_TT);
    }
    const auto end = std::chrono::steady_clock::now();

    SearchResult result;
    result.score = score;
    result.completedDepth = iterationAborted_ ? 0 : depth;
    result.nodes = nodes_;
    result.ttProbes = ttProbes_;
    result.ttHits = ttHits_;
    result.ttCutoffs = ttCutoffs_;
    result.betaCutoffs = betaCutoffs_;
    result.elapsedMilliseconds =
        std::chrono::duration<double, std::milli>(end - start).count();
    result.principalVariation = currentPrincipalVariation();
    if (!result.principalVariation.empty()) {
        result.bestMove = result.principalVariation.front();
    }
    return result;
}

SearchResult Engine::searchFixedDepth(Board& board, int depth, SearchMode mode,
                                      bool clearTable) {
    stopRequested_.store(false, std::memory_order_relaxed);
    useDeadline_ = false;
    killerMoves_ = {};
    historyHeuristic_ = {};
    if (clearTable) {
        transpositionTable_.clear();
    }
    transpositionTable_.newSearch();
    return runSingleDepth(board, std::max(1, depth), mode);
}

SearchResult Engine::searchIterative(
    Board board, const SearchLimits& limits, SearchMode mode,
    const std::function<void(const SearchResult&)>& onIteration) {
    stopRequested_.store(false, std::memory_order_relaxed);
    killerMoves_ = {};
    historyHeuristic_ = {};
    transpositionTable_.newSearch();

    useDeadline_ = limits.moveTimeMilliseconds > 0;
    if (useDeadline_) {
        deadline_ = std::chrono::steady_clock::now() +
            std::chrono::milliseconds(limits.moveTimeMilliseconds);
    }

    SearchResult bestCompleted;
    const int maximumDepth = std::max(1, limits.maxDepth);
    for (int depth = 1; depth <= maximumDepth; ++depth) {
        SearchResult current = runSingleDepth(board, depth, mode);
        if (iterationAborted_) {
            break;
        }
        bestCompleted = current;
        if (onIteration) {
            onIteration(current);
        }
        if (std::abs(current.score) >= MATE_SCORE - MAX_SEARCH_PLY) {
            break;
        }
    }

    useDeadline_ = false;
    if (bestCompleted.bestMove.isNull()) {
        std::vector<Move> legalMoves;
        MoveGenerator::generateLegalMoves(board, legalMoves);
        if (!legalMoves.empty()) {
            bestCompleted.bestMove = legalMoves.front();
        }
    }
    return bestCompleted;
}

}  // namespace chess
