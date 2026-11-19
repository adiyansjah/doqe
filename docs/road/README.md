# road/ — Roadmap

What we are building next, as milestones. Each milestone has a goal, its features, and exit criteria
that prove it is done. Unplanned ideas wait in `backlog.md` until they are scheduled.

## Layout
```
road/
├── README.md              # this file: rules, template, milestone list
├── MNN-short-slug.md      # one file per milestone, e.g. M01-skeleton.md
└── backlog.md             # unplanned ideas, one line each
```

## Rules
- Status: ⬜ todo · 🟨 doing · ✅ done · ⏸ parked
- One milestone is 🟨 at a time; it is marked **Current** below.
- Every exit criterion is checkable and names the command that proves it.
- A milestone is ✅ only when all its exit criteria are ticked.
- Features are numbered `FNN.N` and link to their tasks in `work/`.

## Template: `MNN-slug.md`
````markdown
# MNN Title
Status: ⬜ todo · ADRs: none · Arch: none

## Goal
What this milestone delivers, in one or two lines.

## Features
- FNN.1 Name → TNNNN
- FNN.2 Name → TNNNN

## Exit criteria
- [ ] A checkable result, with the command that proves it
````

## Milestones
Current: none

| #   | Milestone | Status | Exit criteria (short) |
|-----|-----------|--------|-----------------------|
