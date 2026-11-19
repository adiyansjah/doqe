# work/ — Tasks

The units of work and their progress. One file per task, holding its goal, done-when checks,
notes, and result. Start every session here: read this file, then the task marked **Next**.

## Layout
```
work/
├── README.md             # this file: rules, template, task board
└── TNNNN-short-slug.md   # one file per task, e.g. T0001-example-task.md
```

## Rules
- Status: ⬜ todo · 🟨 doing · ✅ done · ⏸ parked
- Every task belongs to a milestone and a feature in `road/`.
- Commit messages start with the task ID, e.g. `T0001: add first test`.
- A task is ✅ only with a commit and proof (test output, log summary) in its Result.
- Update the board below in the same commit as the task file.

## Template: `TNNNN-slug.md`
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
Commit: — · Proof: —
````

## Board
Next: none

| Task | Title | Milestone | Status |
|------|-------|-----------|--------|
