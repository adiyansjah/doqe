# refs/

Everything this ADR points to. Keep it small and reproducible.

| File | Holds |
|---|---|
| `sources.md` | Links, papers, docs, each with one line on the claim it supports and the date read |
| `<name>.sh` | The script or command that produced a result |
| `<name>.txt` / `<name>.csv` | Raw output summary from that script |

Rules
- Begin each output file with: date, machine, OS, compiler, build type, commit.
- Give each output the same name as the script that produced it (`bench-scan.sh` produces `bench-scan.csv`).
- Text and CSV only, under about 100 KB. Large dumps stay out of git; commit the summary.
- Never edit results after the ADR is accepted. Re-run and add a new file.

Delete this README when you copy the template.
