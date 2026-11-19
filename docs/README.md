# doqe docs

| Path | Answers | Changes |
|---|---|---|
| [WORKFLOW.md](WORKFLOW.md) | How do we work? General method, no tool names | When the process changes |
| [DEVELOPMENT.md](DEVELOPMENT.md) | Which tools, commands and machines? | Same commit as the tool change |
| [arch/](arch/) | How is it built right now? | Same commit as the design change |
| [adrs/](adrs/) | Why this design and not another? | New ADR per decision, never edited after acceptance |
| [road/](road/) | What are we building next? | Per milestone |
| [work/](work/) | What is being done, and how far along is it? | Every session |

Flow: root README (vision, goals, non-goals) → arch → adrs → road → work. Every task traces up to a milestone, and every design choice traces to an ADR.

Each folder's README explains it and holds its template. Start a session with `work/README.md`.
