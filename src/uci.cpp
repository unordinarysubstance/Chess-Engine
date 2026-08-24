#include "chess/uci.hpp"

#include "chess/move_generator.hpp"
#include "chess/perft.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

namespace chess {

UciInterface::UciInterface() : engine_(64) {}

UciInterface::~UciInterface() {
    stopSearch();
}

void UciInterface::writeLine(const std::string& line) {
    std::lock_guard<std::mutex> lock(outputMutex_);
    if (output_ != nullptr) {
        *output_ << line << std::endl;
    }
}

void UciInterface::stopSearch() {
    engine_.stop();
    if (searchThread_.joinable()) {
        searchThread_.join();
    }
}

Move UciInterface::parseLegalMove(Board& board, const std::string& text) {
    std::vector<Move> legalMoves;
    MoveGenerator::generateLegalMoves(board, legalMoves);
    for (Move move : legalMoves) {
        if (move.toUci() == text) {
            return move;
        }
    }
    return Move{};
}

std::string UciInterface::scoreForUci(int score) {
    std::ostringstream output;
    if (std::abs(score) >= MATE_SCORE - MAX_SEARCH_PLY) {
        const int plies = MATE_SCORE - std::abs(score);
        const int moves = (plies + 1) / 2;
        output << "mate " << (score >= 0 ? moves : -moves);
    } else {
        output << "cp " << score;
    }
    return output.str();
}

void UciInterface::handlePosition(const std::string& line) {
    std::istringstream stream(line);
    std::string token;
    stream >> token;
    stream >> token;

    Board candidate;
    if (token == "startpos") {
        candidate.setStartPosition();
        if (stream >> token && token != "moves") {
            writeLine("info string position: expected 'moves' after startpos");
            return;
        }
    } else if (token == "fen") {
        std::vector<std::string> fields;
        for (int index = 0; index < 6 && stream >> token; ++index) {
            fields.push_back(token);
        }
        if (fields.size() != 6) {
            writeLine("info string position: incomplete FEN");
            return;
        }
        std::ostringstream fen;
        for (std::size_t index = 0; index < fields.size(); ++index) {
            if (index != 0) fen << ' ';
            fen << fields[index];
        }
        std::string error;
        if (!candidate.setFEN(fen.str(), &error)) {
            writeLine("info string position: " + error);
            return;
        }
        if (stream >> token && token != "moves") {
            writeLine("info string position: expected 'moves' after FEN");
            return;
        }
    } else {
        writeLine("info string position: expected startpos or fen");
        return;
    }

    while (stream >> token) {
        Move move = parseLegalMove(candidate, token);
        if (move.isNull()) {
            writeLine("info string position: illegal move " + token);
            return;
        }
        UndoState undo;
        candidate.makeMove(move, undo);
    }
    board_ = std::move(candidate);
}

void UciInterface::startSearch(const SearchLimits& limits, SearchMode mode) {
    stopSearch();
    const Board position = board_;
    searchThread_ = std::thread([this, position, limits, mode]() mutable {
        const SearchResult result = engine_.searchIterative(
            position, limits, mode,
            [this](const SearchResult& iteration) {
                std::ostringstream info;
                info << "info depth " << iteration.completedDepth
                     << " score " << scoreForUci(iteration.score)
                     << " nodes " << iteration.nodes
                     << " nps " << static_cast<std::uint64_t>(iteration.nodesPerSecond())
                     << " time " << static_cast<std::uint64_t>(iteration.elapsedMilliseconds)
                     << " tthits " << iteration.ttHits
                     << " pv";
                for (Move move : iteration.principalVariation) {
                    info << ' ' << move.toUci();
                }
                writeLine(info.str());
            });
        writeLine("bestmove " + result.bestMove.toUci());
    });
}

void UciInterface::handleGo(const std::string& line) {
    std::istringstream stream(line);
    std::string token;
    stream >> token;

    SearchLimits limits;
    limits.maxDepth = 64;
    int whiteTime = 0;
    int blackTime = 0;
    int whiteIncrement = 0;
    int blackIncrement = 0;
    int movesToGo = 30;
    SearchMode mode = SearchMode::ALPHA_BETA_TT;

    while (stream >> token) {
        if (token == "depth") stream >> limits.maxDepth;
        else if (token == "movetime") stream >> limits.moveTimeMilliseconds;
        else if (token == "wtime") stream >> whiteTime;
        else if (token == "btime") stream >> blackTime;
        else if (token == "winc") stream >> whiteIncrement;
        else if (token == "binc") stream >> blackIncrement;
        else if (token == "movestogo") stream >> movesToGo;
        else if (token == "minimax") mode = SearchMode::MINIMAX;
        else if (token == "alphabeta") mode = SearchMode::ALPHA_BETA;
        else if (token == "tt") mode = SearchMode::ALPHA_BETA_TT;
        else if (token == "infinite") {
            limits.maxDepth = MAX_SEARCH_PLY - 1;
            limits.moveTimeMilliseconds = 0;
        }
    }

    if (limits.moveTimeMilliseconds == 0 && (whiteTime > 0 || blackTime > 0)) {
        const int remaining = board_.sideToMove() == WHITE ? whiteTime : blackTime;
        const int increment = board_.sideToMove() == WHITE ? whiteIncrement : blackIncrement;
        const int divisor = std::max(1, movesToGo);
        const int allocation = remaining / divisor + (increment * 4) / 5;
        limits.moveTimeMilliseconds = std::clamp(allocation, 10, std::max(10, remaining / 2));
    }
    startSearch(limits, mode);
}

void UciInterface::handleSetOption(const std::string& line) {
    std::istringstream stream(line);
    std::string token;
    std::string name;
    std::string value;
    stream >> token;
    stream >> token;
    if (token != "name") return;
    while (stream >> token && token != "value") {
        if (!name.empty()) name += ' ';
        name += token;
    }
    std::getline(stream, value);
    if (!value.empty() && value.front() == ' ') value.erase(value.begin());

    if (name == "Hash") {
        try {
            const auto megabytes = static_cast<std::size_t>(
                std::clamp(std::stoi(value), 1, 2048));
            stopSearch();
            engine_.resizeTranspositionTable(megabytes);
        } catch (...) {
            writeLine("info string Invalid Hash value");
        }
    } else if (name == "Clear Hash") {
        stopSearch();
        engine_.clearTranspositionTable();
    }
}

void UciInterface::handlePerft(const std::string& line) {
    stopSearch();
    std::istringstream stream(line);
    std::string command;
    int depth = 1;
    stream >> command >> depth;
    depth = std::max(0, depth);

    Board copy = board_;
    const auto start = std::chrono::steady_clock::now();
    const auto divide = perftDivide(copy, depth);
    std::uint64_t total = depth == 0 ? 1 : 0;
    for (const auto& [move, nodes] : divide) {
        total += nodes;
        writeLine(move.toUci() + ": " + std::to_string(nodes));
    }
    const double milliseconds = std::chrono::duration<double, std::milli>(
        std::chrono::steady_clock::now() - start).count();
    std::ostringstream summary;
    summary << "Nodes searched: " << total << " in " << std::fixed
            << std::setprecision(2) << milliseconds << " ms";
    writeLine(summary.str());
}

void UciInterface::processCommand(const std::string& line) {
    std::istringstream stream(line);
    std::string command;
    stream >> command;

    if (command == "uci") {
        writeLine("id name HighPerformanceChessEngine 1.0");
        writeLine("id author Shreyan Ray");
        writeLine("option name Hash type spin default 64 min 1 max 2048");
        writeLine("option name Clear Hash type button");
        writeLine("uciok");
    } else if (command == "isready") {
        writeLine("readyok");
    } else if (command == "ucinewgame") {
        stopSearch();
        board_.setStartPosition();
        engine_.clearTranspositionTable();
    } else if (command == "position") {
        stopSearch();
        handlePosition(line);
    } else if (command == "go") {
        handleGo(line);
    } else if (command == "stop") {
        stopSearch();
    } else if (command == "setoption") {
        handleSetOption(line);
    } else if (command == "d") {
        stopSearch();
        std::lock_guard<std::mutex> lock(outputMutex_);
        if (output_ != nullptr) *output_ << board_.pretty() << std::flush;
    } else if (command == "perft") {
        handlePerft(line);
    } else if (command == "help") {
        writeLine("Commands: uci, isready, position, go, stop, perft, d, quit");
    } else if (!command.empty() && command != "quit") {
        writeLine("info string Unknown command: " + command);
    }
}

void UciInterface::run(std::istream& input, std::ostream& output) {
    output_ = &output;
    std::string line;
    while (std::getline(input, line)) {
        processCommand(line);
        if (line == "quit") {
            break;
        }
    }
    stopSearch();
    output_ = nullptr;
}

}  // namespace chess

