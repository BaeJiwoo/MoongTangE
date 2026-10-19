using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using MoongTangE.MatchmakingApi;

var publicKey = RequiredSecret("MOONGTANGE_PUBLIC_API_KEY");
var controlKey = RequiredSecret("MOONGTANGE_CONTROL_KEY");
var builder = WebApplication.CreateBuilder(args);
builder.WebHost.ConfigureKestrel(options => options.Limits.MaxRequestBodySize = 1024);
builder.Services.AddSingleton<MatchmakingStore>();
var app = builder.Build();

app.Use(async (context, next) =>
{
    context.Response.Headers.CacheControl = "no-store";
    await next(context);
});

app.MapGet("/health", () => Results.Json(new { status = "ok" }));

app.MapGet("/matchmaking/status", (HttpRequest request, MatchmakingStore store) =>
    Authorized(request, publicKey)
        ? Results.Json(store.Status(DateTimeOffset.UtcNow))
        : Unauthorized());

app.MapMethods("/matchmaking/reservations", ["GET", "PUT", "DELETE"],
    (HttpRequest request, MatchmakingStore store) =>
    {
        if (!Authorized(request, publicKey))
        {
            return Unauthorized();
        }
        var ticket = request.Headers["X-Admission-Ticket"].ToString();
        if (!ValidTicket(ticket))
        {
            return Results.Json(new { error = "invalid_ticket" }, statusCode: 400);
        }

        var now = DateTimeOffset.UtcNow;
        ReservationRecord? reservation = request.Method switch
        {
            "PUT" => store.Reserve(ticket, now),
            "DELETE" => store.Cancel(ticket, now),
            _ => store.Lookup(ticket, now)
        };
        if (reservation is null)
        {
            var status = request.Method == "PUT" ? 503 : 404;
            var error = request.Method == "PUT" ? "no_game_server" : "unknown_ticket";
            return Results.Json(new { error }, statusCode: status);
        }
        return ReservationResult(store, reservation);
    });

app.MapPut("/game-servers/{serverId}/heartbeat",
    (string serverId, HttpRequest request, MatchmakingStore store) =>
    {
        if (!Authorized(request, controlKey) || !ValidServerId(serverId) ||
            !TryHeartbeat(request, out var heartbeat))
        {
            return Authorized(request, controlKey)
                ? Results.BadRequest("invalid heartbeat")
                : Unauthorized();
        }
        var commands = store.Heartbeat(serverId, heartbeat!, DateTimeOffset.UtcNow);
        if (commands is null)
        {
            return Results.StatusCode(StatusCodes.Status503ServiceUnavailable);
        }
        var body = string.Join('\n', commands.Select(command =>
            $"{command.Id}\t{command.Action}\t{command.Ticket}"));
        return Results.Text(body, "text/plain", Encoding.UTF8);
    });

app.MapPut("/game-servers/{serverId}/commands/{commandId:long}",
    async (string serverId, long commandId, HttpRequest request, MatchmakingStore store) =>
    {
        if (!Authorized(request, controlKey))
        {
            return Unauthorized();
        }
        var report = await ReadReport(request);
        return report is not null &&
               store.ReportCommand(serverId, commandId, report.Value.Status,
                   report.Value.RoomId, report.Value.ExpiresInMs, DateTimeOffset.UtcNow)
            ? Results.NoContent()
            : Results.BadRequest();
    });

app.MapPut("/game-servers/{serverId}/reservations/{ticket}",
    async (string serverId, string ticket, HttpRequest request, MatchmakingStore store) =>
    {
        if (!Authorized(request, controlKey))
        {
            return Unauthorized();
        }
        var report = await ReadReport(request);
        return ValidTicket(ticket) && report is not null &&
               store.ReportReservation(serverId, ticket, report.Value.Status,
                   report.Value.RoomId, report.Value.ExpiresInMs, DateTimeOffset.UtcNow)
            ? Results.NoContent()
            : Results.BadRequest();
    });

app.Run();

static string RequiredSecret(string name)
{
    var secret = Environment.GetEnvironmentVariable(name);
    if (secret is null || secret.Length is < 32 or > 128 || secret.Any(value => value is < '!' or > '~'))
    {
        throw new InvalidOperationException($"{name} must contain 32..128 printable ASCII characters");
    }
    return secret;
}

static bool Authorized(HttpRequest request, string secret)
{
    var expected = Encoding.ASCII.GetBytes("Bearer " + secret);
    var supplied = Encoding.ASCII.GetBytes(request.Headers.Authorization.ToString());
    return supplied.Length == expected.Length && CryptographicOperations.FixedTimeEquals(supplied, expected);
}

static IResult Unauthorized() =>
    Results.Json(new { error = "unauthorized" }, statusCode: StatusCodes.Status401Unauthorized);

static bool ValidTicket(string ticket) =>
    ticket.Length == 64 && ticket.All(value => value is >= '0' and <= '9' or >= 'a' and <= 'f');

static bool ValidServerId(string serverId) =>
    serverId.Length is >= 1 and <= 64 &&
    serverId.All(value => char.IsAsciiLetterOrDigit(value) || value is '-' or '_');

static bool TryHeartbeat(HttpRequest request, out GameServerHeartbeat? heartbeat)
{
    heartbeat = null;
    var host = request.Headers["X-Game-Host"].ToString();
    if (host.Length is < 1 or > 253 || host.Any(char.IsWhiteSpace) ||
        !ushort.TryParse(request.Headers["X-Game-Port"], out var port) || port == 0 ||
        !uint.TryParse(request.Headers["X-Protocol-Version"], out var protocol) || protocol == 0 ||
        !int.TryParse(request.Headers["X-Capacity"], out var capacity) || capacity < 1 ||
        !int.TryParse(request.Headers["X-Players"], out var players) || players < 0 ||
        !int.TryParse(request.Headers["X-Reserved"], out var reserved) || reserved < 0 ||
        !int.TryParse(request.Headers["X-Available"], out var available) || available < 0 ||
        !int.TryParse(request.Headers["X-Reservation-Seconds"], out var reservationSeconds) ||
        reservationSeconds is < 1 or > 300 || players + reserved + available != capacity)
    {
        return false;
    }
    heartbeat = new GameServerHeartbeat(host, port, protocol, capacity, players, reserved, available,
        reservationSeconds);
    return true;
}

static async Task<(string Status, ulong RoomId, long ExpiresInMs)?> ReadReport(HttpRequest request)
{
    using var reader = new StreamReader(request.Body, Encoding.UTF8, false, 1024, leaveOpen: true);
    var body = await reader.ReadToEndAsync();
    var fields = body.Split('\t');
    if (fields.Length != 3 ||
        fields[0] is not ("reserved" or "consumed" or "full" or "expired" or "cancelled" or "unavailable") ||
        !ulong.TryParse(fields[1], CultureInfo.InvariantCulture, out var roomId) ||
        !long.TryParse(fields[2], CultureInfo.InvariantCulture, out var expiresInMs) || expiresInMs < 0)
    {
        return null;
    }
    return (fields[0], roomId, expiresInMs);
}

static IResult ReservationResult(MatchmakingStore store, ReservationRecord reservation)
{
    var status = reservation.Status switch
    {
        "pending" or "cancelling" => 202,
        "full" => 409,
        "expired" or "cancelled" => 410,
        "unavailable" => 503,
        _ => 200
    };
    return Results.Json(store.PublicReservation(reservation), statusCode: status);
}

public partial class Program;
