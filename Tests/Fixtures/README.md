These public test-only PEM files exercise HTTPS certificate and hostname validation on loopback.
The private key is intentionally published, protects no service or user data, and must never be
used by a deployment. The certificates name only `localhost` or `127.0.0.1`. Tests trust them explicitly for the
successful case, then verify rejection without that trust and with the wrong hostname.
