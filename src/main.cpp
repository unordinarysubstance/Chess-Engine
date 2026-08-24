#include "chess/board.hpp"
#include "chess/perft.hpp"
#include "chess/uci.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    if (argc >= 2 && std::string(argv[1]) == "--help") {
        std::cout
            << "HighPerformanceChessEngine 1.0\n\n"
            << "Usage:\n"
            << "  chess_engine                 Start the UCI command loop\n"
            << "  chess_engine --perft DEPTH   Run start-position perft\n"
            << "  chess_engine --help          Show this help\n";
        return 0;
    }

    if (argc >= 3 && std::string(argv[1]) == "--perft") {
        const int depth = std::max(0, std::atoi(argv[2]));
        chess::Board board;
        const auto start = std::chrono::steady_clock::now();
        const std::uint64_t nodes = chess::perft(board, depth);
        const double milliseconds = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - start).count();
        std::cout << "perft(" << depth << ") = " << nodes << "  ["
                  << std::fixed << std::setprecision(2) << milliseconds << " ms]\n";
        return 0;
    }

    chess::UciInterface uci;
    uci.run(std::cin, std::cout);
    return 0;
}
