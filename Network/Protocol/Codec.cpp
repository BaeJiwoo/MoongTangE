#include "Network/Protocol/Codec.h"

#include "Common/Protocol/AdmissionTicket.h"
#include "game.pb.h"

#include <google/protobuf/io/coded_stream.h>
#include <set>
#include <stdexcept>
#include <utility>

namespace MoongTangE::wire {
namespace {

template <class Message> Bytes encode(MessageType kind, const Message& message)
{
    const auto size = message.ByteSizeLong();
    require(size < MAX_FRAME_BYTES);

    Bytes data(size + 1);
    data.front() = static_cast<std::uint8_t>(kind);
    require(message.SerializeToArray(data.data() + 1, static_cast<int>(size)));
    return data;
}

template <class Message> Message decode(std::span<const std::uint8_t> data, MessageType expected)
{
    require(data.size() <= MAX_FRAME_BYTES && type(data) == expected);

    const auto payload = data.subspan(1);
    google::protobuf::io::CodedInputStream stream(payload.data(), static_cast<int>(payload.size()));
    stream.SetTotalBytesLimit(static_cast<int>(MAX_FRAME_BYTES));
    stream.SetRecursionLimit(16);

    Message message;
    require(message.ParseFromCodedStream(&stream) && stream.ConsumedEntireMessage());
    return message;
}

void write_vector(proto::Vector3& message, Vector3 value)
{
    require(value.Finite());
    message.set_x(value.x);
    message.set_y(value.y);
    message.set_z(value.z);
}

Vector3 read_vector(const proto::Vector3& message)
{
    require(message.has_x() && message.has_y() && message.has_z());
    const Vector3 value{message.x(), message.y(), message.z()};
    require(value.Finite());
    return value;
}

void write_quaternion(proto::Quaternion& message, Quaternion value)
{
    require(value.Unit());
    message.set_w(value.w);
    message.set_x(value.x);
    message.set_y(value.y);
    message.set_z(value.z);
}

Quaternion read_quaternion(const proto::Quaternion& message)
{
    require(message.has_w() && message.has_x() && message.has_y() && message.has_z());
    const Quaternion value{message.w(), message.x(), message.y(), message.z()};
    require(value.Unit());
    return value;
}

} // namespace

void require(bool condition)
{
    if (!condition) {
        throw std::invalid_argument("Invalid protocol message");
    }
}

MessageType type(std::span<const std::uint8_t> data)
{
    require(!data.empty() && data.front() >= static_cast<unsigned>(MessageType::CLIENT_HELLO) &&
            data.front() <= static_cast<unsigned>(MessageType::DISCONNECT));
    return static_cast<MessageType>(data.front());
}

Bytes client_hello(const std::string& ticket)
{
    require(ticket.empty() || valid_admission_ticket(ticket));
    proto::ClientHello message;
    message.set_protocol_version(PROTOCOL_VERSION);
    message.set_admission_ticket(ticket);
    return encode(MessageType::CLIENT_HELLO, message);
}

Bytes server_hello(AdmissionResult admission)
{
    require((admission.status == AdmissionStatus::ACCEPTED && admission.playerId != 0 &&
             admission.roomId != 0) ||
            ((admission.status == AdmissionStatus::FULL ||
              admission.status == AdmissionStatus::UNAVAILABLE) &&
             admission.playerId == 0 && admission.roomId == 0));
    proto::ServerHello message;
    message.set_protocol_version(PROTOCOL_VERSION);
    message.set_player_id(admission.playerId);
    message.set_room_id(admission.roomId);
    message.set_status(static_cast<proto::ServerHello::AdmissionStatus>(admission.status));
    return encode(MessageType::SERVER_HELLO, message);
}

std::string read_client_hello(std::span<const std::uint8_t> data)
{
    const auto message = decode<proto::ClientHello>(data, MessageType::CLIENT_HELLO);
    require(message.protocol_version() == PROTOCOL_VERSION);
    require(message.admission_ticket().empty() ||
            valid_admission_ticket(message.admission_ticket()));
    return message.admission_ticket();
}

AdmissionResult read_server_hello(std::span<const std::uint8_t> data)
{
    const auto message = decode<proto::ServerHello>(data, MessageType::SERVER_HELLO);
    require(message.protocol_version() == PROTOCOL_VERSION);
    require(message.status() >= proto::ServerHello::ACCEPTED &&
            message.status() <= proto::ServerHello::UNAVAILABLE);
    const AdmissionResult admission{static_cast<AdmissionStatus>(message.status()),
                                    message.player_id(), message.room_id()};
    require(admission.status == AdmissionStatus::ACCEPTED
                ? admission.playerId != 0 && admission.roomId != 0
                : admission.playerId == 0 && admission.roomId == 0);
    return admission;
}

Bytes input(InputCommand command)
{
    require(command.Valid());

    proto::InputCommand message;
    message.set_sequence(command.sequence);
    message.set_x(command.x);
    message.set_z(command.z);
    return encode(MessageType::INPUT, message);
}

InputCommand read_input(std::span<const std::uint8_t> data, std::uint32_t sessionId)
{
    const auto message = decode<proto::InputCommand>(data, MessageType::INPUT);
    require(message.has_x() && message.has_z());
    const InputCommand command{sessionId, message.sequence(), message.x(), message.z()};
    require(command.Valid());
    return command;
}

Bytes snapshot(const RoomSnapshot& room)
{
    const auto& state = room.world;
    require(room.roomId != 0 && state.players.size() <= MAX_PLAYERS);

    proto::WorldSnapshot message;
    message.set_server_tick(state.tick);
    message.set_room_id(room.roomId);

    std::set<std::uint32_t> ids;
    for (const auto& player : state.players) {
        require(player.id != 0 && ids.insert(player.id).second);

        auto& record = *message.add_players();
        record.set_id(player.id);
        write_vector(*record.mutable_position(), player.position);
        write_vector(*record.mutable_velocity(), player.velocity);
        write_quaternion(*record.mutable_orientation(), player.orientation);
        write_vector(*record.mutable_angular_velocity(), player.angularVelocity);
    }
    return encode(MessageType::SNAPSHOT, message);
}

RoomSnapshot read_snapshot(std::span<const std::uint8_t> data)
{
    const auto message = decode<proto::WorldSnapshot>(data, MessageType::SNAPSHOT);
    require(message.room_id() != 0 && message.has_server_tick() &&
            message.players_size() <= static_cast<int>(MAX_PLAYERS));

    WorldSnapshot state{message.server_tick(), {}};
    state.players.reserve(static_cast<std::size_t>(message.players_size()));

    std::set<std::uint32_t> ids;
    for (const auto& record : message.players()) {
        require(record.id() != 0 && ids.insert(record.id()).second);
        require(record.has_position() && record.has_velocity() && record.has_orientation() &&
                record.has_angular_velocity());

        state.players.push_back(
            {record.id(), read_vector(record.position()), read_vector(record.velocity()),
             read_quaternion(record.orientation()), read_vector(record.angular_velocity())});
    }
    return {message.room_id(), std::move(state)};
}

Bytes disconnect()
{
    return encode(MessageType::DISCONNECT, proto::Disconnect{});
}

bool is_disconnect(std::span<const std::uint8_t> data)
{
    if (type(data) != MessageType::DISCONNECT) {
        return false;
    }

    (void)decode<proto::Disconnect>(data, MessageType::DISCONNECT);
    return true;
}

Bytes frame(std::span<const std::uint8_t> payload)
{
    require(!payload.empty() && payload.size() <= MAX_FRAME_BYTES);

    const auto size = static_cast<std::uint32_t>(payload.size());
    Bytes data;
    data.reserve(FRAME_HEADER_BYTES + size);
    for (unsigned remaining = FRAME_HEADER_BYTES; remaining > 0; --remaining) {
        data.push_back(static_cast<std::uint8_t>(size >> ((remaining - 1) * 8)));
    }

    data.insert(data.end(), payload.begin(), payload.end());
    return data;
}

std::uint32_t frame_size(std::span<const std::uint8_t> header)
{
    require(header.size() == FRAME_HEADER_BYTES);

    std::uint32_t size = 0;
    for (const auto byte : header) {
        size = (size << 8) | byte;
    }

    require(size > 0 && size <= MAX_FRAME_BYTES);
    return size;
}

} // namespace MoongTangE::wire
