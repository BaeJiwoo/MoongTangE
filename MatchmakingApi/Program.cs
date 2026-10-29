using Microsoft.AspNetCore.Mvc.ModelBinding;
using MoongTangE.MatchmakingApi;
using MoongTangE.MatchmakingApi.Security;

var authentication = new ApiKeyAuthentication();
var builder = WebApplication.CreateBuilder(args);
builder.WebHost.ConfigureKestrel(options => options.Limits.MaxRequestBodySize = 1024);
builder.Services.AddSingleton(authentication);
builder.Services.AddSingleton<MatchmakingStore>();
builder.Services.AddControllers(options =>
{
    // Control reports are raw TSV regardless of Content-Type, never HTML form fields.
    options.ValueProviderFactories.RemoveType<FormValueProviderFactory>();
    options.ValueProviderFactories.RemoveType<JQueryFormValueProviderFactory>();
    options.ValueProviderFactories.RemoveType<FormFileValueProviderFactory>();
}).ConfigureApiBehaviorOptions(options =>
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

app.MapControllers();
app.Run();

public partial class Program;
