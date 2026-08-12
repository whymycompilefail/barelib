# Security Policy

## Supported Versions

barelib is developed on a single active branch; the latest release is the only
supported version. Older tagged releases receive security fixes only on request.

| Version        | Supported          |
| -------------- | ------------------ |
| latest release | :white_check_mark: |
| older releases | :x: (fix on request) |

## Reporting a Vulnerability

Please report vulnerabilities privately via the repository Issues with a
`[security]` prefix, or contact the maintainer directly. Do **not** open a
public issue that exposes exploit details before a fix is available.

What to include:

- Affected module (rcore / rlgl / rshapes / rtext / raudio / AI adapter)
- A minimal reproduction (ideally a small `examples/`-style program)
- Impact and any suggested fix, if you have one

We aim to acknowledge reports within 7 days and to ship a fix in the next
release cycle. We will credit reporters in the release notes unless anonymity
is requested.

## Security notes for this project

- **AI protocol input surface**: the AI adapter reads commands from stdin
  (`AIStartCommandLoop`) and injects real input (`SendInput` on Windows).
  Treat any stdin source as untrusted: a game embedding the adapter should gate
  command input to its own trusted channel (local pipe/file), not network-expose
  it. See `docs/ai_adapter.md`.
- **Dependencies**: the engine is dependency-free at build time (no external
  libraries); miniaudio is bundled and follows upstream security updates.
- **Path handling**: no hard-coded resource paths; file APIs take caller
  arguments and do not assume a working directory.
