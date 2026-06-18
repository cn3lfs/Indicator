# Repository Guidelines

## Project Structure & Module Organization

This repository builds a TongDaXin CZSC visualization plugin as `CZSC.dll`.
Source files live at the repository root:

- `Main.cpp` / `Main.h`: exported plugin entrypoints and indicator logic.
- `CCentroid.cpp` / `CCentroid.h`: centroid state and calculations.
- `FxIndicator.h` and `FxSelector.h`: indicator/selector support headers.
- `Makefile`: GCC/MinGW-style build rules.
- `README.md`: user-facing install instructions and TongDaXin formula example.

There is currently no dedicated `tests/` directory or asset pipeline.

## Build, Test, and Development Commands

- `make`: builds `CZSC.dll` from `Main.o` and `CCentroid.o`.
- `make clean`: removes generated `.dep` and `.o` files.
- `make debug`: builds the DLL, then launches `gdb -w CZSC.dll`.
- `make run`: builds, then attempts to execute the target DLL; this is mainly a Makefile convenience and is not a realistic plugin validation.

Use a Windows environment with compatible GNU tools (`make`, `gcc`, `c++`, `gdb`) available on `PATH`.

## Coding Style & Naming Conventions

Match the existing C++ style: two-space indentation, braces on their own lines for functions and control blocks, and compact pointer declarations such as `float *pOut`. Existing identifiers use Hungarian-style prefixes (`nCount`, `pHigh`, `fValue`, `bValid`) and PascalCase for classes and public methods (`CCentroid`, `PushHigh`). Keep changes localized; do not reformat unrelated legacy code or comments.

## Testing Guidelines

No automated test framework is present. For logic changes, add focused tests only if you introduce a test harness; otherwise validate by building with `make` and manually checking the DLL in TongDaXin using the formula shown in `README.md`. Treat compiler warnings, linker errors, and changed exported behavior as release blockers.

## Commit & Pull Request Guidelines

Git history uses short, imperative messages such as `bugfix`, `fix small bug`, and `Update README.md`. Prefer a more specific variant in the same style, for example `fix centroid boundary update` or `document dll install path`.

Pull requests should include a concise description, affected files or behavior, validation performed (`make`, manual TongDaXin check), and screenshots only when chart rendering changes.

## Security & Configuration Tips

Do not commit local TongDaXin installation paths, private market data, or generated debug artifacts. `CZSC.dll` is a build output; replace it only when intentionally publishing a new plugin binary.
