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

macOS executables export no symbols to prevent dyld from coalescing weak C++ runtime definitions at startup.
Shared libraries and plugins will need an exported-symbols design via an ADR. F01.3 will need to
export macOS sanitizer hooks such as `__asan_default_options`.

## Parts
| File | Part | Status |
|------|------|--------|
