# Command-line interface
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/cli/` (not yet created)

## Responsibility

Own command syntax, interactive input, formatting, and process exit codes.

## Design

Call api for embedded SQL and submit administrative operations as SQL procedures through the same façade.

## Interfaces

Command arguments, output, and exit status.

## Invariants

The CLI has only the api module as a direct dependency. Query and maintenance submission use the same façade entry point, and failures map to exit status.

## Planned

Create the target in F02.2 and preserve the existing version command.

## Open questions

Initial SQL, interactive, and administrative commands.
