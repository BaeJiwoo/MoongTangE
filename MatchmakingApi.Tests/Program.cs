using MoongTangE.MatchmakingApi.Models;
using MoongTangE.MatchmakingApi;

var now = DateTimeOffset.UtcNow;
var store = new MatchmakingStore();
var heartbeat = new GameServerHeartbeat("game.example", 27015, 6, 3, 0, 0, 3, 30);

Check(store.Heartbeat("game-1", heartbeat, now) is { Count: 0 }, "registration failed");
var ticket = new string('a', 64);
var reservation = store.Reserve(ticket, now.AddMilliseconds(1));
Check(reservation is not null && reservation.Status == "pending", "reservation was not queued");
Check(ReferenceEquals(reservation, store.Reserve(ticket, now.AddMilliseconds(2))),
    "reservation is not idempotent");

var commands = store.Heartbeat("game-1", heartbeat, now.AddMilliseconds(3));
Check(commands is { Count: 1 } && commands[0].Action == "reserve" &&
    commands[0].Ticket == ticket, "reserve command was not delivered");
Check(store.ReportCommand("game-1", commands![0].Id, "reserved", 1, 29000,
    now.AddMilliseconds(4)), "reserve result was rejected");
Check(store.Lookup(ticket, now.AddMilliseconds(5))?.Status == "reserved",
    "reserved status was not published");

Check(store.Cancel(ticket, now.AddMilliseconds(6))?.Status == "cancelling",
    "cancellation was not queued");
commands = store.Heartbeat("game-1",
    heartbeat with { Reserved = 1, Available = 2 }, now.AddMilliseconds(7));
var cancel = commands?.SingleOrDefault(command => command.Action == "cancel");
Check(cancel is not null, "cancel command was not delivered");
Check(store.ReportCommand("game-1", cancel!.Id, "cancelled", 1, 0,
    now.AddMilliseconds(8)), "cancel result was rejected");
Check(store.Lookup(ticket, now.AddMilliseconds(9))?.Status == "cancelled",
    "cancelled status was not published");

var unavailableTicket = new string('b', 64);
Check(store.Reserve(unavailableTicket, now.AddMilliseconds(10)) is not null,
    "second reservation failed");
Check(store.Lookup(unavailableTicket, now.AddSeconds(4))?.Status == "unavailable",
    "stale game server was not removed from assignment");

Console.WriteLine("MatchmakingStore registration, idempotency, results, cancellation and timeout passed");

static void Check(bool condition, string message)
{
    if (!condition)
    {
        throw new InvalidOperationException(message);
    }
}
