#pragma once

#include <cstddef>
#include <string_view>

namespace MoongTangE {

inline constexpr std::size_t MAX_NICKNAME_LENGTH = 16;

inline bool valid_nickname(std::string_view nickname)
{
    if (nickname.empty() || nickname.size() > MAX_NICKNAME_LENGTH || nickname.front() == ' ' ||
        nickname.back() == ' ') {
        return false;
    }

    for (const unsigned char character : nickname) {
        const bool letter =
            (character >= 'A' && character <= 'Z') || (character >= 'a' && character <= 'z');
        const bool digit = character >= '0' && character <= '9';
        if (!letter && !digit && character != ' ' && character != '_' && character != '-') {
            return false;
        }
    }
    return true;
}

} // namespace MoongTangE
