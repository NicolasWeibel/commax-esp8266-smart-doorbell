# Security Policy

## Supported versions

Security fixes are applied to the latest hardware-tested stage on the `main` branch.

## Reporting a vulnerability

Do not publish credentials, private topic names, signed image URLs, device identifiers, or exploit details in a public issue. Use a private GitHub security advisory when available, or contact the repository owner privately.

A useful report includes:

- affected version or commit;
- impact;
- reproduction steps without real credentials;
- relevant serial output after sanitization;
- a proposed mitigation, when known.

## Secrets that must never be committed

- `SmartDoorbell/Secrets.h`;
- Wi-Fi SSID and password;
- Imou App ID when treated as private configuration;
- Imou App Secret;
- Imou access tokens;
- private ntfy topic names;
- real device and channel identifiers;
- temporary signed snapshot URLs;
- private IP addresses or network topology;
- photographs from the installation;
- unsanitized serial logs or API responses.

`Secrets.example.h` must contain placeholders only.

## If a secret is committed

1. Revoke or rotate it immediately.
2. Remove it from the complete Git history, not only from the latest commit.
3. Check forks, releases, build artifacts, issues, and pull requests.
4. Review `.gitignore` and the publication workflow.
5. Assume the old credential is compromised even after history cleanup.

## Device security model

The planned firmware will make outbound HTTPS connections only. It should not expose a web server, open router ports, or disable TLS verification in production. Camera and notification failures must not disable local bell detection.

## Privacy

Doorbell events and entrance images may reveal presence, routines, and visitors. Keep topic identifiers private, minimize data retention, and do not publish real snapshots or event logs.
