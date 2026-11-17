# TCP protocol v10 — Protobuf

The source of truth for message fields is [game.proto](Network/Protocol/game.proto). [Codec.cpp](Network/Protocol/Codec.cpp) converts between generated messages and the plain C++ game types. Common, ServerCore and LocalTransport do not depend on Protobuf.

The package is `MoongTangE.proto`, producing C++ types under `MoongTangE::proto`; handwritten codecs use `MoongTangE::wire`. Protocol v11 adds growth radius, food counts and mine state, requires zero attachment owners and expands bounded frames to 32KiB. Protocol v10 added server-owned rounds, scores, results and round-scoped replay requests. v9 added authoritative defeat state, idempotent restart inputs and wider collectible scales. v8 added collectible attachment ownership; v7 added deterministic collectible states, v6 added server-validated player nicknames and v5 added a single-use admission ticket. Existing field numbers and type discriminator bytes are preserved. This protocol sends ordinary messages directly, without Any type URLs or RPC service names.

The C++ codec names are `wire::MessageType`, `PROTOCOL_VERSION`, `MAX_FRAME_BYTES` and `FRAME_HEADER_BYTES`. Enum values use uppercase names such as `CLIENT_HELLO` and `SNAPSHOT`; their on-wire numbers are unchanged. [NativeTcpChannel](Network/Native/NativeTcpChannel.h) implements the framing contract, while its server runtime owns sessions and passes decoded inputs to ServerHost. See the [file/function guide](docs/SERVER_ARCHITECTURE.md#파일별-함수-안내).

## Frame and message encoding

Each TCP frame contains a **4-byte unsigned big-endian payload length**, followed by a **1-byte message type** and a **Protobuf body**. Valid payload lengths are 1–32768 bytes, including the type byte. The native IOCP/epoll channel completes partial reads and writes before advancing a frame. The length prefix is transport framing; Protobuf does not delimit multiple messages in a stream by itself.

| Type byte | Protobuf message | Main fields |
| --- | --- | --- |
| 1 | ClientHello | protocol_version (=11), admission_ticket, nickname |
| 2 | ServerHello | protocol_version, status, server-assigned player_id, room_id |
| 3 | InputCommand | sequence, x, z, restart_death_tick, next_round_id |
| 4 | WorldSnapshot | server_tick, repeated PlayerState, room_id, repeated CollectibleState, RoundState |
| 5 | Disconnect | empty message |

The body uses Protobuf's encoding (varints, length-delimited submessages and binary64 `double` fields), not the old fixed-position big-endian field layout. Example framed ClientHello without optional text fields: `00 00 00 03 01 08 0a`. Empty Disconnect has payload `05`. C++ object memory is never transmitted.

The first client message must be ClientHello. The tick thread confirms admission before ServerHello success. No inputs are accepted before admission; snapshots contain only that room. Rejections are flushed before closing the channel, within its existing read deadline. Handshake version must equal 11. Previous versions are incompatible and rejected; update client and server together. Protobuf library version 36.2 is separate from application protocol version 11.

ClientHello field 2 is an optional 64-character lowercase hex ticket. Ticket-only mode requires an unexpired reservation on this server, consumed once on the tick thread. Empty tickets use ordinary allocation only when ticket-only mode is disabled. Invalid, unknown, expired or consumed tickets are rejected as UNAVAILABLE, never downgraded to ordinary admission. See [the matching API contract](docs/MATCHMAKING_API.md).

ClientHello field 3 is the requested nickname. The server validates it before claiming a reservation, so an invalid name does not consume the ticket. Current names are 1–16 ASCII characters using letters, digits, spaces, `_` or `-`, without leading or trailing spaces. Accepted names are attached to server snapshots and rendered above player balls.

## Semantic validation

- Live rooms continuously publish `WorldSnapshot.round` as FREE_PLAY with ID/timer zero and competitive=false. There is no countdown, time limit, winner, result phase or room-wide replay.
- Legacy `round_score` and `round_winner` retain explicit presence and are always zero/false in live snapshots. The client ranks living players by current radius/mass instead. Bounded legacy round decoding remains for compatibility with v11 data; it does not drive current gameplay.
- The client sends zero for legacy `next_round_id`. The server ignores any value in this field; it cannot reset the room. Personal restart still uses `restart_death_tick` and resets only the defeated player's body. See [continuous play rules](docs/GAME_LOOP.md).

- Input sequence must be nonzero and increase within its connection. The server keeps only the newest input per admitted player until the next tick. x/z must be explicitly present, finite and in [-1,1]. Their zero values are valid. The input message has no player ID; the server supplies its session ID. Hello assigns an ID but is **not user authentication**.
- `restart_death_tick` is zero unless the player requests a restart. The client repeats the authoritative defeat tick with neutral movement until a live snapshot acknowledges it. The tick thread restarts only a defeated player whose current death tick equals this token; duplicate, old and speculative tokens cannot reset a live player or restart a later defeat. This held request survives input coalescing and uses the existing bounded input slot.
- ServerHello reports ACCEPTED (1), FULL (2), UNAVAILABLE (3), or INVALID_NICKNAME (4). Success requires nonzero player_id and room_id; rejection requires both IDs to be zero. Unspecified/pending and unknown statuses are rejected.
- Snapshots require a nonzero room_id matching the accepted room. Inputs carry neither room nor player identity.
- A Snapshot must explicitly include server_tick, including when the value is zero. The codec has a defensive ceiling of 32 players with unique nonzero IDs; game rooms enforce the tighter limit of 3.
- Every networked player includes its validated nickname together with position, velocity, orientation and angular_velocity. All scalar components must be explicitly present and finite. Orientation is a unit quaternion mapping local geometry to world space, with norm within 1e-6 of 1. Zero or non-unit quaternions are rejected.
- PlayerState field 7 (`death_tick`) must be explicitly present: zero means alive, otherwise it is the tick of the current defeat and cannot exceed `server_tick`. Defeated players retain their session, room slot and frozen position until restart or disconnect. They do not move, collide or collect objects. A restart resets the body at a safe position in the same room without consuming another admission ticket.
- A snapshot contains at most 96 collectibles with unique nonzero IDs. Each collectible has a known kind (`APPLE`, `BANANA`, `BURGER`, or `DONUT`), finite position, unit orientation and an explicit scale in [0.4,2.0]. `attached_player_id` must be zero; attachment is no longer supported. Velocity is not sent because the client only interpolates authoritative transforms.
- Unknown message type bytes, missing required semantic fields, invalid Protobuf data, oversized frames and unsupported versions close the connection.
- Unknown Protobuf fields are accepted within size/depth limits to allow additive schema changes. Duplicate singular fields follow Protobuf's last-value/merge semantics; the resulting message still passes all game validation. Valid unknown fields are not treated as forbidden trailing bytes. Removed field numbers must be reserved in future schemas and never reused.

The test suite constructs the maximum 32-player and 96-collectible, 6-mine snapshot with maximum IDs, defeat ticks, round ID, scores and 16-character nicknames, and requires it to remain within the 32KiB frame limit. Parsers enforce this limit before parsing and set a recursion limit of 16. Deserialization is not accepted merely because Protobuf parsing succeeds.

## Simulation and transport

Diagonal input is normalized. The character follows a target horizontal speed of
`6 / (radius / 0.5)^0.25` m/s with rapid braking, faces its movement direction and stays inside
a 64m square arena. Input expires after 120 ticks. Server dt remains 1/60.

Player fields 10–12 (`radius`, `food_eaten`, `mine_hit_tick`) require explicit presence.
Radius must be finite and in [0.5,3], food count in [0,4096], and hit tick no later than the snapshot.
Up to six mines carry unique nonzero IDs, finite positions and a reactivation tick at most
300 ticks in the future. Food disappears on consumption. Radius, consumption count, mine damage
and scoring are server authoritative. The client interpolates position, orientation and radius;
its continuously animated mouth is presentation only.

The protocol tests cover growth fields, missing/invalid values, mine cooldown limits, zero attachment
IDs, bounded maximum snapshots, and direct/serialized simulation equivalence. Local and TCP tests
exercise actual consumption, defeat and restart. Unknown old versions are rejected at Hello.

## Limits and shutdown

- Each room holds 3 occupied seats, counting players and reservations together. Default 8 rooms (24 players); --max-rooms accepts 1–256. Rooms are created on demand, fill oldest vacancies first, and expire after --empty-room-seconds (default 30, 0–86400).
- Each room starts with 64 foods, attempts one additional spawn every 30 active room ticks and caps active food at 96. The server or embedded client accepts `--world-seed` as an unsigned 64-bit deterministic seed.
- Core admission records and TCP connections each allow twice the configured player capacity, including pending handshakes. Further requests return UNAVAILABLE or close at the socket cap. FULL is decided by the tick thread after allocation fails.
- Server lifecycle queue: 1024 regular commands plus at most twice the player capacity in guaranteed leave commands. Movement inputs have a separate slot per admitted player (at most player capacity); flooding one slot cannot exhaust other slots.
- Per connection: at most 32 outbound frames; a slower peer is disconnected.
- Client submission queue: at most 64 inputs awaiting its network thread.
- Client connection and handshake deadline: 5 seconds. A complete inbound frame must arrive within 10 seconds.
- Disconnect, EOF, socket/protocol errors and timeouts close the session and enqueue player removal.
- ServerApplication ends control processing, drains the optional HTTP API, closes networking and drains its callbacks, then stops the physics core. See [server architecture](docs/SERVER_ARCHITECTURE.md).

This demonstration protocol has no encryption or account authentication. Tickets are single-use bearer credentials; the matching service authenticates users and distributes tickets. Protect ticket-bearing game traffic using a secure tunnel or add TLS before untrusted-network deployment. Keep the default loopback binding; explicit public binding is intended only for trusted test networks.

References: [Protobuf C++ tutorial](https://protobuf.dev/getting-started/cpptutorial/), [stream framing](https://protobuf.dev/programming-guides/techniques/#streaming-multiple-messages), [C++ compiler/runtime version matching](https://protobuf.dev/support/cross-version-runtime-guarantee/#cpp-and-rust-specific-guarantees).
