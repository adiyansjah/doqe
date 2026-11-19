# AGENTS.md

Instructions for any AI coding agent working on doqe, a portable C17 vectorized SQL engine
(single node first, distributed later) for macOS ARM64, Linux x86_64 and Linux ARM64.

## Start every session
1. Read `docs/work/README.md` and open the task marked **Next**.
2. Read its milestone in `docs/road/`, the related part in `docs/arch/`, and any linked ADRs in `docs/adrs/`.
3. Follow the task loop and definition of done in `docs/workflow.md`.

## Rules
- C17 only, no compiler extensions in core code; OS-specific code and intrinsics only in `src/platform/`.
- Write a failing test before the code. Never loosen a test to make it pass.
- Report evidence: exact commands and their results. Mark anything unchecked "not verified".
- Commit messages start with the task ID, e.g. `T0001: add debug-asan preset`.
- Update the task file, the board, and `docs/arch/` in the same commit as the change.
- Write an ADR before code when there is a real design choice; the owner approves it.
- No x86 performance numbers from the Mac (emulation has no AVX); benchmark on the target machine.

## Ask the owner first
Deleting files, pushing, changing public interfaces or the on-disk/wire format,
anything that costs money, and adding files outside `docs/`, `src/`, `tests/`, `bench/`.

## Commands
Not set up yet. The build and test commands go here once the first task adds them.
