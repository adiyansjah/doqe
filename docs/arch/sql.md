# SQL parsing and planning
Status: planned · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Code: `src/sql/` (not yet created)

## Responsibility

Parse and bind SQL, apply SQL type rules, plan and optimize queries, and bind procedure calls to registered callables.

## Design

Resolve names through catalog and use table for Iceberg predicate rules. Bind functions against the exec registry and lower plans and expressions to exec contracts. Bind CALL arguments and dispatch to callables registered by engine using the callable signature declared in exec and built only from types below sql.

## Interfaces

Internal statement binding and planning, and procedure binding against the exec callable contract.

## Invariants

The runner receives resolved inputs and bound expressions. A CALL dispatches only to a registered procedure after argument binding succeeds. SQL builds without maintain headers or links.

## Planned

Create the target in F02.2. Add the first statement-to-result path in a later slice. Share an authorization point with server and maintenance under the Governance and authorization ADR.

## Open questions

Initial dialect, optimizer rules, and procedure argument and result signatures.
