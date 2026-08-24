#include "chess/transposition_table.hpp"

#include <algorithm>

namespace chess {

TranspositionTable::TranspositionTable(std::size_t megabytes) {
    resize(megabytes);
}

void TranspositionTable::resize(std::size_t megabytes) {
    megabytes = std::max<std::size_t>(1, megabytes);
    const std::size_t bytes = megabytes * 1024U * 1024U;
    const std::size_t maximumEntries = std::max<std::size_t>(1, bytes / sizeof(TranspositionEntry));

    std::size_t powerOfTwoEntries = 1;
    while (powerOfTwoEntries <= maximumEntries / 2) {
        powerOfTwoEntries *= 2;
    }

    entries_.assign(powerOfTwoEntries, TranspositionEntry{});
    mask_ = powerOfTwoEntries - 1;
    sizeMegabytes_ = megabytes;
    generation_ = 0;
}

void TranspositionTable::clear() {
    std::fill(entries_.begin(), entries_.end(), TranspositionEntry{});
}

void TranspositionTable::newSearch() {
    ++generation_;
}

bool TranspositionTable::probe(std::uint64_t key, TranspositionEntry& entry) const {
    const TranspositionEntry& candidate = entries_[static_cast<std::size_t>(key) & mask_];
    if (candidate.bound == BoundType::NONE || candidate.key != key) {
        return false;
    }
    entry = candidate;
    return true;
}

void TranspositionTable::store(std::uint64_t key, int depth, int score,
                               BoundType bound, Move bestMove) {
    TranspositionEntry& entry = entries_[static_cast<std::size_t>(key) & mask_];
    const bool replace = entry.bound == BoundType::NONE || entry.key == key ||
                         entry.generation != generation_ || depth >= entry.depth;
    if (!replace) {
        return;
    }
    entry.key = key;
    entry.depth = depth;
    entry.score = score;
    entry.bound = bound;
    entry.bestMove = bestMove;
    entry.generation = generation_;
}

}  // namespace chess

