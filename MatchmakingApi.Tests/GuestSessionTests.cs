using MoongTangE.MatchmakingApi;
using MoongTangE.MatchmakingApi.Models;

internal static class GuestSessionTests
{
    internal static void Run()
    {
        var now = DateTimeOffset.UtcNow;
        var store = new MatchmakingStore();
        var sessions = new GuestSessionStore(store);
        var heartbeat = new GameServerHeartbeat("127.0.0.1", 27015, 8, 10, 0, 0, 10, 30);
        store.Heartbeat("game", heartbeat, now);
        var owner = (GuestSessionResponse)sessions.Create(now).Body;
        var other = (GuestSessionResponse)sessions.Create(now).Body;
        var attempt = Guid.NewGuid().ToString("N");
        Check(owner.access_token.Length == 64 && owner.access_token != other.access_token, "random tokens");
        Check(sessions.Handle("invalid", attempt, "create", now).Code == 401, "authentication");
        Check(sessions.Handle(owner.access_token, "bad", "create", now).Code == 400, "attempt validation");
        Parallel.For(0, 8, _ => Check(sessions.Handle(owner.access_token, attempt, "create", now).Code == 202,
            "concurrent idempotency"));
        Check(sessions.Handle(other.access_token, attempt, "get", now).Code == 404, "owner isolation");
        Check(sessions.Handle(other.access_token, attempt, "cancel", now).Code == 404, "cancel isolation");
        Check(sessions.Handle(owner.access_token, Guid.NewGuid().ToString("N"), "create", now).Code == 409,
            "single active attempt");
        var commands = store.Heartbeat("game", heartbeat, now)!;
        Check(commands.Count == 1, "only one reserve command");
        var command = commands.Single();
        store.ReportCommand("game", command.Id, "reserved", 7, 29000, now);
        var reserved = (ClientReservationResponse)sessions.Handle(owner.access_token, attempt, "get", now).Body;
        Check(reserved.admission_ticket == command.Ticket && reserved.room_id == "7", "assignment");
        var retry = (ClientReservationResponse)sessions.Handle(owner.access_token, attempt, "create", now).Body;
        Check(retry.admission_ticket == reserved.admission_ticket, "response-loss retry");
        var cancelling = (ClientReservationResponse)sessions.Handle(owner.access_token, attempt, "cancel", now).Body;
        Check(cancelling.admission_ticket is null, "no ticket after cancellation");
        store.ReportReservation("game", command.Ticket, "reserved", 7, 29000, now);
        Check(((ClientReservationResponse)sessions.Handle(owner.access_token, attempt, "get", now).Body)
            .admission_ticket is null, "late reserve must not revive cancelled assignment");
        store.ReportReservation("game", command.Ticket, "consumed", 7, 0, now);
        Check(sessions.Handle(owner.access_token, attempt, "get", now.AddSeconds(1)).Code == 410,
            "consumed ticket cannot be reused");
        Check(sessions.Handle(owner.access_token, attempt, "create", now.AddSeconds(1)).Code == 410,
            "terminal attempt cannot be recreated");

        var next = Guid.NewGuid().ToString("N");
        Check(sessions.Handle(other.access_token, next, "create", now.AddSeconds(1)).Code == 202, "new attempt");
        Check(sessions.Handle(other.access_token, next, "get", now + GuestSessionStore.SessionLifetime).Code == 401,
            "session expiry");
        Check(store.Heartbeat("game", heartbeat, now + GuestSessionStore.SessionLifetime)!
            .Any(value => value.Action == "cancel"), "expiry cancels reservations");

        var emptyStore = new GuestSessionStore(new MatchmakingStore());
        var emptyGuest = (GuestSessionResponse)emptyStore.Create(now).Body;
        Check(emptyStore.Handle(emptyGuest.access_token, attempt, "create", now).Code == 503, "no server");
        Check(emptyStore.Handle(emptyGuest.access_token, attempt, "get", now).Code == 503, "failure is idempotent");
        for (var index = 0; index < 16; ++index) emptyStore.Handle(emptyGuest.access_token, attempt, "get", now);
        Check(emptyStore.Handle(emptyGuest.access_token, attempt, "get", now).Code == 429, "bounded session rate");
        Check(emptyStore.Handle(emptyGuest.access_token, attempt, "get", now.AddSeconds(1)).Code == 503,
            "rate window recovers");
        for (var index = 1; index < GuestSessionStore.MaxSessions; ++index) emptyStore.Create(now);
        Check(emptyStore.Create(now).Code == 503, "bounded session count");
        Check(emptyStore.Create(now + GuestSessionStore.SessionLifetime).Code == 201, "expired sessions reclaimed");
        Console.WriteLine("Guest sessions: ownership, concurrency, replay, cancellation, expiry, capacity and rate passed");
    }

    private static void Check(bool condition, string message)
    {
        if (!condition) throw new InvalidOperationException(message);
    }
}
