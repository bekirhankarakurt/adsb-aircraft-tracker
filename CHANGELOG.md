# Changelog

All notable changes to this project are documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.0.0] - 2026-11-04

### Added
- Streaming 2 Msps PPM demodulator for 8-bit RTL-SDR IQ (files and stdin pipes).
- CRC-24 verification with single-bit error correction.
- DF17 decoding: identification, airborne position, ground speed and airspeed, vertical rate.
- CPR global (even/odd) and local (reference position) position decoding.
- Aircraft tracker with terminal table, JSON snapshot and self-contained HTML map output.
- Hex input in `*HEX;`, `@TIMESTAMPHEX;` and bare formats.
- `adsb-sim` scenario generator and bundled sample captures for hardware-free use.
- Unit, end-to-end CLI tests and CI on GCC, Clang and ASan/UBSan.
- Static analysis (cppcheck, clang-tidy), CodeQL, coverage report and Doxygen API docs.

[Unreleased]: https://github.com/bekirhankarakurt/adsb-aircraft-tracker/compare/v1.0.0...HEAD
[1.0.0]: https://github.com/bekirhankarakurt/adsb-aircraft-tracker/releases/tag/v1.0.0
