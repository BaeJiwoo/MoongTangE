namespace MoongTangE.MatchmakingApi.Models;

internal sealed record GameServerHeartbeat(
    string Host,
    ushort Port,
    uint ProtocolVersion,
    int Capacity,
    int Players,
    int Reserved,
    int Available,
    int ReservationSeconds);

internal sealed record ControlCommand(long Id, string Action, string Ticket);

internal sealed class ReservationRecord
{
    public required string Ticket { get; init; }
    public required string ServerId { get; init; }
    public required DateTimeOffset CreatedAt { get; init; }
    public DateTimeOffset ChangedAt { get; set; }
    public string Status { get; set; } = "pending";
    public ulong RoomId { get; set; }
    public long ExpiresInMs { get; set; }
}

internal sealed class GameServerRecord
{
    public required string Id { get; init; }
    public required GameServerHeartbeat Heartbeat { get; set; }
    public required DateTimeOffset LastSeen { get; set; }
    public List<ControlCommand> Commands { get; } = [];
    public HashSet<long> CompletedCommands { get; } = [];
}
