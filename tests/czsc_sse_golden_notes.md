# SSE Golden Notes

`czsc_sse_result.txt` is generated output. Keep manual review notes in this
file or in C++ assertions so regenerating the diagnostic output does not erase
human annotations.

Current manually reviewed stroke-center anchors. These are matched by
date/range in tests, not by ordinal id, because emitting all same-level centers
can shift later generated labels.

`czsc_sse_result.txt` now contains five real-data samples:

- `全量日线`: the complete 000001.SH daily fixture.
- `2018-2019 下跌修复切片`: the early falling/recovery window.
- `2020-2021 上涨震荡切片`: the strongest rising/choppy window in the fixture.
- `2022-2023 下跌盘整切片`: the falling/consolidating window.
- `2024-2026 震荡上行切片`: the recent choppy rising window.

| id | direction | start | end | ZG | ZD |
| --- | --- | --- | --- | --- | --- |
| BZ00 | 上升 | 2018-02-26 | 2018-07-06 | 3129 | 3091 |
| BZ01 | 上升 | 2018-07-12 | 2018-11-30 | 2676.48 | 2653.11 |
| BZ02 | 下降 | 2019-01-04 | 2019-05-10 | 3125.02 | 2987.77 |
| BZ03 | 上升 | 2019-05-17 | 2020-03-19 | 2922.91 | 2891.54 |
| BZ04 | 上升 | 2020-04-10 | 2020-07-09 | 2833.02 | 2802.47 |
| BZ05 | 下降 | 2020-07-27 | 2021-01-25 | 3350.59 | 3325.17 |

Current manually reviewed line-segment center anchors. These are generated with
`nCenterUnit = CZSC_UNIT_SEGMENT` and `nSegmentMethod = CZSC_SEG_FEATURE`.

| id | direction | start | end | ZG | ZD |
| --- | --- | --- | --- | --- | --- |
| SZ00 | 上升 | 2018-04-11 | 2019-04-08 | 2703.51 | 2449.20 |
| SZ01 | 下降 | 2019-06-06 | 2020-08-18 | 3042.93 | 2822.19 |
| SZ02 | 下降 | 2020-09-25 | 2023-06-26 | 3418.95 | 3344.97 |
| SZ03 | 上升 | 2023-08-04 | 2025-11-14 | 3174.27 | 3040.69 |

## Center Lifecycle Audit

The lifecycle rows implement lessons 17/18/20:

- `[ZD,ZG]` overlap is same-level center extension.
- Full `GG/DD` overlap without `[ZD,ZG]` overlap is higher-level expansion.
- Full non-overlap is same-level trend newborn up/down.

Current five-sample lifecycle summaries:

| sample | stroke extension | stroke expansion | stroke newborn up | stroke newborn down | segment extension | segment expansion | segment newborn up | segment newborn down |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 全量日线 | 1 | 16 | 0 | 0 | 0 | 1 | 0 | 0 |
| 2018-2019 下跌修复切片 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2020-2021 上涨震荡切片 | 0 | 4 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2022-2023 下跌盘整切片 | 0 | 3 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2024-2026 震荡上行切片 | 1 | 4 | 0 | 0 | 0 | 0 | 0 | 0 |

The current real samples are dominated by expansion rather than newborn trend
relations. This is an observation checkpoint, not an unfinished algorithmic
requirement. Future lifecycle changes should first add a sample where full
`GG/DD` non-overlap is expected, then prove the newborn up/down count changes.

## Seventh-Round Sample Audit

The latest generated `czsc_sse_result.txt` adds three machine-checked sample
summaries:

- no-trend filter attribution for `原因1 无趋势`;
- trend-structure completion summaries for stroke centers and segment centers;
- nested-divergence semantic summaries recalculated from context rows.

Current no-trend attribution by sample:

| sample | stroke early centers | stroke same-direction trend missing | stroke outside completed trend | segment early centers | segment same-direction trend missing | segment outside completed trend |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 全量日线 | 12 | 142 | 0 | 8 | 3 | 0 |
| 2018-2019 下跌修复切片 | 12 | 22 | 0 | 0 | 0 | 0 |
| 2020-2021 上涨震荡切片 | 6 | 34 | 0 | 3 | 0 | 0 |
| 2022-2023 下跌盘整切片 | 15 | 18 | 0 | 0 | 0 | 0 |
| 2024-2026 震荡上行切片 | 12 | 24 | 0 | 0 | 0 | 0 |

Current trend-structure summaries:

| sample | stroke up | stroke down | stroke consolidation | segment up | segment down | segment consolidation |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| 全量日线 | 0 | 0 | 18 | 0 | 0 | 2 |
| 2018-2019 下跌修复切片 | 0 | 0 | 4 | 0 | 0 | 0 |
| 2020-2021 上涨震荡切片 | 0 | 0 | 5 | 0 | 0 | 1 |
| 2022-2023 下跌盘整切片 | 0 | 0 | 4 | 0 | 0 | 0 |
| 2024-2026 震荡上行切片 | 0 | 0 | 6 | 0 | 0 | 0 |

All generated trend summary sections currently report `invalid_center=0` and
`invalid_span=0`. This means the emitted structures are internally well formed,
but the default five-sample SSE matrix still does not include a real same-level
trend newborn case.

Current nested-divergence semantic summaries are all zero:

| sample | total | trend divergence | consolidation divergence | small-turn | new extreme | buy | sell |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 全量日线 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2018-2019 下跌修复切片 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2020-2021 上涨震荡切片 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2022-2023 下跌盘整切片 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |
| 2024-2026 震荡上行切片 | 0 | 0 | 0 | 0 | 0 | 0 | 0 |

The absence of nested-divergence contexts in these samples is a data coverage
gap. Future changes should add a real or minimal fixture that produces at
least one context row before changing nested-divergence defaults.

Current manually reviewed trading-signal anchors. These lock the generated
candidate date, signal type, quality, center/point/breakout ids, position,
movement type, aftermath, and context flags in C++ tests.
The generated `czsc_sse_result.txt` candidate lines also include raw A/C
strengths, C/A percent ratios, and divergence condition flags for manual
P4动力学核对, plus `BLP`/`BRP` and `bko[...]` leave/retest context for checking 第20课首次离开/首次回试;
those diagnostics are generated, not edited here.

Machine-checked structural invariants in `tests/check_sse_result.py` for each
generated sample:

- Every generated line-segment endpoint must also appear in the stroke endpoint
  section with the same date, type, and price.
- The generated line-segment endpoint count must be lower than the stroke
  endpoint count.
- Candidate debug ids must keep ordered divergence A/C endpoints, second-signal
  `SFP < SMP < PID`, small-turn `STF < STL < STR`, and third-signal
  `BLP < BRP == PID`.
- Breakout debug ids must match the rendered `bko[...]` leave/retest context.
- Filter summaries must include 1-8 reasons with example dates, and no-trend
  attribution summaries must reconcile early-center, same-direction-trend, and
  outside-completed-trend counts.
- Trend summaries must reconcile up/down/consolidation counts and keep
  `invalid_center=0`, `invalid_span=0`.
- Nested semantic summaries must be recalculated from the context rows and keep
  total, new-extreme, buy, and sell counts consistent.

## Morphology Dispute Notes

These notes are the manual gate before changing the default feature-sequence
line-segment algorithm. Current automatic checks cover the lesson 62/65 minimum
geometry and endpoint ownership, but they do not prove every disputed
feature-sequence boundary is the only valid reading.

Full-sample line-segment spans measured against the stroke endpoints:

| segment edge | stroke edge | span in strokes | review meaning |
| --- | --- | ---: | --- |
| L001 2018-02-09 -> L002 2018-11-19 | B001 -> B016 | 15 | large first segment, stable anchor |
| L003 2019-06-06 -> L004 2019-09-16 | B029 -> B034 | 5 | medium segment, above minimum |
| L009 2021-07-28 -> L010 2021-09-14 | B073 -> B076 | 3 | exact minimum legal line segment |
| L012 2023-05-09 -> L013 2023-06-26 | B106 -> B109 | 3 | exact minimum legal line segment |
| L014 2024-05-20 -> L015 2025-04-07 | B122 -> B135 | 13 | latest long anchor before current sample tail |

Before changing `BuildLineSegmentPointsByFeature`, add an explicit row here
for the disputed edge with:

- current L id/date pair and matching B id/date pair;
- whether the first three strokes still overlap;
- whether a feature-sequence gap exists;
- if there is a gap, the reverse feature-sequence fractal date used to confirm
  the boundary.

Without that annotation, the current minimum-span edges should be treated as
known-good regression anchors rather than as candidates for cleanup.

| unit | date | signal | quality | center | movement | point | breakout | position | aftermath | ctx |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 笔中枢 | 2018-07-12 | 三卖 | 1 | 0 | 盘整 | 9 | 0 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2018-12-13 | 三卖 | 2 | 1 | 盘整 | 17 | 1 | 下方 | 扩张 | 4225 |
| 笔中枢 | 2019-05-17 | 三卖 | 1 | 2 | 盘整 | 25 | 2 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2020-04-10 | 三卖 | 1 | 3 | 盘整 | 43 | 3 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2020-07-27 | 三买 | 1 | 4 | 盘整 | 48 | 4 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2021-01-29 | 三买 | 2 | 5 | 盘整 | 58 | 5 | 上方 | 扩张 | 4233 |
| 笔中枢 | 2021-06-18 | 三买 | 1 | 6 | 盘整 | 68 | 6 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2022-03-03 | 三卖 | 1 | 7 | 盘整 | 81 | 7 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2022-10-18 | 三卖 | 1 | 8 | 盘整 | 93 | 8 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2023-02-17 | 三买 | 2 | 9 | 盘整 | 100 | 9 | 上方 | 扩张 | 4225 |
| 笔中枢 | 2023-09-04 | 三卖 | 1 | 10 | 盘整 | 113 | 10 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2024-03-28 | 三买 | 1 | 11 | 盘整 | 120 | 11 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2024-10-16 | 三买 | 1 | 12 | 盘整 | 126 | 12 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2025-04-24 | 三卖 | 2 | 13 | 盘整 | 135 | 13 | 下方 | 扩张 | 4233 |
| 笔中枢 | 2025-09-04 | 三买 | 1 | 14 | 盘整 | 142 | 14 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2025-11-05 | 三买 | 2 | 15 | 盘整 | 146 | 15 | 上方 | 扩张 | 4233 |
| 笔中枢 | 2026-02-03 | 三买 | 1 | 16 | 盘整 | 152 | 16 | 上方 | 扩张 | 4224 |
| 线段中枢 | 2019-06-06 | 三买 | 1 | 0 | 盘整 | 6 | 0 | 上方 | 扩张 | 4224 |
| 线段中枢 | 2020-09-25 | 三买 | 1 | 1 | 盘整 | 12 | 1 | 上方 | 扩张 | 4224 |
| 线段中枢 | 2023-08-04 | 三卖 | 1 | 2 | 盘整 | 23 | 2 | 下方 | 扩张 | 4224 |
| 线段中枢 | 2025-12-16 | 三买 | 2 | 3 | 盘整 | 30 | 3 | 上方 | - | 4105 |
