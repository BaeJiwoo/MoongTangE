using System.Globalization;
using System.Text;
using MoongTangE.MatchmakingApi.Models;

namespace MoongTangE.MatchmakingApi;

internal static class ControlRequestReader
{
    internal static bool ValidTicket(string ticket) =>
        ticket.Length == 64 && ticket.All(value => value is >= '0' and <= '9' or >= 'a' and <= 'f');

    internal static bool ValidServerId(string serverId) =>
        serverId.Length is >= 1 and <= 64 &&
        serverId.All(value => char.IsAsciiLetterOrDigit(value) || value is '-' or '_');

    internal static bool TryHeartbeat(HttpRequest request, out GameServerHeartbeat? heartbeat)
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

    internal static async Task<(string Status, ulong RoomId, long ExpiresInMs)?> ReadReport(HttpRequest request)
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
}
