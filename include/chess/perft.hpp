#pragma once

#include "chess/board.hpp"

#include <cstdint>
#include <utility>
#include <vector>

namespace chess {

std::uint64_t perft(Board& board, int depth);
std::vector<std::pair<Move, std::uint64_t>> perftDivide(Board& board, int depth);

}  // namespace chess

