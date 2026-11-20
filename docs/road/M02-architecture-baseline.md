# M02 Architecture baseline
Status: 🟨 doing · ADRs: [ADR-0002](../adrs/0002-architecture-modules/adr.md) (proposed) · Arch: [arch/](../arch/README.md)

## Goal
Define doqe's modules, layers, dependency rules and code layout, and enforce them in the build,
so every later feature lands in a known module with known boundaries. No data features.

## Features
- F02.1 Architecture decision: ADR-0002 (modules, layers, dependency rules, code layout, error policy, column format principles, public surfaces) → [T0004](../work/T0004-adr-0002-architecture/task.md) ✅
- F02.2 Module skeleton: one static library per active module with a placeholder source as needed and the allowed dependencies enforced → task not yet created

## Exit criteria
- [x] ADR-0002 is accepted
- [ ] Every module has a `docs/arch/<module>.md` with an honest Status, listed in `docs/arch/README.md`
- [ ] Each active module is its own CMake static library with only its allowed dependencies. The deferred `dist` module is documented and excluded from M02 targets and links.
- [ ] Seeded forbidden links and direct includes fail the checks. A positive transitive public-header fixture passes.
- [ ] CI is green on `main`
