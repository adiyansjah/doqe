# Physical execution
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/exec/` (not yet created)

## Responsibility

Run local physical plans, vectorized operators, and bound expressions. Own the function registry for signatures and kernels.

## Design

Scan and write operators consume table contracts. They use io and format to read and decode or encode and write. Scan removes position deletes by exact match to the recorded data-file path (`file_path`) and original row position, reuses the hash join as an anti-join for equality deletes with Iceberg key semantics, applies Iceberg v3 deletion vectors decoded by format when applicable, then evaluates the residual and projects the output. Writers return data-file metadata to table for commit. Execution uses the supplied function registry instance and query pool.

## Interfaces

Physical plans, bound expressions, function signatures and kernels, the procedure callable signature built only from types below sql, the local runner, scan operators, and writer results.

## Invariants

Position deletes use original file row numbers. Equality deletes match null keys to null keys. The output omits hidden inputs unless requested. The runner accepts resolved inputs and writer operators complete without publication.

## Planned

Create the target in F02.2. Add the distributed runner seam when needed.

## Open questions

Operator scheduling under the Engine concurrency model ADR.
