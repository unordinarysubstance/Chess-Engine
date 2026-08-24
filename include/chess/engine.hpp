#pragma once

#include "chess/board.hpp"
#include "chess/evaluator.hpp"
#include "chess/transposition_table.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace chess {

enum class SearchMode {
    MINIMAX,
    ALPHA_BETA,
    ALPHA_BETA_TT
};

[[nodiscard]] std::string searchModeName(SearchMode mode);

struct SearchLimits {
    int maxDepth = 6;
    int moveTimeMilliseconds = 0;
};

struct SearchResult {
    Move bestMove{};
    int score = 0;
    int completedDepth = 0;
    std::uint64_t nodes = 0;
    std::uint64_t ttProbes = 0;
    std::uint64_t ttHits = 0;
    std::uint64_t ttCutoffs = 0;
    std::uint64_t betaCutoffs = 0;
    double elapsedMilliseconds = 0.0;
    std::vector<Move> principalVariation;

    [[nodiscard]] double nodesPerSecond() const;
};

class Engine {
public:
    explicit Engine(std::size_t transpositionTableMegabytes = 64);

    SearchResult searchFixedDepth(Board& board, int depth, SearchMode mode,
                                  bool clearTable = true);
    SearchResult searchIterative(
        Board board, const SearchLimits& limits, SearchMode mode,
        const std::function<void(const SearchResult&)>& onIteration = {});

    void stop();
    void clearTranspositionTable();
    void resizeTranspositionTable(std::size_t megabytes);

private:
    Evaluator evaluator_;
    TranspositionTable transpositionTable_;
    std::atomic<bool> stopRequested_{false};
    bool iterationAborted_ = false;
    bool useDeadline_ = false;
    std::chrono::steady_clock::time_point deadline_{};

    std::uint64_t nodes_ = 0;
    std::uint64_t ttProbes_ = 0;
    std::uint64_t ttHits_ = 0;
    std::uint64_t ttCutoffs_ = 0;
    std::uint64_t betaCutoffs_ = 0;

    std::array<std::array<Move, MAX_SEARCH_PLY>, MAX_SEARCH_PLY> pvTable_{};
    std::array<int, MAX_SEARCH_PLY> pvLength_{};
    std::array<std::array<Move, 2>, MAX_SEARCH_PLY> killerMoves_{};
    std::array<std::array<int, 64>, 12> historyHeuristic_{};

    int minimax(Board& board, int depth, int ply);
    int alphaBeta(Board& board, int depth, int alpha, int beta, int ply,
                  bool useTranspositionTable);
    SearchResult runSingleDepth(Board& board, int depth, SearchMode mode);

    void orderMoves(const Board& board, std::vector<Move>& moves, Move ttMove,
                    int ply) const;
    [[nodiscard]] int moveOrderingScore(const Board& board, Move move,
                                        Move ttMove, int ply) const;
    void updatePrincipalVariation(int ply, Move move);
    [[nodiscard]] std::vector<Move> currentPrincipalVariation() const;
    [[nodiscard]] bool shouldStop();
    [[nodiscard]] static int scoreToTranspositionTable(int score, int ply);
    [[nodiscard]] static int scoreFromTranspositionTable(int score, int ply);
};

}  // namespace chess

