#include "Network/Protocol/Codec.h"
#include "TestSupport.h"
#include "game.pb.h"
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <string>

namespace {
using namespace MoongTangE;

template <class Message> wire::Bytes packet(wire::MessageType kind, const Message& message) {
    const auto bytes = message.SerializeAsString();
    wire::Bytes result{static_cast<std::uint8_t>(kind)};
    result.insert(result.end(), bytes.begin(), bytes.end());
    return result;
}

proto::WorldSnapshot protobufSnapshot(const WorldSnapshot& state) {
    const auto bytes = wire::snapshot(state);
    proto::WorldSnapshot message;
    MOONGTANGE_CHECK(message.ParseFromArray(bytes.data() + 1, static_cast<int>(bytes.size() - 1)));
    return message;
}

void framingAndVersionNegotiation() {
    // Known Protobuf bytes: field 1, varint wire type, protocol version 3.
    MOONGTANGE_CHECK(wire::clientHello() == wire::Bytes{1, 8, 3});
    MOONGTANGE_CHECK(wire::frame(wire::clientHello()) == wire::Bytes{0, 0, 0, 3, 1, 8, 3});
    wire::readClientHello(wire::Bytes{1, 8, 3});
    MOONGTANGE_CHECK(wire::readServerHello(wire::serverHello(42)) == 42);
    for (const auto& bad : {wire::Bytes{}, wire::Bytes{1}, wire::Bytes{1, 8}, wire::Bytes{1, 0, 2},
                            wire::Bytes{1, 8, 2}, wire::Bytes{1, 8, 99}, wire::Bytes{2, 8, 3},
                            wire::Bytes{1, 8, 3, 0}, wire::Bytes{255}}) {
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            wire::readClientHello(bad);
        }));
    }
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        wire::serverHello(0);
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        wire::readServerHello(wire::Bytes{2, 8, 3});
    }));
    MOONGTANGE_CHECK(wire::isDisconnect(wire::disconnect()));
    MOONGTANGE_CHECK(!wire::isDisconnect(wire::clientHello()));
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        wire::isDisconnect(wire::Bytes{5, 0});
    }));
    for (const auto& bad : {wire::Bytes{}, wire::Bytes{0, 0, 1}, wire::Bytes{0, 0, 0, 0},
                            wire::Bytes{0, 0, 32, 1}, wire::Bytes{0, 0, 0, 1, 0}}) {
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            wire::frameSize(bad);
        }));
    }
    MOONGTANGE_CHECK(wire::frameSize(wire::Bytes{0, 0, 32, 0}) == wire::MAX_FRAME_BYTES);
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        wire::frame({});
    }));
    MOONGTANGE_CHECK(MoongTangE::test::throws([] {
        wire::frame(wire::Bytes(wire::MAX_FRAME_BYTES + 1));
    }));
    auto oversized = wire::Bytes(wire::MAX_FRAME_BYTES + 1, 0);
    oversized.front() = 1;
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::readClientHello(oversized);
    }));

    // Unknown groups are legal Protobuf, but nesting must remain bounded.
    auto nested = wire::clientHello();
    nested.insert(nested.end(), {0x7b, 0x7c}); // Empty group, field 15.
    wire::readClientHello(nested);
    nested = wire::clientHello();
    nested.insert(nested.end(), 32, 0x7b);
    nested.insert(nested.end(), 32, 0x7c);
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::readClientHello(nested);
    }));
}

void inputsUseSessionIdentityAndValidatePresence() {
    const InputCommand input{42, std::numeric_limits<std::uint64_t>::max(), -0.0, -1.0};
    const auto bytes = wire::input(input);
    const auto restored = wire::readInput(bytes, 7);
    MOONGTANGE_CHECK(restored.playerId == 7 && restored.sequence == input.sequence);
    MOONGTANGE_CHECK(restored.x == input.x && std::signbit(restored.x) && restored.z == -1);
    for (std::size_t size = 0; size < bytes.size(); ++size) {
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            wire::readInput(std::span(bytes).first(size), 7);
        }));
    }
    proto::InputCommand message;
    message.set_sequence(1);
    message.set_x(0);
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::readInput(packet(wire::MessageType::INPUT, message), 7);
    }));
    message.set_z(0);
    MOONGTANGE_CHECK(wire::readInput(packet(wire::MessageType::INPUT, message), 7).x == 0);
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::readInput(packet(wire::MessageType::INPUT, message), 0);
    }));
    for (const double bad :
         {2.0, std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN()}) {
        message.set_x(bad);
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            wire::readInput(packet(wire::MessageType::INPUT, message), 7);
        }));
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            wire::input({7, 1, bad, 0});
        }));
    }
    message.set_x(0);
    message.set_sequence(0);
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::readInput(packet(wire::MessageType::INPUT, message), 7);
    }));
    // Unknown fields support additive schema evolution; they cannot assign identity.
    message.set_sequence(2);
    auto extended = packet(wire::MessageType::INPUT, message);
    extended.insert(extended.end(), {0x78, 99}); // Unknown field 15 = 99.
    MOONGTANGE_CHECK(wire::readInput(extended, 7).playerId == 7);
    auto malformed = bytes;
    malformed.push_back(0); // Invalid Protobuf tag.
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::readInput(malformed, 7);
    }));
}

void snapshotsValidateAllGameInvariants() {
    const WorldSnapshot state{
        42, {{7, {1, 0.5, -3}, {2, 0, 1}, Quaternion::fromRotation({0.1, 0, 0.2}), {2, 3, -4}}}};
    const auto original = protobufSnapshot(state);
    const auto restored = wire::readSnapshot(wire::snapshot(state));
    MOONGTANGE_CHECK(restored.tick == state.tick && restored.players.size() == 1);
    MOONGTANGE_CHECK(restored.players[0].position == state.players[0].position);
    MOONGTANGE_CHECK(restored.players[0].velocity == state.players[0].velocity);
    MOONGTANGE_CHECK(restored.players[0].orientation == state.players[0].orientation);
    MOONGTANGE_CHECK(restored.players[0].angularVelocity == state.players[0].angularVelocity);
    MOONGTANGE_CHECK(wire::readSnapshot(wire::snapshot({0, {}})).tick == 0);
    auto reject = [](const proto::WorldSnapshot& message) {
        MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
            wire::readSnapshot(packet(wire::MessageType::SNAPSHOT, message));
        }));
    };
    auto bad = original;
    bad.clear_server_tick();
    reject(bad);
    bad = original;
    bad.mutable_players(0)->set_id(0);
    reject(bad);
    bad = original;
    bad.add_players()->CopyFrom(original.players(0));
    reject(bad);
    bad = original;
    bad.mutable_players(0)->clear_orientation();
    reject(bad);
    bad = original;
    bad.mutable_players(0)->mutable_position()->clear_x();
    reject(bad);
    bad = original;
    bad.mutable_players(0)->mutable_orientation()->set_w(0);
    reject(bad);
    bad = original;
    bad.mutable_players(0)->mutable_angular_velocity()->set_x(
        std::numeric_limits<double>::quiet_NaN());
    reject(bad);
    bad = original;
    bad.clear_players();
    for (std::uint32_t id = 1; id <= MAX_PLAYERS + 1; ++id) {
        auto& record = *bad.add_players();
        record.CopyFrom(original.players(0));
        record.set_id(id);
    }
    reject(bad);
    auto invalidState = state;
    invalidState.players.push_back(state.players[0]);
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::snapshot(invalidState);
    }));
    invalidState = state;
    invalidState.players[0].orientation = {0, 0, 0, 0};
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::snapshot(invalidState);
    }));
}

void maximumSnapshotFitsBoundedFrame() {
    WorldSnapshot state{std::numeric_limits<std::uint64_t>::max(), {}};
    // Largest varints, all vector/quaternion components present, all 32 records.
    for (std::uint32_t index = 0; index < MAX_PLAYERS; ++index) {
        state.players.push_back({std::numeric_limits<std::uint32_t>::max() - index,
                                 {1, 2, 3},
                                 {4, 5, 6},
                                 {0.5, 0.5, 0.5, 0.5},
                                 {7, 8, 9}});
    }
    const auto bytes = wire::snapshot(state);
    MOONGTANGE_CHECK(bytes.size() == 4300 && bytes.size() <= wire::MAX_FRAME_BYTES);
    MOONGTANGE_CHECK(wire::readSnapshot(bytes).players.size() == MAX_PLAYERS);
    const auto framed = wire::frame(bytes);
    MOONGTANGE_CHECK(wire::frameSize(std::span(framed).first(wire::FRAME_HEADER_BYTES)) ==
                     bytes.size());
    state.players.push_back({1, {}, {}});
    MOONGTANGE_CHECK(MoongTangE::test::throws([&] {
        wire::snapshot(state);
    }));
}
} // namespace

int main() {
    return MoongTangE::test::run([] {
        framingAndVersionNegotiation();
        inputsUseSessionIdentityAndValidatePresence();
        snapshotsValidateAllGameInvariants();
        maximumSnapshotFitsBoundedFrame();
    });
}
