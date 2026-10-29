using Microsoft.AspNetCore.Mvc;

namespace MoongTangE.MatchmakingApi.Controllers;

[ApiController]
[Route("health")]
public sealed class HealthController : ControllerBase
{
    [HttpGet]
    public IActionResult Get() => new JsonResult(new { status = "ok" });
}
