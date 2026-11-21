# Byte and object I/O
Status: local reads implemented in T0008 · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md), [ADR-0005](../adrs/0005-metadata-arena-planning/adr.md) · Code: [`src/io/`](../../src/io/)

## Responsibility

Provide local and object byte access, listing and deletion, and the outbound HTTP/TLS and request-signing client.

## Interfaces

[`FileSystem`](../../src/io/include/doqe/io/file_system.hpp) supplies a capped
whole-file read into caller-owned arena storage and an exact range read into
caller-provided bytes. A file system can use the same arena for dynamic read
scratch. `LocalFileSystem` implements both operations using a regular-file
descriptor and positional reads.

## Invariants

- Whole-file reads check the size cap before allocating input bytes. They read the
  stat size and probe for EOF. A short read or unexpected extra byte returns
  `IoError`.
- Range reads check offset arithmetic before opening the file. Overflow returns
  `InvalidArgument`. Short reads and directory targets return `IoError`.
  Both read operations return `NotFound` for missing paths (`ENOENT`) and paths
  with a non-directory component before the final name (`ENOTDIR`).
- Bare absolute paths preserve literal `%`, `?` and `#`. File URI schemes are
  case-insensitive. Absolute file URIs with an empty authority are supported.
  File URI queries and fragments return `InvalidArgument` and are checked before
  percent signs, which return `Unsupported`. Nonempty authorities and other
  URI schemes return `Unsupported`. Embedded NULs and relative bare paths
  return `InvalidArgument`.

## Planned

Add object-store backends as needed. Each instance owns its credentials,
transport settings and caches. Cache footer bytes for format to interpret.

## Open questions

Initial object-store backend and I/O scheduling.
