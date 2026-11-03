using Microsoft.AspNetCore.Mvc.ModelBinding;
using System.Threading.RateLimiting;
using MoongTangE.MatchmakingApi;
using MoongTangE.MatchmakingApi.Security;

var authentication = new ApiKeyAuthentication();
var builder = WebApplication.CreateBuilder(args);
builder.WebHost.ConfigureKestrel(options => options.Limits.MaxRequestBodySize = 1024);
builder.Services.AddSingleton(authentication);
builder.Services.AddSingleton<MatchmakingStore>();
builder.Services.AddSingleton<GuestSessionStore>();
builder.Services.AddRateLimiter(options =>
{
    options.RejectionStatusCode = 429;
    // A fixed global bucket avoids an unbounded dictionary keyed by remote addresses.
    options.AddPolicy("guest_creation", _ => RateLimitPartition.GetFixedWindowLimiter("guests",
        _ => new FixedWindowRateLimiterOptions
        {
            PermitLimit = 120, Window = TimeSpan.FromMinutes(1), QueueLimit = 0
        }));
    options.OnRejected = (context, _) =>
    {
        context.HttpContext.Response.Headers.RetryAfter = "60";
        return ValueTask.CompletedTask;
    };
});
builder.Services.AddControllers(options =>
{
    // Control reports are raw TSV regardless of Content-Type, never HTML form fields.
    options.ValueProviderFactories.RemoveType<FormValueProviderFactory>();
    options.ValueProviderFactories.RemoveType<JQueryFormValueProviderFactory>();
    options.ValueProviderFactories.RemoveType<FormFileValueProviderFactory>();
}).AddJsonOptions(options => options.JsonSerializerOptions.DefaultIgnoreCondition =
    System.Text.Json.Serialization.JsonIgnoreCondition.WhenWritingNull)
.ConfigureApiBehaviorOptions(options =>
{
    // Keep the control protocol's empty 400/503 responses instead of ProblemDetails.
    options.SuppressMapClientErrors = true;
});
var app = builder.Build();

app.Use(async (context, next) =>
{
    context.Response.Headers.CacheControl = "no-store";
    await next(context);
});

app.UseRateLimiter();
app.MapControllers();
app.Run();

public partial class Program;
