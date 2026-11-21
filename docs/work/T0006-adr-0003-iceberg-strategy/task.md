# T0006 ADR-0003: Iceberg and Parquet strategy
Status: 🟨 doing · Type: feature · Risk: high · Milestone: M03 · Feature: F03.1 · ADRs: ADR-0003 (planned)

## Goal
Draft ADR-0003 choosing how doqe reads (and later writes) Iceberg metadata and Parquet data files;
the owner decides from measured options and accepts it.

## Scope
- In: options at least (a) iceberg-cpp with Arrow, (b) in-house Iceberg metadata and Parquet reader,
  (c) in-house Iceberg metadata with a library for Parquet; fit with ADR-0001 (pinned toolchain, static
  libc++, no package manager) and ADR-0002 (own column format, no Arrow dependency, module placement);
  a throwaway spike outside the repo measuring build cost, binary size, toolchain fit and licences;
  test-table generation approach for F03.6.
- Out: writes, deletes, REST catalog, codecs beyond what M03 test tables need (may be named as follow-ups).

## Edge cases
- A chosen library that needs Arrow, Thrift, Avro, zstd/snappy or a package manager: what ADR-0001/0002 then require.
- Iceberg v3 and future Parquet features: what each option costs to keep up with.
- Licence and vendoring rules for any third-party code.

## Done when
- [ ] ADR-0003 exists with options, measured references and a proposed decision, listed in the ADR index
- [ ] Two independent reviews done and findings resolved
- [ ] The owner accepts ADR-0003; AGENTS.md and DEVELOPMENT.md get any resulting rules

## Performance budget
None.

## Notes
- 2026-11-21 task started. The owner has not chosen an option; do not assume one.

## Result
Commit: — · Proof: —
