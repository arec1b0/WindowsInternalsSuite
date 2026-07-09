# Windows Internals Suite

A single native Windows application for inspecting operating-system internals:
processes, threads, virtual memory, PE files, kernel objects, tokens, heaps, and
the PEB/TEB. Conceptually similar to the Sysinternals utilities, but unified into
one modular, extensible tool.

> **Status: early development.** The build foundation and the lower layers
> (`common`, `ntapi`) are in place; domain modules land incrementally. See
> [Project status](#project-status) for the exact per-module state — the tree in
> [Architecture](#architecture) shows the *target* design, not what is shipped today.

---

## Goals

- **Native API first.** Where practical, the suite calls the Windows Native API
  (`ntdll` exports such as `NtQuerySystemInformation`, `NtOpenProcess`,
  `NtReadVirtualMemory`) instead of the higher-level Win32 wrappers.
- **No driver required.** The base feature set runs entirely in user mode.
  A kernel driver, if added later, stays optional.
- **Minimal third-party surface.** The core depends only on the Windows SDK
  (Win32, `ntdll`, `dbghelp`). Optional extras (Capstone disassembler,
  GoogleTest) are opt-in via CMake flags.
- **Correctness at ABI boundaries.** Native structures are defined locally and
  guarded with compile-time `static_assert` size checks to catch layout drift.

---

## Requirements

| Item        | Requirement                                              |
| ----------- | ------------------------------------------------------- |
| OS          | Windows 10 or 11, x64                                    |
| Compiler    | MSVC (Visual Studio 2022, toolset v143)                  |
| Language    | C++20                                                    |
| Build system| CMake ≥ 3.25                                            |
| Optional    | Ninja (for the command-line presets); clang-tidy        |

Some queries against protected or system processes require elevation
(`SeDebugPrivilege`). Without it, those operations degrade gracefully to
best-effort partial results rather than failing outright.

---

## Building

The project is CMake-based and ships with presets. Visual Studio 2022 opens the
folder directly (File → Open → Folder) and picks up `CMakePresets.json`.

### Visual Studio generator

```powershell
cmake --preset vs2022
cmake --build build\vs2022 --config Debug
```

### Ninja (command line)

The Ninja presets require the **x64 Native Tools Command Prompt for VS 2022**,
so the x64 toolchain is on `PATH`:

```powershell
cmake --preset ninja-debug
cmake --build build\ninja-debug
```

### Build options

| Option                   | Default | Effect                                        |
| ------------------------ | ------- | --------------------------------------------- |
| `WIS_BUILD_TESTS`        | `ON`    | Build the unit-test target (fetches GoogleTest)|
| `WIS_WARNINGS_AS_ERRORS` | `OFF`   | Treat compiler warnings as errors (`/WX`)     |
| `WIS_ENABLE_CLANG_TIDY`  | `OFF`   | Run clang-tidy during build (Ninja only)      |
| `WIS_ENABLE_CAPSTONE`    | `OFF`   | Enable the Capstone-based disassembler        |

> **Note:** `clang-tidy` integration works only with the Ninja/Makefile
> generators. The Visual Studio generator ignores `CMAKE_CXX_CLANG_TIDY`; a
> dedicated tidy target for VS builds is planned.

---

## Architecture

Layering is strict and one-directional: each layer depends only on the ones
below it. The UI never touches `ntdll` directly — it goes through the `core`
domain managers, which in turn go through the `INativeApi` seam. That seam is
what makes the domain layer unit-testable against a fake native backend.

```
app            composition root: wires the dependency graph, owns main()
 │
ui             Win32-free viewmodels + a swappable Win32 view backend
 │
core           domain logic: process/thread/memory/pe/handle/... managers
 │
ntapi          typed wrappers over ntdll, behind the INativeApi interface
 │
common         cross-cutting primitives: Result<T,E>, RAII, logging, text
```

### Design principles

- **SOLID**, applied pragmatically. The most load-bearing part is DIP: `core`
  and `ui` depend on interfaces (`INativeApi`, `IProcessManager`, …), never on
  concrete implementations.
- **RAII everywhere** for OS resources — handles, `LocalAlloc` buffers,
  `VirtualAlloc` regions, and token privileges each have an owning wrapper.
- **`Result<T, E>` over exceptions** at module boundaries. This is a
  `std::expected`-compatible type for C++20; migrating to `std::expected` once
  the project moves to C++23 is a header swap. Exceptions remain reserved for
  genuinely exceptional conditions.
- **UTF-8 internally**, UTF-16 only at the Win32 boundary.

### Module layout (target)

```
WindowsInternalsSuite/
├── cmake/            shared CMake modules (flags, warnings, deps, packaging)
├── docs/             architecture notes and per-module documentation
├── src/
│   ├── common/       Result, RAII wrappers, logger, encoding, hex formatting
│   ├── ntapi/        NtDll binding, native structs, INativeApi seam
│   ├── core/         domain managers (process, thread, memory, pe, ...)
│   ├── ui/           viewmodels + Win32 views
│   └── app/          main(), composition root, manifest, resources
├── tests/            unit tests (GoogleTest) + native-API fakes
└── tools/scripts/    configure / build / test helpers
```

---

## Project status

The lower layers are implemented and building; the domain and UI layers are in
progress. This table tracks reality, not intent.

| Layer / module            | State           | Notes                                            |
| ------------------------- | --------------- | ------------------------------------------------ |
| Build foundation          | ✅ Done         | CMake, presets, compiler/warning modules         |
| `common`                  | ✅ Done         | Result, RAII, logging, encoding, hex             |
| `ntapi`                   | ✅ Core done    | Binding + system/process/thread/memory/object    |
| `core` · Process          | 🟡 Partial      | Enumeration + details; process control pending   |
| `core` · Thread           | 🟡 Partial      | Enumeration + details; stack walk pending        |
| `core` · Memory           | ⬜ Planned      | Region map, read, signature scan, dump           |
| `core` · PE               | ⬜ Planned      | Headers, sections, imports/exports, resources    |
| `core` · Module           | ⬜ Planned      | Loaded DLLs, base/entry, signature, version      |
| `core` · Handle           | ⬜ Planned      | System-wide handle table, per-object detail      |
| `core` · Token            | ⬜ Planned      | SID, integrity, privileges, groups               |
| `core` · Heap             | ⬜ Planned      | Heap segments and allocated blocks               |
| `core` · PEB/TEB          | ⬜ Planned      | Process/thread environment blocks                |
| `core` · System info      | ⬜ Planned      | CPU, NUMA, RAM, cache, uptime, pagefile          |
| `core` · Native API monitor| ⬜ Planned     | Observed native call surface                     |
| `ui` (viewmodels + Win32) | ⬜ Planned      | Not started                                      |
| `app` (entry point)       | ⬜ Planned      | Not started                                      |
| Tests                     | ⬜ Planned      | Scaffolding exists; suites pending               |
| Documentation             | 🟡 In progress  | This README; per-module docs pending             |

Legend: ✅ done · 🟡 partial · ⬜ planned

---

## Testing

Unit tests target the layers that can be exercised without live OS state: the
`common` primitives (`Result`, RAII wrappers, hex formatting) and the `core`
domain logic, which is tested against a fake `INativeApi` implementation rather
than the real `ntdll`. Once built:

```powershell
ctest --preset vs2022-debug
```

---

## Roadmap

1. Finish the `core` domain modules (Memory and PE are the largest).
2. Add process/thread control (terminate, suspend, resume) behind explicit
   confirmation, kept separate from the read-only enumeration paths.
3. Thread stack walking with symbol resolution (`dbghelp` / `SymbolResolver`).
4. Build the Win32 UI: viewmodels first (testable, Win32-free), then the view
   backend, with enumeration moved off the UI thread.
5. Optional extras: Capstone disassembly, digital-signature analysis, report
   export (JSON/HTML), light/dark themes, plugins.

---

## License

See `LICENSE` (to be added).