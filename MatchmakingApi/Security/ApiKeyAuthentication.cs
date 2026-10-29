using System.Security.Cryptography;
using System.Text;

namespace MoongTangE.MatchmakingApi.Security;

public enum ApiKeyScope
{
    Public,
    Control
}

public sealed class ApiKeyAuthentication
{
    private readonly byte[] publicAuthorization = RequiredAuthorization("MOONGTANGE_PUBLIC_API_KEY");
    private readonly byte[] controlAuthorization = RequiredAuthorization("MOONGTANGE_CONTROL_KEY");

    internal bool Authorized(HttpRequest request, ApiKeyScope scope)
    {
        var expected = scope == ApiKeyScope.Public ? publicAuthorization : controlAuthorization;
        var supplied = Encoding.ASCII.GetBytes(request.Headers.Authorization.ToString());
        return supplied.Length == expected.Length &&
               CryptographicOperations.FixedTimeEquals(supplied, expected);
    }

    private static byte[] RequiredAuthorization(string name)
    {
        var secret = Environment.GetEnvironmentVariable(name);
        if (secret is null || secret.Length is < 32 or > 128 ||
            secret.Any(value => value is < '!' or > '~'))
        {
            throw new InvalidOperationException($"{name} must contain 32..128 printable ASCII characters");
        }
        return Encoding.ASCII.GetBytes("Bearer " + secret);
    }
}
