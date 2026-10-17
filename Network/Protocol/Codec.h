#pragma once
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include <cstdint>
#include <span>
#include <vector>

namespace sandbox::wire {
using Bytes = std::vector<std::uint8_t>;
inline constexpr std::uint32_t version = 3;
inline constexpr std::uint32_t maxFrame = 8192;
inline constexpr unsigned frameHeaderBytes = 4;
enum class Type : std::uint8_t {
    ClientHello = 1,
    ServerHello = 2,
    Input = 3,
    Snapshot = 4,
    Disconnect = 5
};

void require(bool condition);
Type type(std::span<const std::uint8_t> data);
Bytes clientHello();
Bytes serverHello(std::uint32_t playerId);
void readClientHello(std::span<const std::uint8_t> data);
std::uint32_t readServerHello(std::span<const std::uint8_t> data);
Bytes input(InputCommand command);
InputCommand readInput(std::span<const std::uint8_t> data, std::uint32_t sessionId);
Bytes snapshot(const WorldSnapshot& state);
WorldSnapshot readSnapshot(std::span<const std::uint8_t> data);
Bytes disconnect();
bool isDisconnect(std::span<const std::uint8_t> data);

// TCP boundaries remain independent of Protobuf's message encoding.
Bytes frame(std::span<const std::uint8_t> payload);
std::uint32_t frameSize(std::span<const std::uint8_t> header);
} // namespace sandbox::wire
