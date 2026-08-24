#pragma once

#include "chess/board.hpp"
#include "chess/engine.hpp"

#include <atomic>
#include <iosfwd>
#include <mutex>
#include <string>
#include <thread>

namespace chess {

class UciInterface {
public:
    UciInterface();
    ~UciInterface();

    void run(std::istream& input, std::ostream& output);

private:
    Board board_;
    Engine engine_;
    std::thread searchThread_;
    std::mutex outputMutex_;
    std::ostream* output_ = nullptr;

    void processCommand(const std::string& line);
    void handlePosition(const std::string& line);
    void handleGo(const std::string& line);
    void handleSetOption(const std::string& line);
    void handlePerft(const std::string& line);
    void startSearch(const SearchLimits& limits, SearchMode mode);
    void stopSearch();
    void writeLine(const std::string& line);
    [[nodiscard]] Move parseLegalMove(Board& board, const std::string& text);
    [[nodiscard]] static std::string scoreForUci(int score);
};

}  // namespace chess

