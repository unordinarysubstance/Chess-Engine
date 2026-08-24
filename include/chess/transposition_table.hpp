#pragma once

#include "chess/move.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace chess {

enum class BoundType : std::uint8_t {
    NONE,
    EXACT,
    LOWER,
    UPPER
};

struct TranspositionEntry {
    std::uint64_t key = 0;
    Move bestMove{};
    int score = 0;
    int depth = -1;
    BoundType bound = BoundType::NONE;
    std::uint8_t generation = 0;
};

class TranspositionTable {
public:
    explicit TranspositionTable(std::size_t megabytes = 64);

    void resize(std::size_t megabytes);
    void clear();
    void newSearch();

    [[nodiscard]] bool probe(std::uint64_t key, TranspositionEntry& entry) const;
    void store(std::uint64_t key, int depth, int score, BoundType bound,
               Move bestMove);

    [[nodiscard]] std::size_t entryCount() const { return entries_.size(); }
    [[nodiscard]] std::size_t sizeMegabytes() const { return sizeMegabytes_; }

private:
    std::vector<TranspositionEntry> entries_;
    std::size_t mask_ = 0;
    std::size_t sizeMegabytes_ = 0;
    std::uint8_t generation_ = 0;
};

}  // namespace chess

