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
Next: none

| Task | Title | Milestone | Status |
|------|-------|-----------|--------|
