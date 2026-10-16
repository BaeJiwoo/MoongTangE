#pragma once
#include "Common/Protocol/InputCommand.h"
#include "Common/Protocol/WorldSnapshot.h"
#include <bit>
#include <span>
#include <set>
#include <limits>
namespace sandbox::wire {
static_assert(sizeof(double)==8 && std::numeric_limits<double>::is_iec559, "Protocol requires IEEE-754 binary64");
using Bytes = std::vector<std::uint8_t>;
inline constexpr std::uint16_t version=1;
inline constexpr std::uint32_t maxFrame=4096;
enum class Type : std::uint8_t { ClientHello=1, ServerHello=2, Input=3, Snapshot=4, Disconnect=5 };
inline void require(bool ok) { if (!ok) throw std::invalid_argument("Invalid protocol message"); }
struct Writer {
    Bytes data;
    void integer(std::uint64_t value, unsigned bytes) {
        for (unsigned i=bytes; i>0; --i) data.push_back(static_cast<std::uint8_t>(value >> ((i-1)*8)));
    }
    void real(double value) { require(std::isfinite(value)); integer(std::bit_cast<std::uint64_t>(value),8); }
    void vector(Vector3 v) { real(v.x); real(v.y); real(v.z); }
};
struct Reader {
    std::span<const std::uint8_t> data;
    std::size_t cursor{};
    std::uint64_t integer(unsigned bytes) {
        require(bytes <= data.size()-cursor);
        std::uint64_t value=0;
        for (unsigned i=0; i<bytes; ++i) value=(value<<8)|data[cursor++];
        return value;
    }
    double real() { const auto value=std::bit_cast<double>(integer(8)); require(std::isfinite(value)); return value; }
    Vector3 vector() { const double x=real(), y=real(), z=real(); return {x,y,z}; }
    void end() const { require(cursor==data.size()); }
};
inline Type type(std::span<const std::uint8_t> data) { require(!data.empty()); return static_cast<Type>(data[0]); }
inline Bytes hello(std::uint32_t id=0) {
    Writer w;
    w.integer(static_cast<unsigned>(id ? Type::ServerHello : Type::ClientHello),1);
    w.integer(version,2);
    if (id) w.integer(id,4);
    return w.data;
}
inline std::uint32_t readHello(std::span<const std::uint8_t> data, bool server) {
    Reader r{data};
    require(r.integer(1)==static_cast<unsigned>(server ? Type::ServerHello : Type::ClientHello));
    require(r.integer(2)==version);
    const auto id=server ? static_cast<std::uint32_t>(r.integer(4)) : 0;
    require(!server || id!=0); r.end(); return id;
}
inline Bytes input(InputCommand command) {
    require(command.valid());
    Writer w; w.integer(static_cast<unsigned>(Type::Input),1);
    w.integer(command.sequence,8); w.real(command.x); w.real(command.z); return w.data;
}
inline InputCommand readInput(std::span<const std::uint8_t> data, std::uint32_t sessionId) {
    Reader r{data}; require(r.integer(1)==static_cast<unsigned>(Type::Input));
    InputCommand result{sessionId,r.integer(8),r.real(),r.real()}; r.end();
    require(result.valid()); return result;
}
inline Bytes snapshot(const WorldSnapshot& state) {
    require(state.players.size()<=maxPlayers);
    Writer w; w.integer(static_cast<unsigned>(Type::Snapshot),1); w.integer(state.tick,8); w.integer(state.players.size(),2);
    std::set<std::uint32_t> ids;
    for (const auto& p : state.players) {
        require(p.id && ids.insert(p.id).second);
        w.integer(p.id,4); w.vector(p.position); w.vector(p.velocity);
    }
    return w.data;
}
inline WorldSnapshot readSnapshot(std::span<const std::uint8_t> data) {
    Reader r{data}; require(r.integer(1)==static_cast<unsigned>(Type::Snapshot));
    WorldSnapshot state{r.integer(8),{}};
    const auto count=r.integer(2); require(count<=maxPlayers);
    std::set<std::uint32_t> ids;
    for (std::uint64_t i=0; i<count; ++i) {
        PlayerState p; p.id=static_cast<std::uint32_t>(r.integer(4)); p.position=r.vector(); p.velocity=r.vector();
        require(p.id && ids.insert(p.id).second); state.players.push_back(p);
    }
    r.end(); return state;
}
inline Bytes frame(std::span<const std::uint8_t> payload) {
    require(!payload.empty() && payload.size()<=maxFrame);
    Writer w; w.integer(payload.size(),4); w.data.insert(w.data.end(),payload.begin(),payload.end()); return w.data;
}
inline std::uint32_t frameSize(std::span<const std::uint8_t> header) {
    Reader r{header}; const auto size=static_cast<std::uint32_t>(r.integer(4)); r.end();
    require(size>0 && size<=maxFrame); return size;
}
inline bool isDisconnect(std::span<const std::uint8_t> payload) {
    return payload.size()==1 && type(payload)==Type::Disconnect;
}
}
