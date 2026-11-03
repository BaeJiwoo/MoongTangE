using System.Security.Cryptography;
using System.Text;

namespace MoongTangE.MatchmakingApi;

internal sealed record ClientReply(int Code, object Body);
internal sealed record GuestSessionResponse(string guest_id, string access_token, long expires_in_ms);
internal sealed record ClientReservationResponse(
    string status, string? game_host = null, int? game_port = null,
    uint? protocol_version = null, string? room_id = null, long? expires_in_ms = null,
    string? admission_ticket = null);

// All guest state is bounded and guarded by this lock. Lock order: guest -> matchmaking.
// Tokens are returned once and only their SHA-256 hashes are retained.
public sealed class GuestSessionStore(MatchmakingStore matchmaking)
{
    internal const int MaxSessions = 4096;
    internal const int MaxAttempts = 64;
    internal static readonly TimeSpan SessionLifetime = TimeSpan.FromMinutes(30);
    private readonly object sync = new();
    private readonly Dictionary<string, Session> sessions = [];

    private sealed class Session(DateTimeOffset now)
    {
        internal DateTimeOffset ExpiresAt { get; } = now + SessionLifetime;
        internal Dictionary<string, Attempt> Attempts { get; } = [];
        internal string? ActiveAttempt { get; set; }
        internal DateTimeOffset WindowStart { get; set; } = now;
        internal int Requests { get; set; }
    }

    private sealed class Attempt(string ticket)
    {
        internal string Ticket { get; } = ticket;
        internal bool Cancelling { get; set; }
        internal ClientReply? Terminal { get; set; }
    }

    internal ClientReply Create(DateTimeOffset now)
    {
        lock (sync)
        {
            Prune(now);
            if (sessions.Count >= MaxSessions) return Error(503, "session_capacity");
            var token = NewSecret();
            sessions.Add(Hash(token), new Session(now));
            return new(201, new GuestSessionResponse(Guid.NewGuid().ToString("N"), token,
                (long)SessionLifetime.TotalMilliseconds));
        }
    }

    internal ClientReply Handle(string token, string attemptId, string operation, DateTimeOffset now)
    {
        lock (sync)
        {
            Prune(now);
            if (!ControlRequestReader.ValidTicket(token) ||
                !sessions.TryGetValue(Hash(token), out var session)) return Error(401, "unauthorized");
            if (attemptId.Length != 32 || !attemptId.All(char.IsAsciiHexDigitLower))
                return Error(400, "invalid_attempt_id");
            if (now - session.WindowStart >= TimeSpan.FromSeconds(1))
            {
                session.WindowStart = now;
                session.Requests = 0;
            }
            if (++session.Requests > 16) return Error(429, "rate_limited");

            if (!session.Attempts.TryGetValue(attemptId, out var attempt))
            {
                if (operation != "create") return Error(404, "unknown_attempt");
                if (session.ActiveAttempt is { } activeId &&
                    Read(session.Attempts[activeId], now).Code is 200 or 202)
                    return Error(409, "active_reservation");
                if (session.Attempts.Count >= MaxAttempts) return Error(429, "attempt_capacity");
                // Keep the idempotency record for the entire session, including failures.
                attempt = new Attempt(NewSecret());
                session.Attempts.Add(attemptId, attempt);
                if (matchmaking.Reserve(attempt.Ticket, now) is null)
                    attempt.Terminal = Error(503, "no_game_server");
                session.ActiveAttempt = attemptId;
            }
            if (operation == "cancel" && attempt.Terminal is null)
            {
                attempt.Cancelling = true;
                matchmaking.Cancel(attempt.Ticket, now);
            }
            return Read(attempt, now);
        }
    }

    private ClientReply Read(Attempt attempt, DateTimeOffset now)
    {
        if (attempt.Terminal is not null) return attempt.Terminal;
        var reply = matchmaking.ClientReservation(attempt.Ticket, now);
        // A cancellation racing a reserve report must never expose a usable ticket.
        if (attempt.Cancelling && reply.Code is 200 or 202)
            reply = new(202, new ClientReservationResponse("cancelling"));
        if (reply.Code != 202 && reply.Body is not ClientReservationResponse { status: "reserved" })
            attempt.Terminal = reply;
        return reply;
    }

    private void Prune(DateTimeOffset now)
    {
        foreach (var key in sessions.Where(pair => pair.Value.ExpiresAt <= now)
                     .Select(pair => pair.Key).ToArray())
        {
            foreach (var attempt in sessions[key].Attempts.Values.Where(value => value.Terminal is null))
                matchmaking.Cancel(attempt.Ticket, now);
            sessions.Remove(key);
        }
    }

    private static string NewSecret() => Convert.ToHexString(RandomNumberGenerator.GetBytes(32)).ToLowerInvariant();
    private static string Hash(string token) => Convert.ToHexString(SHA256.HashData(Encoding.ASCII.GetBytes(token)));
    internal static ClientReply Error(int code, string error) => new(code, new { error });
}
