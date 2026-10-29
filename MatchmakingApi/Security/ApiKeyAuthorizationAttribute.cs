using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Mvc.Filters;

namespace MoongTangE.MatchmakingApi.Security;

[AttributeUsage(AttributeTargets.Class | AttributeTargets.Method)]
public sealed class ApiKeyAuthorizationAttribute : TypeFilterAttribute
{
    public ApiKeyAuthorizationAttribute(ApiKeyScope scope) : base(typeof(ApiKeyAuthorizationFilter))
    {
        Arguments = [scope];
    }
}

internal sealed class ApiKeyAuthorizationFilter(ApiKeyAuthentication authentication, ApiKeyScope scope)
    : IAuthorizationFilter
{
    public void OnAuthorization(AuthorizationFilterContext context)
    {
        if (!authentication.Authorized(context.HttpContext.Request, scope))
        {
            context.Result = new JsonResult(new { error = "unauthorized" })
            {
                StatusCode = StatusCodes.Status401Unauthorized
            };
        }
    }
}
