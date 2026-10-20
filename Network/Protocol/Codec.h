#pragma once

#include "Common/Protocol/AdmissionResult.h"
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/RoomSnapshot.h"

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace MoongTangE::wire {

using Bytes = std::vector<std::uint8_t>;

inline constexpr std::uint32_t PROTOCOL_VERSION = 8;
inline constexpr std::uint32_t MAX_FRAME_BYTES = 8192;
inline constexpr unsigned FRAME_HEADER_BYTES = 4;

enum class MessageType : std::uint8_t
{
    CLIENT_HELLO = 1,
    SERVER_HELLO = 2,
    INPUT = 3,
    SNAPSHOT = 4,
    DISCONNECT = 5
};

void require(bool condition);
MessageType type(std::span<const std::uint8_t> data);

struct ClientHello
{
    std::string ticket;
    std::string nickname;
};

Bytes client_hello(const std::string& ticket = {}, const std::string& nickname = {});
Bytes server_hello(AdmissionResult admission);
ClientHello read_client_hello(std::span<const std::uint8_t> data);
AdmissionResult read_server_hello(std::span<const std::uint8_t> data);

Bytes input(InputCommand command);
InputCommand read_input(std::span<const std::uint8_t> data, std::uint32_t sessionId);

Bytes snapshot(const RoomSnapshot& room);
RoomSnapshot read_snapshot(std::span<const std::uint8_t> data);

Bytes disconnect();
bool is_disconnect(std::span<const std::uint8_t> data);

// TCP boundaries remain independent of Protobuf's message encoding.
Bytes frame(std::span<const std::uint8_t> payload);
std::uint32_t frame_size(std::span<const std::uint8_t> header);

} // namespace MoongTangE::wire
