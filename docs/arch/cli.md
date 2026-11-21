# Command-line interface
Status: partial · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) · Code: `src/cli/`

The CLI implements `doqe --version`. The command owns version formatting and
build provenance because both are part of its output contract.

## Responsibility

Own command syntax, interactive input, formatting, and process exit codes.

## Design

Call api for embedded SQL and submit administrative operations as SQL procedures through the same façade.

## Interfaces

Command arguments, output, and exit status.

## Invariants

The CLI has only the api module as a direct dependency. Query and maintenance submission use the same façade entry point, and failures map to exit status.

## Planned

Add SQL, interactive, and administrative commands.

## Open questions

Initial SQL, interactive, and administrative commands.
