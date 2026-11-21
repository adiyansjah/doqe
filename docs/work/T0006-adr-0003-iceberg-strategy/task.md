# T0006 ADR-0003: Iceberg and Parquet strategy
Status: ✅ done · Type: feature · Risk: high · Milestone: M03 · Feature: F03.1 · ADRs: [ADR-0003](../../adrs/0003-iceberg-parquet-strategy/adr.md)

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
- [x] ADR-0003 exists with options, measured references and a proposed decision, listed in the ADR index
- [x] Two independent reviews done and findings resolved
- [x] The owner accepts ADR-0003; AGENTS.md and DEVELOPMENT.md get any resulting rules

## Performance budget
None.

## Notes
- 2026-11-21 added the [proposed ADR-0003](../../adrs/0003-iceberg-parquet-strategy/adr.md)
  and [Mac spike summary](../../adrs/0003-iceberg-parquet-strategy/refs/mac-spike.txt).
- 2026-11-21 revised the draft to record the owner's Option B decision, uncompressed M03
  input, and fixture relocation through `io`. Updated the options and spike provenance
  from the review findings. A throwaway PyIceberg/PyArrow check confirmed that swapped
  names and Parquet field-ID order give the expected rows after removing the generated
  name-mapping property. The revised script accepted the pinned LLVM installation and
  rejected a substituted runtime. Verified provenance and value checks from the retained
  Mac build trees. Owner acceptance and end-to-end doqe checks are pending.
- 2026-11-21 one re-review: clarified missing field IDs vs optional columns absent from older files (nulls), required absent → Unsupported; corrected iceberg-cpp 0.4.0 features. Owner accepted ADR-0003.

## Result
Commit: see the T0006 acceptance commit · Proof: [Mac spike summary](../../adrs/0003-iceberg-parquet-strategy/refs/mac-spike.txt)
