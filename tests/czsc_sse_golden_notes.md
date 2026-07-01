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
| BZ01 | 上升 | 2018-07-12 | 2018-11-12 | 2676 | 2653 |
| BZ02 | 上升 | 2018-11-19 | 2019-01-04 | 2646 | 2555 |
| BZ03 | 上升 | 2019-03-07 | 2019-04-08 | 3125 | 2988 |
| BZ04 | 下降 | 2019-05-10 | 2019-06-06 | 2922.91 | 2838.38 |
| BZ05 | 下降 | 2020-03-19 | 2020-07-09 | 2833.02 | 2802.47 |

Current manually reviewed line-segment center anchors. These are generated with
`nCenterUnit = CZSC_UNIT_SEGMENT` and `nSegmentMethod = CZSC_SEG_FEATURE`.

| id | direction | start | end | ZG | ZD |
| --- | --- | --- | --- | --- | --- |
| SZ00 | 上升 | 2018-04-11 | 2019-01-04 | 2703.51 | 2449.20 |
| SZ01 | 上升 | 2019-04-08 | 2020-02-04 | 3042.93 | 2822.19 |
| SZ02 | 下降 | 2020-04-16 | 2021-06-02 | 3456.72 | 3344.97 |
| SZ03 | 下降 | 2021-07-28 | 2024-02-05 | 3315.05 | 3312.72 |
| SZ04 | 上升 | 2024-05-20 | 2025-11-14 | 3174.27 | 3040.69 |

Current manually reviewed trading-signal anchors. These lock the generated
candidate date, signal type, quality, center/point/breakout ids, position,
movement type, aftermath, and context flags in C++ tests.

| unit | date | signal | quality | center | movement | point | breakout | position | aftermath | ctx |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 笔中枢 | 2018-07-12 | 三卖 | 1 | 0 | 盘整 | 9 | 0 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2020-07-27 | 三买 | 1 | 8 | 盘整 | 48 | 8 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2021-01-29 | 三买 | 2 | 10 | 盘整 | 58 | 10 | 上方 | 扩张 | 4233 |
| 笔中枢 | 2021-06-18 | 三买 | 1 | 12 | 盘整 | 68 | 12 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2022-04-07 | 三卖 | 2 | 15 | 盘整 | 83 | 15 | 下方 | 扩张 | 4233 |
| 笔中枢 | 2022-10-18 | 三卖 | 1 | 17 | 盘整 | 93 | 17 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2024-01-02 | 三卖 | 1 | 21 | 盘整 | 117 | 21 | 下方 | 扩张 | 4224 |
| 笔中枢 | 2024-10-16 | 三买 | 1 | 23 | 盘整 | 126 | 23 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2025-05-27 | 三买 | 2 | 25 | 盘整 | 138 | 25 | 上方 | 扩张 | 4225 |
| 笔中枢 | 2025-09-04 | 三买 | 1 | 26 | 盘整 | 142 | 26 | 上方 | 扩张 | 4224 |
| 笔中枢 | 2025-11-05 | 三买 | 2 | 27 | 盘整 | 146 | 27 | 上方 | 扩张 | 4233 |
| 线段中枢 | 2024-05-20 | 三卖 | 1 | 3 | 盘整 | 25 | 3 | 下方 | 扩张 | 4224 |
| 线段中枢 | 2025-12-16 | 三买 | 2 | 4 | 盘整 | 30 | 4 | 上方 | - | 4105 |
