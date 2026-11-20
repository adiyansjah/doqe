# Development

Everything tool- and machine-specific for this project: language, toolchain, commands, checks, machines, benchmarks.
The method lives in [WORKFLOW.md](WORKFLOW.md); why each tool was chosen lives in ADRs in [adrs/](adrs/).

The owner chose C++ as the implementation language, as recorded in the [root README](../README.md).
ADR-0001 decides the C++ standard, build system, compilers, test framework and target platforms.
Record the remaining choices only from accepted ADRs. Describe the current setup only, and update it in the same
commit as the tool change. Every ADR-based entry links the ADR it comes from.

## Language and toolchain
Language and version, compilers or runtimes, build tools, versions, and how to install them.

The language is C++. Toolchain setup is pending ADR-0001.

## Commands
Exact commands to build, test, run each check, and reproduce CI locally.

Not set up yet.

## Checks
The tool that covers each check kind in WORKFLOW.md.

| Check kind | Tool | Where it runs |
|---|---|---|
| memory safety / undefined behavior | | |
| races and leaks | | |
| fuzz / property tests | | |
| static checks and formatting | | |
| target platforms | | |

## Machines
Each machine or CI runner and what it is trusted for (correctness, performance, or both).

| Machine | Platform | Use for | Not for |
|---|---|---|---|

## Benchmarks
Harness, which machine to run on, and where results are recorded.

Not set up yet.
