# T0010 Isolate Status allocation probes from ASan tests
Status: ✅ done · Type: bug · Risk: low · Milestone: M03 · Feature: F03.2 follow-up · ADRs: none

## Goal

Restore Linux ASan allocation-mismatch diagnostics in the shared test executable after the T0007 regression.

## Scope

- In. Run the existing Status tests in `doqe_status_tests` and cover that executable with the module-boundary checks.
- Out. Production allocator changes and the T0008 metadata implementation.

## Edge cases

- Linux ASan must diagnose `new` paired with `free`, array `new` paired with scalar `delete`, and wrong-sized `delete`.
- Status allocation counts and injected failures must still be exercised under CTest in Debug and ASan+UBSan.

## Done when

- [x] Record the existing failing `quality-checks.sh` run on dev01 main before applying the fix.
- [x] On Mac, `ctest --preset debug`, `cmake --workflow --preset asan-ubsan`, `cmake --build --preset quality`, and the Debug module-boundary checks pass.
- [x] On dev01, `bash docs/work/T0003-quality-checks-ci/refs/quality-checks.sh` exits 0 with all three Linux allocation-mismatch diagnostics.
- [x] Confirm the Status suite runs from `doqe_status_tests` and passes on Mac and dev01.
- [x] Update the architecture and development docs.
- [ ] CI is green on `main`.

## Performance budget

None.

## Notes

- 2026-11-21 confirmed the retained dev01 failure on clean main `4f031b0`
  in `/home/dev/doqe-main-check/quality-checks.log`. The proof script exited 1
  when CTest returned 0 for `new-free` without an ASan diagnostic. T0007
  (`be62ac7`) linked the strong replacement `operator new` from
  `tests/base/status_test.cpp` into `doqe_tests`, routing allocation through
  `malloc` and bypassing ASan's C++ allocation-family tracking.

- 2026-11-21 verified the Mac and dev01 runs. All three Linux seeds emitted
  their expected ASan diagnostics, and the Status allocation probes passed
  from the separate executable in both presets on both hosts. Main CI is
  pending. T0010 must merge before T0008.

## Result

Commit: this T0010 commit · Proof: [41/41 tests per host and preset, passing quality and module checks, and all three Linux ASan diagnostics](refs/verification.txt)
