#include "libsed/codec/token.h"
#include <sstream>
#include <iomanip>

namespace libsed {

std::string Token::toString() const {
    std::ostringstream oss;
    switch (type) {
        case TokenType::TinyAtom:
        case TokenType::ShortAtom:
        case TokenType::MediumAtom:
        case TokenType::LongAtom:
            if (isByteSequence) {
                oss << "Bytes[" << byteData.size() << "]{";
                for (size_t i = 0; i < byteData.size() && i < 16; ++i) {
                    if (i > 0) oss << " ";
                    oss << std::hex << std::setw(2) << std::setfill('0')
                        << static_cast<int>(byteData[i]);
                }
                if (byteData.size() > 16) oss << "...";
                oss << "}";
            } else if (isSigned) {
                oss << "Int(" << intVal << ")";
            } else {
                oss << "Uint(" << uintVal << ")";
            }
            break;
        case TokenType::StartList:       oss << "StartList"; break;
        case TokenType::EndList:         oss << "EndList"; break;
        case TokenType::StartName:       oss << "StartName"; break;
        case TokenType::EndName:         oss << "EndName"; break;
        case TokenType::Call:            oss << "Call"; break;
        case TokenType::EndOfData:       oss << "EndOfData"; break;
        case TokenType::EndOfSession:    oss << "EndOfSession"; break;
        case TokenType::StartTransaction: oss << "StartTransaction"; break;
        case TokenType::EndTransaction:  oss << "EndTransaction"; break;
        case TokenType::EmptyAtom:       oss << "Empty"; break;
        default:                         oss << "Invalid"; break;
    }
    return oss.str();
}

} // namespace libsed
