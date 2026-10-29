using System.Text;
using Microsoft.AspNetCore.Mvc;
using MoongTangE.MatchmakingApi.Security;

namespace MoongTangE.MatchmakingApi.Controllers;

[ApiController]
[Route("game-servers/{serverId}")]
[ApiKeyAuthorization(ApiKeyScope.Control)]
public sealed class GameServersController(MatchmakingStore store) : ControllerBase
{
    [HttpPut("heartbeat")]
    public IActionResult Heartbeat(string serverId)
    {
        if (!ControlRequestReader.ValidServerId(serverId) ||
            !ControlRequestReader.TryHeartbeat(Request, out var heartbeat))
        {
            return new JsonResult("invalid heartbeat") { StatusCode = 400 };
        }

        var commands = store.Heartbeat(serverId, heartbeat!, DateTimeOffset.UtcNow);
        if (commands is null)
        {
            return StatusCode(StatusCodes.Status503ServiceUnavailable);
        }

        var body = string.Join('\n', commands.Select(command =>
            $"{command.Id}\t{command.Action}\t{command.Ticket}"));
        return Content(body, "text/plain", Encoding.UTF8);
    }

    [HttpPut("commands/{commandId:long}")]
    public async Task<IActionResult> ReportCommand(string serverId, long commandId)
    {
        var report = await ControlRequestReader.ReadReport(Request);
        return report is not null &&
               store.ReportCommand(serverId, commandId, report.Value.Status,
                   report.Value.RoomId, report.Value.ExpiresInMs, DateTimeOffset.UtcNow)
            ? NoContent()
            : BadRequest();
    }

    [HttpPut("reservations/{ticket}")]
    public async Task<IActionResult> ReportReservation(string serverId, string ticket)
    {
        var report = await ControlRequestReader.ReadReport(Request);
        return ControlRequestReader.ValidTicket(ticket) && report is not null &&
               store.ReportReservation(serverId, ticket, report.Value.Status,
                   report.Value.RoomId, report.Value.ExpiresInMs, DateTimeOffset.UtcNow)
            ? NoContent()
            : BadRequest();
    }
}
