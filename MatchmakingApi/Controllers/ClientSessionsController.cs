using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.RateLimiting;

namespace MoongTangE.MatchmakingApi.Controllers;

[ApiController]
[Route("client")]
public sealed class ClientSessionsController(GuestSessionStore sessions) : ControllerBase
{
    [HttpPost("sessions")]
    [EnableRateLimiting("guest_creation")]
    public IActionResult Create() => Result(sessions.Create(DateTimeOffset.UtcNow));

    [HttpPost("matchmaking/reservations")]
    public IActionResult Reserve() => Handle(Request.Headers["Idempotency-Key"].ToString(), "create");

    [HttpGet("matchmaking/reservations/{attemptId}")]
    public IActionResult Get(string attemptId) => Handle(attemptId, "get");

    [HttpDelete("matchmaking/reservations/{attemptId}")]
    public IActionResult Cancel(string attemptId) => Handle(attemptId, "cancel");

    private IActionResult Handle(string attemptId, string operation)
    {
        var authorization = Request.Headers.Authorization.ToString();
        var token = authorization.StartsWith("Bearer ", StringComparison.Ordinal)
            ? authorization[7..] : "";
        return Result(sessions.Handle(token, attemptId, operation, DateTimeOffset.UtcNow));
    }

    private JsonResult Result(ClientReply reply)
    {
        if (reply.Code == 429) Response.Headers.RetryAfter = "1";
        return new(reply.Body) { StatusCode = reply.Code };
    }
}
