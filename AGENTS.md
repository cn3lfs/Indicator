# Repository Guidelines

## Project Structure

TongDaXin (TDX) CZSC (缠论) plugin, built as `build/CZSC.dll` (32-bit) and `build/CZSC64.dll` (64-bit).

- `core/`: pure domain engine in namespace `chan` — no TDX types, output buffers or globals.
  - `model.h` value types (merged bar, fractal, pivot, center, movement, signal, event), `config.h`, `series.h`.
  - `morphology` (inclusion, fractals, strokes, segments), `structure` (centers, relations, movements),
    `dynamics` (MACD tables, strength, divergence, MA kisses), `signals` (first/second/third class, breakouts),
    `engine` (`Analyze`: causal step-by-step analysis producing a final snapshot plus appear/revoke events).
  - Each derived layer is a resumable stream (`StrokeStream`, `SegmentStream`, `CenterStream`, `SignalStream`);
    the batch function is “feed everything from an empty state”, so batch and incremental share one code path.
- `tdx/`: thin adapter projecting `chan::Analysis` into per-bar series; owns the analysis cache and the
  close/volume registration (function 40). `Main.cpp` registers the function table.
- `FxIndicator.h` / `Main.h`: TDX plugin ABI (`RegisterTdxFunc`, `pack(1)`, cdecl). Do not change.
- `formulas/`: TDX formulas; `tests/check_formulas.py` validates them against `Main.cpp`.
- `tests/unit/`: self-registering tests (`TEST`/`CHECK`), SSE sample data and `golden/sse.txt`.
- `docs/chan-ambiguity-decisions.md`: the chosen rule wherever the lessons leave a boundary open.

## Build, Test, and Development Commands

- `make test`: build and run `tests/unit/ChanTests` (native g++) plus the formula check.
- `make golden`: regenerate `tests/unit/golden/sse.txt` after an intended algorithm change; review the diff.
- `make release`: 32/64-bit DLLs via MinGW (WSL2) and `release-check` (PE type, imports only
  KERNEL32/msvcrt, zero timestamp). Toolchain: `sh scripts/bootstrap-wsl-mingw.sh`.
- `ChanTests <substring>` runs only matching test cases.

## Coding Style

C++17, two-space indentation, braces on their own lines for functions/classes, value types and free
functions over class hierarchies, `lowerCamel` locals/fields, `PascalCase` functions/types, trailing `_`
for private members. Comments are Chinese and cite lesson numbers (e.g. “第20课”) for every rule taken
from the text; boundaries the text does not define must be listed in `docs/chan-ambiguity-decisions.md`.

## Testing Guidelines

Three kinds of tests: lesson rules on hand-built inputs, invariants (alternating fractals, price progress,
segments as a subset of strokes, every prefix analysis equals the full analysis up to that bar,
incremental engine equals `AnalyzeReference`), and the SSE golden. A change to a stream must keep
`IncrementalEngineMatchesReference` and `EngineIsCausalOnEveryPrefix` passing. Existing assertions are not
authorities on the lessons: when they conflict with the text, follow the text and update them.

## Commit & Pull Request Guidelines

Conventional-style messages (`feat(core): ...`, `fix(tdx): ...`, `perf(...)`, `docs: ...`), one feature per
commit, body explaining the lesson basis and observable effect (e.g. SSE counts). Mark interface breaks with `!`.

## Security & Configuration Tips

Do not commit local TongDaXin paths, private market data or debug artifacts; `build/` is ignored.
