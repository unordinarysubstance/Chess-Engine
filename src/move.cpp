#include "chess/move.hpp"

#include <ostream>

namespace chess {

std::string Move::toUci() const {
    if (isNull()) {
        return "0000";
    }

    std::string result = squareToString(from()) + squareToString(to());
    if (isPromotion()) {
        char promotionCharacter = 'q';
        switch (promotionType()) {
            case KNIGHT: promotionCharacter = 'n'; break;
            case BISHOP: promotionCharacter = 'b'; break;
            case ROOK: promotionCharacter = 'r'; break;
            case QUEEN: promotionCharacter = 'q'; break;
            default: break;
        }
        result.push_back(promotionCharacter);
    }
    return result;
}

std::ostream& operator<<(std::ostream& output, Move move) {
    output << move.toUci();
    return output;
}

}  // namespace chess

