# TCP protocol v4 — Protobuf

The source of truth for message fields is [game.proto](Network/Protocol/game.proto). [Codec.cpp](Network/Protocol/Codec.cpp) converts between generated messages and the plain C++ game types. Common, ServerCore and LocalTransport do not depend on Protobuf.

The package is `MoongTangE.proto`, producing C++ types under `MoongTangE::proto`; handwritten codecs use `MoongTangE::wire`. Protocol v4 adds admission status and room identity while preserving existing field numbers and type discriminator bytes. This protocol sends ordinary messages directly, without Any type URLs or RPC service names.

The C++ codec names are `wire::MessageType`, `PROTOCOL_VERSION`, `MAX_FRAME_BYTES` and `FRAME_HEADER_BYTES`. Enum values use uppercase names such as `CLIENT_HELLO` and `SNAPSHOT`; their on-wire numbers are unchanged. [TcpChannel.h](Network/TcpChannel.h) declares framing operations and [TcpChannel.cpp](Network/TcpChannel.cpp) implements them. The server's [ServerNetworkRuntime](Server/Network/ServerNetworkRuntime.h) owns sessions and passes decoded inputs to ServerHost. See the [file/function guide](docs/SERVER_ARCHITECTURE.md#파일별-함수-안내).

## Frame and message encoding

Each TCP frame contains a **4-byte unsigned big-endian payload length**, followed by a **1-byte message type** and a **Protobuf body**. Valid payload lengths are 1–8192 bytes, including the type byte. Asio composed reads/writes handle partial transfers. The length prefix is transport framing; Protobuf does not delimit multiple messages in a stream by itself.

| Type byte | Protobuf message | Main fields |
| --- | --- | --- |
| 1 | ClientHello | protocol_version (=4) |
| 2 | ServerHello | protocol_version, status, server-assigned player_id, room_id |
| 3 | InputCommand | sequence, x, z |
| 4 | WorldSnapshot | server_tick, repeated PlayerState, room_id |
| 5 | Disconnect | empty message |

The body uses Protobuf's encoding (varints, length-delimited submessages and binary64 `double` fields), not the old fixed-position big-endian field layout. Example framed ClientHello: `00 00 00 03 01 08 04`. Empty Disconnect has payload `05`. C++ object memory is never transmitted.

The first client message must be ClientHello. The tick thread confirms admission before ServerHello success. No inputs are accepted before admission; snapshots contain only that room. Rejections are flushed before closing the channel, within its existing read deadline. Handshake version must equal 4. Previous v1/v2/v3 encoding is incompatible and rejected; update client and server together. Protobuf library version 36.2 is separate from application protocol version 4.

## Semantic validation

- Input sequence must be nonzero and increase within its connection. x/z must be explicitly present, finite and in [-1,1]. Their zero values are valid. The input message has no player ID; the server supplies its session ID. Hello assigns an ID but is **not user authentication**.
- ServerHello reports ACCEPTED (1), FULL (2), or UNAVAILABLE (3). Success requires nonzero player_id and room_id; rejection requires both IDs to be zero. Unspecified/pending and unknown statuses are rejected.
- Snapshots require a nonzero room_id matching the accepted room. Inputs carry neither room nor player identity.
- A Snapshot must explicitly include server_tick, including when the value is zero. The codec has a defensive ceiling of 32 players with unique nonzero IDs; game rooms enforce the tighter limit of 3.
- Every player must include position, velocity, orientation and angular_velocity. All scalar components must be explicitly present and finite. Orientation is a unit quaternion mapping local geometry to world space, with norm within 1e-6 of 1. Zero or non-unit quaternions are rejected.
- Unknown message type bytes, missing required semantic fields, invalid Protobuf data, oversized frames and unsupported versions close the connection.
- Unknown Protobuf fields are accepted within size/depth limits to allow additive schema changes. Duplicate singular fields follow Protobuf's last-value/merge semantics; the resulting message still passes all game validation. Valid unknown fields are not treated as forbidden trailing bytes. Removed field numbers must be reserved in future schemas and never reused.

With all fields present and maximum uint32 player IDs/uint64 tick and room ID, 32 players produce a 4,311-byte payload including the type byte. The 8KiB frame limit leaves bounded room for additive fields. Parsers enforce this limit before parsing and set a recursion limit of 16. Deserialization is not accepted merely because Protobuf parsing succeeds.

## Simulation and transport

Diagonal inputs are normalized on the server and request up to 10 N of force while grounded. If no new input reaches the world for 120 simulation ticks, input force stops; the ball coasts under contact resistance.

GameWorld advances with fixed dt=1/60. Slow scheduling caps catch-up after eight periods without changing dt. The server publishes the latest snapshot approximately every 50ms. TCP arrival determines which tick consumes an input. The client interpolates toward received positions and orientations, with interval clamped to 1/60–0.1 seconds. It never extrapolates or changes authoritative state. Cross-platform bit-identical physics is not promised.

Tests compare decoded positions, velocities, orientations and ticks across direct and Protobuf paths. They do not require arbitrary Protobuf serialization to have one canonical byte order. A separate known-byte Hello case verifies the discriminator and frame contract.

## Limits and shutdown

- Each room holds 3 players. Default 8 rooms (24 players); --max-rooms accepts 1–256. Rooms are created on demand, fill oldest vacancies first, and expire after --empty-room-seconds (default 30, 0–86400).
- Core admission records and TCP connections each allow twice the configured player capacity, including pending handshakes. Further requests return UNAVAILABLE or close at the socket cap. FULL is decided by the tick thread after allocation fails.
- Server command queue: 1024 regular commands plus at most twice the player capacity in guaranteed leave commands.
- Per connection: at most 32 outbound frames; a slower peer is disconnected.
- Client submission queue: at most 64 inputs awaiting its network thread.
- Client connection and handshake deadline: 5 seconds. A complete inbound frame must arrive within 10 seconds.
- Disconnect, EOF, socket/protocol errors and timeouts close the session and enqueue player removal.
- ServerApplication ends control processing, closes networking and drains its callbacks, then stops the physics core. See [server architecture](docs/SERVER_ARCHITECTURE.md).

This demonstration protocol has no encryption or user authentication. Keep the default loopback binding; explicit public binding is intended only for trusted test networks.

References: [Protobuf C++ tutorial](https://protobuf.dev/getting-started/cpptutorial/), [stream framing](https://protobuf.dev/programming-guides/techniques/#streaming-multiple-messages), [C++ compiler/runtime version matching](https://protobuf.dev/support/cross-version-runtime-guarantee/#cpp-and-rust-specific-guarantees).
