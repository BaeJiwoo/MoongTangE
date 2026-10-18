#pragma once
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include <cstdint>
#include <optional>
#include <string>

namespace MoongTangE {
// Call from one consumer thread. Implementations own their background synchronization.
class IClientTransport {
public:
    IClientTransport() = default;
    virtual ~IClientTransport() = default;
    IClientTransport(const IClientTransport&) = delete;
    IClientTransport& operator=(const IClientTransport&) = delete;
    IClientTransport(IClientTransport&&) = delete;
    IClientTransport& operator=(IClientTransport&&) = delete;
    virtual bool connected() const = 0;
    virtual std::uint32_t playerId() const = 0;
    virtual bool send(InputCommand input) = 0;
    virtual std::optional<WorldSnapshot> receive() = 0;
    virtual std::string error() const = 0;
    virtual void close() = 0;
};
} // namespace MoongTangE
