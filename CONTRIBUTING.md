# Contributing

Thanks for your interest. Bug reports, fixes and features are welcome.

## Build and test

```bash
cmake -S . -B build -DADSB_WARNINGS_AS_ERRORS=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Before opening a pull request also run the sanitizer build:

```bash
cmake -S . -B build-san -DADSB_SANITIZE=ON -DADSB_WARNINGS_AS_ERRORS=ON
cmake --build build-san -j && ctest --test-dir build-san --output-on-failure
```

## Static analysis

```bash
cmake -S . -B build-tidy -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
clang-tidy -p build-tidy src/*.cpp apps/*.cpp
cppcheck --enable=warning,style,performance,portability --error-exitcode=1 \
  --suppressions-list=cppcheck-suppressions.txt -Iinclude --std=c++17 --quiet src apps tests
```

## Code style

- C++17, RAII, no raw `new`/`delete`, `const` wherever possible, `[[nodiscard]]` on queries, `enum class`.
- Zero warnings with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion`.
- No TODOs, stubs or dead code. New behaviour needs a unit test in `tests/`.
- Hardware-dependent behaviour must keep a simulation or replay path so tests run without a dongle.

## Commits and pull requests

- Small, logical commits with plain imperative subjects ("Add X", "Fix Y"), no trailing period.
- Describe what changed and why in the pull request; link the issue it closes.
- Add an entry under `[Unreleased]` in `CHANGELOG.md` for user-visible changes.
