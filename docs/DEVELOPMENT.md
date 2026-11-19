# Development

Everything tool- and machine-specific for this project: toolchain, commands, checks, machines, benchmarks.
The method lives in [WORKFLOW.md](WORKFLOW.md); why each tool was chosen lives in ADRs in [adrs/](adrs/).
Describe the current setup only, and update it in the same commit as the tool change.

## Toolchain
Compilers, build tools, versions, and how to install them.

Not set up yet.

## Commands
Exact commands to build, test, run each check, use the Linux VM, and run CI locally.

Not set up yet.

## Checks
The tool that covers each check kind in WORKFLOW.md.

| Check kind | Tool | Where it runs |
|---|---|---|
| memory safety / undefined behavior | | |
| races and leaks | | |
| fuzz / property tests | | |
| static checks and formatting | | |
| target platforms (CI) | | |

## Machines
Each machine or CI runner and what it is trusted for (correctness, performance, or both).

| Machine | Platform | Use for | Not for |
|---|---|---|---|

## Benchmarks
Harness, which machine to run on, and where results are recorded.

Not set up yet.
