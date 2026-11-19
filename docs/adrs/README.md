# adrs/ — Architecture Decision Records

Why the design is the way it is. One folder per decision, holding the ADR and everything it refers to.
An accepted ADR is never edited; to change a decision, write a new ADR and mark the old one `superseded by NNNN`.

## Layout
```
adrs/
├── README.md               # this file: rules, template, index
└── NNNN-short-slug/        # e.g. 0001-c17-no-extensions
    ├── adr.md              # the decision (template below)
    └── refs/               # what the ADR refers to
        ├── sources.md      # links, each with the claim it supports and the date read
        ├── <name>.sh       # script or command that produced a result
        └── <name>.csv|txt  # its output, same name as the script
```

## Rules
- No decision without References. If it is judgment only, say so.
- Each result file begins with: date, machine, OS, compiler, build type, commit.
- `refs/` holds text and CSV only, under about 100 KB; commit summaries, not dumps.
- Results are never edited after acceptance; re-run and add a new file.
- Status: `proposed` · `accepted` · `rejected` · `superseded by NNNN`

## Template: `NNNN-slug/adr.md`
````markdown
# ADR-NNNN: Short title in the form "Use X for Y"

| Status | Date | Decided by | Confidence | Tags |
|---|---|---|---|---|
| proposed | YYYY-MM-DD | owner | low / medium / high | e.g. execution, build |

Supersedes: none · Superseded by: none · Related: none

## Context
What problem forces a decision now? Constraints, and what happens if we don't decide.

## Criteria
What a good answer must do, most important first.
1. ...

## Options
### A. Name
- Good: ...
- Bad: ...

### B. Name
- Good: ...
- Bad: ...

## References
- **Measured:** `refs/<file>`: result in one line (machine, OS, build type, commit). Reproduce: `refs/<script>`
- **Source:** link or `refs/sources.md`: the claim it supports.
- **Judgment:** reasoning only, no data. Say so plainly.

## Decision
We choose **X** because ... (tie back to criteria and references).

## Consequences
- Easier: ...
- Harder / cost we accept: ...
- Follow-up work: ...

## Revisit if
A specific result or change that would make us reconsider.
````

## Index
Newest first.

| #    | Date | Title | Status |
|------|------|-------|--------|
