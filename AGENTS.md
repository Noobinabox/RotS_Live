# Repository Guidelines

## Project Structure & Module Organization
- src/: C/C++ game server sources, headers, and build scripts (`Makefile`, `CMakeLists.txt`).
- bin/: Built server binary (`ageland`) and backup (`ageland~`).
- lib/: Runtime data (players, world, text, etc.); many subpaths are git-ignored.
- build/: CMake build artifacts and test scaffolding; not checked in.
- proxy/: Rust workspace member (`cargo` crate) for proxy/CLI utilities.
- release-notes/, game design docs/, code documentation/: Docs and release history.

## Build, Test, and Development Commands
- Bootstrap data: `cd src && make setup` — creates required runtime directories/files under `lib/`, `log/`, and `bin/`.
- Build (Make): `cd src && make all` — compiles C/C++ sources to `bin/ageland`.
- Run: `cd src && make run` or `./bin/ageland -p &` — starts server in background.
- Clean: `cd src && make clean` — removes `*.o` objects.
- CMake alt build: `cmake -S src -B build && cmake --build build` (C++17).
- Rust proxy: `cargo build -p proxy` | `cargo test -p proxy` | `cargo run -p proxy -- --help`.

## Coding Style & Naming Conventions
- Formatter: run `cd src && make format` (WebKit style). Prefer this over local defaults; CI expects formatted diffs.
- .clang-format: present for IDEs; indentation 4 spaces; column limit ~100.
- Filenames: lower_snake_case for `.cpp`/`.h` (e.g., `act_comm.cpp`, `protocol.h`).
- C/C++: functions/variables lower_snake_case; constants UPPER_SNAKE_CASE; types TitleCase where applicable.
- Rust (proxy): follow `rustfmt` defaults; module/file lowercase with underscores.

## Testing Guidelines
- C/C++: no formal unit tests; perform smoke tests by building and running locally. Verify server boots, accepts connections, and changed features behave as expected.
- Rust: write unit/integration tests in `proxy/`; run with `cargo test -p proxy` and keep coverage reasonable.

## Commit & Pull Request Guidelines
- Commits: concise, imperative subject (<=72 chars). Reference issues/PRs, e.g., "ranger: fix stun timing (#255)".
- Scope small, logically grouped changes; include short body for context when needed.
- PRs: describe changes, link issues, list validation steps (build/run commands), and note data/world impacts. Include logs/screens where useful.
- Do not commit generated binaries or runtime data (`bin/`, `build/`, many `lib/` paths are git-ignored).

## Security & Configuration
- World files live in a separate repo; keep `lib/world/` and player data out of commits.
- Never check in PII or live server logs (`log/`). Use local testing accounts and sanitized samples.

