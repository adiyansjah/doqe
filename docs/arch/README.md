# arch/ — Architecture

How the project is built **right now**, plus where it is heading. A living document: update it in the same
commit as the design change. Why a choice was made belongs in `adrs/`, not here; old designs live in ADRs and git.

## Layout
```
arch/
├── README.md        # this file: rules, templates, overview, parts list
└── <part>.md        # one file per part, e.g. storage.md, api.md
```

## Rules
- The `Status` line is honest: `implemented` · `partial` · `planned`.
- Describe the current state only; no crossed-out history.
- About 1–2 pages per file; detail goes in code comments and tests.
- Write invariants so a test can check them.

## Template: `<part>.md`
````markdown
# Part name
Status: planned · ADRs: none · Code: `<path>`

## Responsibility
What this part does, and what it does not do.

## Design
How it works. Diagram if useful.

## Interfaces
Key structs and functions other parts use.

## Invariants
Rules that must always hold.

## Planned
Intended changes, with milestone or ADR.

## Open questions
Not decided yet; becomes an ADR when it matters.
````

## Overview
The walking skeleton exposes `doqe --version`. The executable uses the CMake project version
and the Git revision and worktree status captured during the build. A source export reports
`unknown` for the revision. The build and test setup follows
[ADR-0001](../adrs/0001-core-cpp-toolchain/adr.md).

macOS executables export no symbols to prevent dyld from coalescing weak C++ runtime definitions at
startup. Debug and Release link the pinned static C++ runtime and run without ASan. The
macOS ASan+UBSan preset uses the pinned compiler-rt ASan dylib with static libc++ and empty export
tables. ASan on macOS sees `malloc` and `free` beneath the static C++ allocation operators.
Allocation-family mismatch detection runs on Linux, where compiler-rt's `asan_cxx` supplies
`operator new` and `operator delete`. Shared libraries and plugins will need an exported-symbols
design via an ADR. [Development checks](../DEVELOPMENT.md#checks)

The 13 active modules build as static libraries with links restricted to declared lower-layer dependencies.
The engine will compose SQL, catalog, execution, and maintenance for the server and embedding façade.
The module map and allowed edges are in [ADR-0002](../adrs/0002-architecture-modules/adr.md#layers-and-direct-dependencies-criteria-1-and-4).
The build checks declared links, final CLI and test link inputs, and source inclusion.
Module targets export only their public include roots. Shared test support uses module-public
interfaces. The boundary checker uses the compiled support target's direct links for the whole
support directory when that target exists. Otherwise, it uses the test executable's direct links.
GoogleTest headers are limited to test targets.
Code review covers symbol-level use without headers and deliberate build-script tricks,
including custom archive edits and overrides of `CMAKE_CXX_ARCHIVE_CREATE`.
Distributed execution is deferred beyond M02.

## Parts
| File | Part | Status |
|------|------|--------|
| [base.md](base.md) | Foundation | planned |
| [column.md](column.md) | Column types and buffers | planned |
| [io.md](io.md) | Byte and object I/O | planned |
| [format.md](format.md) | File codecs | planned |
| [table.md](table.md) | Iceberg table semantics | planned |
| [catalog.md](catalog.md) | Iceberg catalog | planned |
| [exec.md](exec.md) | Physical execution | planned |
| [sql.md](sql.md) | SQL parsing and planning | planned |
| [maintain.md](maintain.md) | Table maintenance | planned |
| [engine.md](engine.md) | Engine composition and query lifecycle | planned |
| [api.md](api.md) | Embeddable C++ API | planned |
| [server.md](server.md) | Network server | planned |
| [cli.md](cli.md) | Command-line interface | partial |
| [dist.md](dist.md) | Distributed execution | planned |
