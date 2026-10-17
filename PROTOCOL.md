# TCP protocol v1

Each frame is a 4-byte unsigned big-endian payload length followed by exactly that many bytes. Valid payload lengths are 1–4096. Asio composed reads/writes handle partial transfers. Every integer uses network byte order; real numbers are finite IEEE-754 binary64 encoded as 8 big-endian bytes. C++ object memory is never transmitted.

| Type byte | Message | Payload after type |
| --- | --- | --- |
| 1 | ClientHello | version: u16 (=1) |
| 2 | ServerHello | version: u16, assigned player ID: u32 |
| 3 | InputCommand | sequence: u64, x: f64, z: f64 |
| 4 | WorldSnapshot | server tick: u64, player count: u16, repeated player records |
| 5 | Disconnect | empty |

A player record contains ID (u32), position x/y/z (3×f64), and velocity x/y/z (3×f64). At most 32 records are allowed; IDs must be nonzero and unique. The first client message must be ClientHello. The server replies with ServerHello before sending snapshots. Unknown types, versions, truncation, trailing bytes and non-finite values are rejected.

Input IDs are taken from the authenticated protocol session assignment (the Hello handshake is **not user authentication**). A packet cannot choose another player's ID. Sequences start above zero and increase strictly within a connection; x/z must be in [-1,1]. Diagonal inputs are normalized on the server. If no new input reaches the world for 120 simulation ticks, movement stops.

The server publishes its latest snapshot approximately every 50 ms. GameWorld always advances with dt=1/60; slow scheduling can skip wall-clock catch-up after eight periods, but never changes physics dt. TCP arrival time determines the tick that consumes an input. Cross-platform bit-identical floating-point results, client prediction and lag compensation are not promised.

## Resource limits and shutdown

- Maximum 32 joined players and 64 TCP connections including incomplete handshakes.
- Server command queue: 1024 regular commands plus at most 32 guaranteed leave commands.
- Per connection: at most 32 queued outbound frames; a slower peer is disconnected.
- Client submission queue: at most 64 inputs awaiting its network thread.
- Client connection and handshake deadline: 5 seconds. Each complete inbound TCP frame must arrive within 10 seconds.
- Peers may send Disconnect or close TCP. EOF, socket errors, protocol errors and timeouts close the session and enqueue player removal.
- Closing a server cancels accept, timers and sockets, drains cancellation callbacks and joins its networking thread. ServerCore is stopped separately after transports are destroyed.

This is an unencrypted, unauthenticated demonstration protocol. Keep the default loopback binding. The dedicated server's explicit public option is suitable only for trusted test networks; it is not an internet-facing production service.
