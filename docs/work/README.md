# work/ — Tasks

The units of work and their progress. One folder per task, holding the task file and everything it refers to. Start every session here: read this file, then the task marked **Next**.

## Layout
```
work/
├── README.md             # this file: rules, template, task board
└── TNNNN-short-slug/     # one folder per task, e.g. T0001-example-task
    ├── task.md           # goal, scope, done-when checks, notes, result (template below)
    └── refs/             # what the task refers to: proof, scripts, data, review notes
        ├── <name>.sh     # command or script that produced a result
        └── <name>.txt|csv # its summarized output, same name as the script
```

## Rules
- Status: ⬜ todo · 🟨 doing · ✅ done · ⏸ parked
- Every task belongs to a milestone and a feature in `road/`.
- Commit messages start with the task ID, e.g. `T0001: add first test`.
- A task is ✅ only with a commit and proof in its Result: a short summary there, details in `refs/`.
- Each result file begins with: date, machine, OS, toolchain and version, build settings, commit.
- `refs/` holds text and CSV only, under about 100 KB; commit summaries, never raw logs. Raw logs stay outside the repo.
- A task with no files to keep has no `refs/`; the folder then holds only `task.md`.
- Update the board below in the same commit as the task file.

## Template: `TNNNN-slug/task.md`
````markdown
# TNNNN Title
Status: ⬜ todo · Type: feature / bug / refactor / optimization · Risk: low / high · Milestone: MNN · Feature: FNN.N · ADRs: none

## Goal
What this task changes, in one or two lines.

## Scope
- In: what this task covers
- Out: what it deliberately leaves for later

## Edge cases
- Boundary and failure cases the tests must cover

## Done when
- [ ] A checkable result, with the command that proves it

## Performance budget
None, or: what must not get slower or bigger, and what improvement is the target.

## Notes
- YYYY-MM-DD what happened, what was learned

## Result
Commit: — · Proof: — (one-line summary; files in `refs/`)
````

## Board
Next: T0006

| Task | Title | Milestone | Status |
|------|-------|-----------|--------|
| [T0001](T0001-adr-0001-toolchain/task.md) | ADR-0001: core C++ toolchain | M01 | ✅ done |
| [T0002](T0002-build-test-skeleton/task.md) | Build and test skeleton | M01 | ✅ done |
| [T0003](T0003-quality-checks-ci/task.md) | Quality checks and CI | M01 | ✅ done |
| [T0004](T0004-adr-0002-architecture/task.md) | ADR-0002: architecture, modules and components | M02 | ✅ done |
| [T0005](T0005-module-skeleton/task.md) | Module skeleton and boundary checks | M02 | ✅ done |
| [T0006](T0006-adr-0003-iceberg-strategy/task.md) | ADR-0003: Iceberg and Parquet strategy | M03 | 🟨 doing |
