# M03 First Iceberg read
Status: 🟨 doing · ADRs: [ADR-0003](../adrs/0003-iceberg-parquet-strategy/adr.md) (accepted) · Arch: [arch/](../arch/README.md)

## Goal
`doqe` reads a small Iceberg table from local disk and prints the correct rows, through the ADR-0002
modules. Format v2, no delete files, types int64 and string with nulls, no partition pruning, single-threaded.

## Features
- F03.1 Iceberg and Parquet strategy: ADR-0003 (library vs in-house vs mixed), with a measured spike → [T0006](../work/T0006-adr-0003-iceberg-strategy/task.md) ✅
- F03.2 `base::Status` and the minimal `column` layout for the M03 types, with Arrow C Data export → [T0007](../work/T0007-status-column-memory/task.md) ✅
- F03.3 Table metadata and scan planning: `metadata.json`, manifest list, manifests (`table`, local-path `catalog`) → TBD
- F03.4 Data file reading for the M03 types in `format`, batches produced by `exec` → TBD
- F03.5 `doqe scan <path>` in `cli`, checked against expected output → TBD
- F03.6 Test tables: small checked-in Iceberg tables with a record of how they were generated, no new build tool → TBD

## Exit criteria
- [x] ADR-0003 is accepted
- [ ] `doqe scan` on each checked-in test table prints the expected rows on the Mac and dev01 (`ctest --preset debug`)
- [ ] Debug and ASan+UBSan builds, format, lint and module-boundary checks pass
- [ ] CI is green on `main`
