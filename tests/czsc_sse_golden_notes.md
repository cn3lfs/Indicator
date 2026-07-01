# SSE Golden Notes

`czsc_sse_result.txt` is generated output. Keep manual review notes in this
file or in C++ assertions so regenerating the diagnostic output does not erase
human annotations.

Current manually reviewed stroke-center anchors. These are matched by
date/range in tests, not by ordinal id, because emitting all same-level centers
can shift later generated labels.

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

Current manually reviewed trading-signal anchors. These lock the generated
candidate date, signal type, quality, center/point/breakout ids, position,
movement type, aftermath, and context flags in C++ tests.
The generated `czsc_sse_result.txt` candidate lines also include raw A/C
strengths, C/A percent ratios, and divergence condition flags for manual
P4动力学核对; those numeric diagnostics are generated, not edited here.

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
