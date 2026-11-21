# Embeddable C++ API
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) · Code: `src/api/` (empty skeleton)

The static target is an empty skeleton.

## Responsibility

Provide a thin C++ façade over engine for configuration, session setup, SQL submission, and results.

## Design

Consume the `ArrowArrayStream` C ABI through engine’s public result header without including column headers. Map internal `base::Status` to an API-owned error type. Embedders build with the same pinned toolchain and static libc++ under ADR-0001. No API stability before 1.0.

## Interfaces

Designated C++ façade headers and error type. Results use the Arrow C Data and Stream C ABI with no Arrow library dependency.

## Invariants

Designated public headers expose façade types, standard library types, and Arrow C interchange declarations. A result remains valid for its promised lifetime through its release callbacks. The error mapping preserves uncertain publication and retryable conflicts.

## Planned

Plan one CMake package exporting the façade target with its static links.

## Open questions

Initial signatures, package installation details, and result schema mapping.
