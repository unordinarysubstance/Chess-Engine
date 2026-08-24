#include "chess/board.hpp"
#include "chess/engine.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <string>
#include <vector>

namespace {

struct BenchmarkPosition {
    const char* name;
    const char* fen;
    int defaultDepth;
};

struct Measurement {
    chess::SearchResult result;
    double medianMilliseconds = 0.0;
};

double median(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const std::size_t middle = values.size() / 2;
    if (values.size() % 2 == 0) {
        return (values[middle - 1] + values[middle]) / 2.0;
    }
    return values[middle];
}

Measurement measure(chess::Engine& engine, const BenchmarkPosition& position,
                    int depth, chess::SearchMode mode, int repetitions) {
    std::vector<double> times;
    chess::SearchResult representative;
    for (int repetition = 0; repetition < repetitions; ++repetition) {
        chess::Board board;
        std::string error;
        if (!board.setFEN(position.fen, &error)) {
            std::cerr << "Invalid benchmark FEN: " << error << '\n';
            std::exit(2);
        }
        chess::SearchResult current = engine.searchFixedDepth(board, depth, mode, true);
        if (repetition == 0) {
            representative = current;
        } else if (current.score != representative.score ||
                   current.nodes != representative.nodes) {
            std::cerr << "Non-deterministic benchmark result for " << position.name << '\n';
            std::exit(3);
        }
        times.push_back(current.elapsedMilliseconds);
    }
    return {representative, median(std::move(times))};
}

}  // namespace

int main(int argc, char** argv) {
    int forcedDepth = 0;
    int ttOnlyDepth = 0;
    int repetitions = 3;
    for (int index = 1; index < argc; ++index) {
        const std::string argument = argv[index];
        if (argument == "--depth" && index + 1 < argc) {
            forcedDepth = std::max(1, std::atoi(argv[++index]));
        } else if (argument == "--tt-only-depth" && index + 1 < argc) {
            ttOnlyDepth = std::max(1, std::atoi(argv[++index]));
        } else if (argument == "--repeat" && index + 1 < argc) {
            repetitions = std::max(1, std::atoi(argv[++index]));
        } else if (argument == "--help") {
            std::cout << "Usage: chess_bench [--depth N] [--repeat N] "
                         "[--tt-only-depth N]\n";
            return 0;
        }
    }

    const std::array<BenchmarkPosition, 3> positions = {{
        {"Starting position", chess::Board::START_FEN, 4},
        {"Kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 3},
        {"Open middlegame", "r1bq1rk1/ppp2ppp/2np1n2/8/2B1P3/2N2N2/PPP2PPP/R1BQ1RK1 w - - 4 9", 4}
    }};
    const std::array<chess::SearchMode, 3> modes = {{
        chess::SearchMode::MINIMAX,
        chess::SearchMode::ALPHA_BETA,
        chess::SearchMode::ALPHA_BETA_TT
    }};

    chess::Engine engine(64);
    std::cout << "Benchmark configuration: median of " << repetitions
              << " run(s), 64 MiB TT\n\n";

    if (ttOnlyDepth > 0) {
        for (const auto& position : positions) {
            const Measurement withoutTable = measure(
                engine, position, ttOnlyDepth, chess::SearchMode::ALPHA_BETA,
                repetitions);
            const Measurement withTable = measure(
                engine, position, ttOnlyDepth, chess::SearchMode::ALPHA_BETA_TT,
                repetitions);
            if (withoutTable.result.score != withTable.result.score) {
                std::cerr << "TT comparison disagreed on score for "
                          << position.name << '\n';
                return 4;
            }
            const double nodeImprovement =
                static_cast<double>(withoutTable.result.nodes) /
                static_cast<double>(std::max<std::uint64_t>(1, withTable.result.nodes));
            const double timeImprovement = withoutTable.medianMilliseconds /
                std::max(0.001, withTable.medianMilliseconds);
            std::cout << position.name << " (depth " << ttOnlyDepth << ")\n"
                      << "  Alpha-Beta:      " << withoutTable.result.nodes << " nodes, "
                      << std::fixed << std::setprecision(2)
                      << withoutTable.medianMilliseconds << " ms\n"
                      << "  Alpha-Beta + TT: " << withTable.result.nodes << " nodes, "
                      << withTable.medianMilliseconds << " ms, "
                      << withTable.result.ttHits << " TT hits\n"
                      << "  Improvement:     " << nodeImprovement
                      << "x nodes, " << timeImprovement << "x time\n\n";
        }
        return 0;
    }

    for (const auto& position : positions) {
        const int depth = forcedDepth > 0 ? forcedDepth : position.defaultDepth;
        std::array<Measurement, 3> measurements;
        for (std::size_t index = 0; index < modes.size(); ++index) {
            measurements[index] = measure(engine, position, depth, modes[index], repetitions);
        }

        const int expectedScore = measurements[0].result.score;
        for (std::size_t index = 1; index < measurements.size(); ++index) {
            if (measurements[index].result.score != expectedScore) {
                std::cerr << "Search modes disagreed on score for " << position.name << '\n';
                return 4;
            }
        }

        std::cout << position.name << " (depth " << depth << ")\n";
        std::cout << std::left << std::setw(20) << "Mode"
                  << std::right << std::setw(14) << "Nodes"
                  << std::setw(13) << "Median ms"
                  << std::setw(13) << "vs minimax"
                  << std::setw(12) << "TT hits"
                  << std::setw(12) << "Best move" << '\n';

        const double baselineNodes = static_cast<double>(measurements[0].result.nodes);
        for (std::size_t index = 0; index < modes.size(); ++index) {
            const auto& measurement = measurements[index];
            const double reduction = baselineNodes /
                static_cast<double>(std::max<std::uint64_t>(1, measurement.result.nodes));
            std::cout << std::left << std::setw(20) << chess::searchModeName(modes[index])
                      << std::right << std::setw(14) << measurement.result.nodes
                      << std::setw(13) << std::fixed << std::setprecision(2)
                      << measurement.medianMilliseconds
                      << std::setw(12) << std::setprecision(2) << reduction << 'x'
                      << std::setw(12) << measurement.result.ttHits
                      << std::setw(12) << measurement.result.bestMove.toUci() << '\n';
        }
        const double ttOverAlphaBeta = static_cast<double>(measurements[1].result.nodes) /
            static_cast<double>(std::max<std::uint64_t>(1, measurements[2].result.nodes));
        std::cout << "TT improvement over alpha-beta alone: " << std::fixed
                  << std::setprecision(2) << ttOverAlphaBeta << "x fewer nodes\n\n";
    }
    return 0;
}
