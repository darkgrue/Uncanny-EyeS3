# Project AGENTS.md

## Project Overview

This is an embedded firmware project using **PlatformIO** within **VS Code** on **Windows**.
All build, upload, and serial monitor operations are managed through PlatformIO CLI or the
PlatformIO VS Code extension. Do NOT use Arduino IDE, CMake, or other build systems.

---

## Environment

- **OS**: Windows (paths use backslashes or forward slashes — prefer forward slashes in code)
- **IDE**: Visual Studio Code with the KiloCode and PlatformIO IDE extensions
- **Build System**: PlatformIO (pio)
- **Shell**: PowerShell or Command Prompt (CMD) — avoid bash-only syntax
- **Python**: Used internally by PlatformIO; do not invoke it directly unless necessary

---

## Dev environment tips
- When on a Windows environment, use `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe` to invoke PlatformIO, not `pio`.

## Build & Flash Commands

Always use PlatformIO CLI commands, not raw compiler/linker calls:
```
pio run                        # Build the project
pio run --target upload        # Build and flash to device
pio run --target clean         # Clean build artifacts
pio device monitor             # Open serial monitor
pio run --target uploadfs      # Upload filesystem image (if used)
```

Run these from the project root (where `platformio.ini` lives).

---

## Project Structure
```
/
├── AGENTS.md
├── platformio.ini             # PlatformIO config — primary project definition
├── src/
│   └── main.cpp               # Main firmware entry point
├── include/                   # Project-wide header files
├── lib/                       # Local libraries
├── test/                      # Unit tests (PlatformIO Unity framework)
└── .pio/                      # Build output — DO NOT edit or commit
```

---

## Code Style

- Language: **C/C++** (C++17 unless `platformio.ini` specifies otherwise)
- Indentation: 4 spaces (no tabs)
- Naming:
  - Functions and variables: `camelCase`
  - Constants and macros: `UPPER_SNAKE_CASE`
  - Classes and structs: `PascalCase`
- Keep files under 500 lines; split large modules into logical units under `src/` or `lib/`
- Avoid dynamic memory allocation (`new`/`malloc`) on constrained microcontrollers unless justified

---

## platformio.ini Guidelines

- Do not change `[env:...]` section names without asking the user for permission. If permission is given ensure all references are updated.
- Library dependencies go in `lib_deps`, not manually copied into `lib/`
- Board-specific flags belong in `build_flags`; keep them commented if experimental
- Do not hard-code upload ports — use `upload_port = auto` or leave unset when possible

---

## Libraries

- Prefer PlatformIO's registry (`lib_deps`) over manual installs
- Pin library versions to avoid breaking changes: `SomeLib @ 1.2.3`
- Document why each library is used in a comment next to its `lib_deps` entry

---

## Serial / Debug

- Use `Serial.begin(115200)` as the default baud rate unless the project specifies otherwise
- Wrap all debug output in `#ifdef DEBUG_ENABLED` guards so it can be stripped for release builds
- Do not leave blocking `while(!Serial)` calls in production code

### Serial Port Access Rules

**CRITICAL**: Before attempting to flash firmware or read serial output, you MUST check for and close any serial consoles that may be holding the COM port open. Failure to do so will result in "Access is denied" errors when trying to access the port.

**IMPORTANT**: Do NOT attempt to close serial monitor processes yourself. Always pause and ask the user with a question prompt to close any serial consoles (PlatformIO monitor, Arduino IDE Serial Monitor, PuTTY, etc.) before proceeding with flash or serial read operations.

When you need the user to close serial monitors:
1. Explicitly ask the user to close all serial console programs
2. Wait for confirmation before proceeding
3. After closing, wait 2-3 seconds for the port to be fully released

---

## Testing

- Unit tests live in `test/` and use the **PlatformIO Unity** framework
- Run tests with: `pio test`
- Mock hardware dependencies where possible; do not require physical hardware for logic tests
- PlatformIO “test” configuration extends a build environment with the `PIO_UNIT_TESTING` macro and with extra flags provided by the Unit Testing framework.

---

## Windows-Specific Notes

- Avoid hardcoded Unix paths (e.g., `/dev/ttyUSB0`); use `COMx` port format or leave auto-detect on
- Line endings: use **LF** in source files (configure `.editorconfig` or Git `core.autocrlf`)
- If you need to run scripts, use PowerShell syntax; avoid `#!/bin/bash` shebangs

---

## What NOT to Do

- Do not modify files inside `.pio/` — these are generated build artifacts
- Do not commit `*.bin`, `*.elf`, or other build outputs
- Do not introduce OS-level dependencies (WSL paths, Linux-only tools) without discussion
- Do not add Arduino IDE `.ino` files — this is a pure PlatformIO project

---

## Key Files (for quick lookup)

- `platformio.ini` — build config, board targets, lib_deps
- `src/main.cpp` — firmware entry point
- `src/` — additional .cpp/.h modules organized by functional domain
- `include/` — shared headers

---

## Common Mistakes to Avoid

- **Don't** run `pio` directly — use `%USERPROFILE%\.platformio\penv\Scripts\platformio.exe` on Windows
- **Don't** edit `.pio/` contents — will be overwritten on next build
- **Don't** add `.ino` files — PlatformIO uses `.cpp`, not Arduino-style sketches
- **Don't** use Unix paths (e.g., `/dev/ttyUSB0`) — Windows uses `COMx` or auto-detect
- **Don't** use `cd <dir> && <command>` in PowerShell — use the `workdir` parameter instead

---

## What Works Well

- Breaking modules into `src/` subdirectories by functional domain
- Using `#ifdef` guards for platform-specific code
- Serial output at 115200 baud for debugging
- Adding `-DDEBUG_ENABLED` to `build_flags` in `platformio.ini` to enable debug output

---

## Common Question Quick Answers

**Q: How do I add a new library?**  
A: Add it to `lib_deps` in `platformio.ini`, then run `pio run` to fetch and build.

**Q: Where are build artifacts?**  
A: `.pio/build/<env_name>/`

**Q: How to enable debug output?**  
A: Add `build_flags = -DDEBUG_ENABLED` to the `[env:...]` section in `platformio.ini`.

**Q: How do I clean a build?**  
A: Run `pio run --target clean`

**Q: What's the upload procedure?**  
A: Connect device, then run `pio run --target upload` from the project root.
