using Microsoft.AspNetCore.Mvc;
using MoongTangE.MatchmakingApi.Models;
using MoongTangE.MatchmakingApi.Security;

namespace MoongTangE.MatchmakingApi.Controllers;

[ApiController]
[Route("matchmaking")]
[ApiKeyAuthorization(ApiKeyScope.Public)]
public sealed class MatchmakingController(MatchmakingStore store) : ControllerBase
{
    [HttpGet("status")]
    public IActionResult GetStatus() => new JsonResult(store.Status(DateTimeOffset.UtcNow));

    [HttpGet("reservations")]
    public IActionResult GetReservation() => HandleReservation(store.Lookup, false);

    [HttpPut("reservations")]
    public IActionResult Reserve() => HandleReservation(store.Reserve, true);

    [HttpDelete("reservations")]
    public IActionResult Cancel() => HandleReservation(store.Cancel, false);

    private IActionResult HandleReservation(
        Func<string, DateTimeOffset, ReservationRecord?> operation, bool creating)
    {
        var ticket = Request.Headers["X-Admission-Ticket"].ToString();
        if (!ControlRequestReader.ValidTicket(ticket))
        {
            return new JsonResult(new { error = "invalid_ticket" }) { StatusCode = 400 };
        }

        var reservation = operation(ticket, DateTimeOffset.UtcNow);
        if (reservation is null)
        {
            return new JsonResult(new { error = creating ? "no_game_server" : "unknown_ticket" })
            {
                StatusCode = creating ? 503 : 404
            };
        }

        var status = reservation.Status switch
        {
            "pending" or "cancelling" => 202,
            "full" => 409,
            "expired" or "cancelled" => 410,
            "unavailable" => 503,
            _ => 200
        };
        return new JsonResult(store.PublicReservation(reservation)) { StatusCode = status };
    }
}
