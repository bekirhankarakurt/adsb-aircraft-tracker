# Security Policy

## Supported versions

| Version | Supported |
|---|---|
| 1.x | yes |
| < 1.0 | no |

## Reporting a vulnerability

Please do not open a public issue for security problems. Report them privately through
[GitHub Security Advisories](https://github.com/bekirhankarakurt/adsb-aircraft-tracker/security/advisories/new).

Include the affected version, a description of the issue and, if possible, a minimal input
(IQ capture or hex line) that reproduces it. You can expect an initial response within a week.

The tool parses untrusted radio data and file input, so crashes, out-of-bounds accesses and
undefined behaviour on malformed frames are treated as security-relevant.
