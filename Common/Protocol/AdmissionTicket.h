#pragma once

#include <algorithm>
#include <string_view>

namespace MoongTangE {

// A matching service generates 32 cryptographically random bytes encoded as lowercase hex.
inline bool valid_admission_ticket(std::string_view ticket)
{
    return ticket.size() == 64 && std::all_of(ticket.begin(), ticket.end(), [](char value) {
               return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
           });
}

} // namespace MoongTangE
