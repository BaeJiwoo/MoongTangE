#pragma once
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace sandbox::wire {
static_assert(sizeof(double) == 8 && std::numeric_limits<double>::is_iec559,
              "Protocol requires IEEE-754 binary64");
using Bytes = std::vector<std::uint8_t>;
inline constexpr std::uint16_t version = 2;
inline constexpr std::uint32_t maxFrame = 4096;
inline constexpr unsigned frameHeaderBytes = 4;
inline constexpr unsigned realBytes = 8;
inline constexpr unsigned snapshotHeaderBytes = 1 + 8 + 2;
inline constexpr unsigned playerRecordBytes = 4 + (3 + 3 + 4 + 3) * realBytes;
enum class Type : std::uint8_t {
    ClientHello = 1,
    ServerHello = 2,
    Input = 3,
    Snapshot = 4,
    Disconnect = 5
};

inline void require(bool condition) {
    if (!condition) {
        throw std::invalid_argument("Invalid protocol message");
    }
}

// These classes own the buffer/cursor invariants; callers cannot bypass bounds checks.
class Writer {
public:
    void integer(std::uint64_t value, unsigned byteCount) {
        require(byteCount > 0 && byteCount <= sizeof(value));
        if (byteCount < sizeof(value)) {
            require(value < (std::uint64_t{1} << (byteCount * 8)));
        }
        for (unsigned remaining = byteCount; remaining > 0; --remaining) {
            data_.push_back(static_cast<std::uint8_t>(value >> ((remaining - 1) * 8)));
        }
    }

    void real(double value) {
        require(std::isfinite(value));
        integer(std::bit_cast<std::uint64_t>(value), realBytes);
    }

    void vector(Vector3 value) {
        real(value.x);
        real(value.y);
        real(value.z);
    }

    void quaternion(Quaternion value) {
        require(value.unit());
        real(value.w);
        real(value.x);
        real(value.y);
        real(value.z);
    }

    void append(std::span<const std::uint8_t> bytes) {
        data_.insert(data_.end(), bytes.begin(), bytes.end());
    }

    Bytes take() {
        return std::exchange(data_, {});
    }

private:
    Bytes data_;
};

class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> data) : data_(data) {}

    std::uint64_t integer(unsigned byteCount) {
        require(byteCount > 0 && byteCount <= sizeof(std::uint64_t));
        require(byteCount <= data_.size() - cursor_);
        std::uint64_t value = 0;
        for (unsigned index = 0; index < byteCount; ++index) {
            value = (value << 8) | data_[cursor_++];
        }
        return value;
    }

    double real() {
        const double value = std::bit_cast<double>(integer(realBytes));
        require(std::isfinite(value));
        return value;
    }

    Vector3 vector() {
        // Read in separate statements: function argument evaluation order is not a wire format.
        const double x = real();
        const double y = real();
        const double z = real();
        return {x, y, z};
    }

    Quaternion quaternion() {
        const double w = real();
        const double x = real();
        const double y = real();
        const double z = real();
        const Quaternion value{w, x, y, z};
        require(value.unit());
        return value;
    }

    void end() const {
        require(cursor_ == data_.size());
    }

private:
    std::span<const std::uint8_t> data_;
    std::size_t cursor_{};
};

inline Type type(std::span<const std::uint8_t> data) {
    require(!data.empty());
    return static_cast<Type>(data.front());
}

inline Bytes clientHello() {
    Writer writer;
    writer.integer(static_cast<unsigned>(Type::ClientHello), 1);
    writer.integer(version, 2);
    return writer.take();
}

inline Bytes serverHello(std::uint32_t playerId) {
    require(playerId != 0);
    Writer writer;
    writer.integer(static_cast<unsigned>(Type::ServerHello), 1);
    writer.integer(version, 2);
    writer.integer(playerId, 4);
    return writer.take();
}

inline void readClientHello(std::span<const std::uint8_t> data) {
    Reader reader(data);
    require(reader.integer(1) == static_cast<unsigned>(Type::ClientHello));
    require(reader.integer(2) == version);
    reader.end();
}

inline std::uint32_t readServerHello(std::span<const std::uint8_t> data) {
    Reader reader(data);
    require(reader.integer(1) == static_cast<unsigned>(Type::ServerHello));
    require(reader.integer(2) == version);
    const auto playerId = static_cast<std::uint32_t>(reader.integer(4));
    require(playerId != 0);
    reader.end();
    return playerId;
}

inline Bytes input(InputCommand command) {
    require(command.valid());
    Writer writer;
    writer.integer(static_cast<unsigned>(Type::Input), 1);
    writer.integer(command.sequence, 8);
    writer.real(command.x);
    writer.real(command.z);
    return writer.take();
}

inline InputCommand readInput(std::span<const std::uint8_t> data, std::uint32_t sessionId) {
    Reader reader(data);
    require(reader.integer(1) == static_cast<unsigned>(Type::Input));
    InputCommand command{sessionId, reader.integer(8), reader.real(), reader.real()};
    reader.end();
    require(command.valid());
    return command;
}

inline Bytes snapshot(const WorldSnapshot& state) {
    require(state.players.size() <= maxPlayers);
    Writer writer;
    writer.integer(static_cast<unsigned>(Type::Snapshot), 1);
    writer.integer(state.tick, 8);
    writer.integer(state.players.size(), 2);
    std::set<std::uint32_t> playerIds;
    for (const auto& player : state.players) {
        require(player.id != 0 && playerIds.insert(player.id).second);
        writer.integer(player.id, 4);
        writer.vector(player.position);
        writer.vector(player.velocity);
        writer.quaternion(player.orientation);
        writer.vector(player.angularVelocity);
    }
    return writer.take();
}

inline WorldSnapshot readSnapshot(std::span<const std::uint8_t> data) {
    Reader reader(data);
    require(reader.integer(1) == static_cast<unsigned>(Type::Snapshot));
    WorldSnapshot state{reader.integer(8), {}};
    const auto playerCount = reader.integer(2);
    require(playerCount <= maxPlayers);
    state.players.reserve(static_cast<std::size_t>(playerCount));
    std::set<std::uint32_t> playerIds;
    for (std::uint64_t index = 0; index < playerCount; ++index) {
        PlayerState player;
        player.id = static_cast<std::uint32_t>(reader.integer(4));
        player.position = reader.vector();
        player.velocity = reader.vector();
        player.orientation = reader.quaternion();
        player.angularVelocity = reader.vector();
        require(player.id != 0 && playerIds.insert(player.id).second);
        state.players.push_back(player);
    }
    reader.end();
    return state;
}

inline Bytes frame(std::span<const std::uint8_t> payload) {
    require(!payload.empty() && payload.size() <= maxFrame);
    Writer writer;
    writer.integer(payload.size(), frameHeaderBytes);
    writer.append(payload);
    return writer.take();
}

inline std::uint32_t frameSize(std::span<const std::uint8_t> header) {
    Reader reader(header);
    const auto size = static_cast<std::uint32_t>(reader.integer(frameHeaderBytes));
    reader.end();
    require(size > 0 && size <= maxFrame);
    return size;
}

inline bool isDisconnect(std::span<const std::uint8_t> payload) {
    return payload.size() == 1 && type(payload) == Type::Disconnect;
}
} // namespace sandbox::wire