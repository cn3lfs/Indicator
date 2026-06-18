# Repository Guidelines

## Project Structure & Module Organization

This repository builds a TongDaXin CZSC visualization plugin as `CZSC.dll`.
Source files live at the repository root:

- `Main.cpp` / `Main.h`: exported plugin entrypoints and indicator logic.
- `CCentroid.cpp` / `CCentroid.h`: legacy centroid state retained for reference.
- `FxIndicator.h` and `FxSelector.h`: indicator/selector support headers.
- `CzscCore.cpp` / `CzscCore.h`: testable indicator and CZSC calculation core.
- `Makefile`: GCC/MinGW-style build rules.
- `README.md`: user-facing install instructions and TongDaXin formula example.
- `tests/`: lightweight C++ regression tests for the calculation core.

There is currently no asset pipeline.

## Build, Test, and Development Commands

- `make`: builds `CZSC.dll` from `Main.o` and `CzscCore.o`.
- `make test`: builds and runs the lightweight core regression test executable.
- `make clean`: removes generated `.dep` and `.o` files.
- `make debug`: builds the DLL, then launches `gdb -w CZSC.dll`.
- `make run`: builds, then attempts to execute the target DLL; this is mainly a Makefile convenience and is not a realistic plugin validation.

Use a GNU/MinGW toolchain. For WSL2 cross-compilation to the 32-bit Windows DLL required by TongDaXin, install `make`, native `g++`, `mingw-w64`, `gcc-mingw-w64-i686`, and `g++-mingw-w64-i686` manually or with `sh scripts/bootstrap-wsl-mingw.sh`. Use native `make test` for runnable core tests, `make mingw32-test-build` for cross-compiled test binary checks, and `make mingw32` for the DLL. Use `sh scripts/build-mingw32.sh` to check the toolchain, run native tests, cross-compile test code, and build the DLL.

## Coding Style & Naming Conventions

Match the existing C++ style: two-space indentation, braces on their own lines for functions and control blocks, and compact pointer declarations such as `float *pOut`. Existing identifiers use Hungarian-style prefixes (`nCount`, `pHigh`, `fValue`, `bValid`) and PascalCase for classes and public methods (`CCentroid`, `PushHigh`). Keep changes localized; do not reformat unrelated legacy code or comments.

## Testing Guidelines

Automated coverage is intentionally lightweight and uses plain C++ assertions-style return codes instead of a framework. For logic changes, add focused cases under `tests/` and run `make test`; also validate by building with `make` and manually checking the DLL in TongDaXin using the formula shown in `README.md` when exported behavior or chart rendering changes. Treat compiler warnings, linker errors, test failures, and changed exported behavior as release blockers.

## Commit & Pull Request Guidelines

Git history uses short, imperative messages such as `bugfix`, `fix small bug`, and `Update README.md`. Prefer a more specific variant in the same style, for example `fix centroid boundary update` or `document dll install path`.

Pull requests should include a concise description, affected files or behavior, validation performed (`make`, manual TongDaXin check), and screenshots only when chart rendering changes.

## Security & Configuration Tips

Do not commit local TongDaXin installation paths, private market data, or generated debug artifacts. `CZSC.dll` is a build output; replace it only when intentionally publishing a new plugin binary.
