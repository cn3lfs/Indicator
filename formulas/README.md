# 缠论插件 极简使用指南

## 第一步：复制 DLL

把 `build` 文件夹里的 DLL 复制到通达信目录：

| 你的通达信 | 复制这个文件 |
|-----------|-------------|
| 32 位（老版本） | `build\CZSC.dll` |
| 64 位（新版本） | `build\CZSC64.dll` |

> 不知道位数？通达信菜单 → 帮助 → 关于，看有没有 "64位" 字样。
> 放错会加载失败，换个文件就行。

粘贴到：**通达信安装目录 `\T0002\dlls\`** 下面。

## 第二步：绑定 DLL

1. 打开通达信 → 顶部菜单 **功能** → **公式系统** → **公式管理器**
2. 点 **DLL 插件** 页签
3. 在 **插件 1** 那行点 **绑定**，选刚才复制进去的 DLL 文件
4. 确定

## 第三步：导入公式

还是在公式管理器里：

**主图公式**（看盘用）：
- 点 **技术指标公式** → **新建**
- 公式名称写 `缠论主图`
- 画线方法选 **主图叠加**
- 把 `chan-main.txt` 的内容复制粘贴进去；旧名 `chan-all-buys.txt` 保持兼容，内容相同
- 点 **确定**

**调试公式**（核对中枢关系、信号质量、背驰段用）：
- 仍然点 **技术指标公式** → **新建**
- 公式名称写 `缠论调试`
- 画线方法选 **主图叠加**
- 把 `chan-debug.txt` 的内容复制粘贴进去。`STB` 标记第44课小转大必要条件，三买为 `1`，三卖为 `-1`；`ABC` 标记第37课一类背驰的 A-B-C 结构，一买为 `1`，一卖为 `-1`；`MLW` 标记 MACD 黄白线高度走弱；`MZP` 标记 B 中枢 MACD 回零；`STD` 标记标准趋势背驰确认；`MAR/SPR/VPR` 标记胜出候选 C/A 段 MACD 柱面积比、价差力度比、平均力度比；`DVG` 是背驰要素位图；`CTX` 是胜出买卖点候选的上下文位图，含 `2048` 二三类重合与 `4096` 关联中枢首次离开/回试；`CLF/LCF` 标记胜出候选所属中枢生命周期、相邻中枢生命周期；`POS/MOV/PRI/CEN/BKO/PID/TID/BLP/BRP/ABK/ABL/ABR/STL/STR/STF/SFP/SMP/APS/APE/CPS/CPE` 标记候选相对中枢位置、所属走势类型、优先级、所属中枢编号、突破编号、端点编号、走势编号、突破离开端点编号、回试端点编号、ABC 关联突破编号与其离开/回试端点编号、小转大离开/回试端点编号、小转大关联一类端点编号、二类关联一类/中间反向端点编号、背驰 A/C 段起止端点编号；`NLV/NSR/NSP/NEP` 标记区间套层级、源高级别背驰段编号、低级别背驰段起止端点编号；蓝线 `BSG` 为第27课一类背驰段，青线 `NST` 为第27/61课区间套背驰段

**选股公式（选股用，默认推荐）**：
- 点 **条件选股公式** → **新建**
- 核心三类：`chan-first-buy.txt`、`chan-second-buy.txt`、`chan-third-buy.txt`
- 核心三类卖点：`chan-first-sell.txt`、`chan-second-sell.txt`、`chan-third-sell.txt`
- 二买/二卖公式默认要求第21课“一类后第二段次级别走势”端点可定位
- 高级选股公式：`chan-first-buy-standard.txt`、`chan-first-sell-standard.txt`、`chan-first-buy-dynamics.txt`、`chan-first-sell-dynamics.txt`
- 小转大与上下文：`chan-small-turn-buy.txt`、`chan-small-turn-sell.txt`、`chan-first-buy-context.txt`、`chan-first-sell-context.txt`
- 多级别共振：`chan-multi-buy.txt`
  （该公式会分别注册日线与 30 分钟 C/V，保证两级别动力学输出都优先使用真实收盘价）
- 兼容/细分公式按需导入：`chan-first-buy-original.txt`、`chan-first-sell-original.txt`、`chan-first-buy-abc.txt`、`chan-first-sell-abc.txt`
- 三买/三卖细分公式按需导入：`chan-third-buy-strong.txt`、`chan-third-sell-strong.txt`、`chan-third-buy-original.txt`、`chan-third-sell-original.txt`
- 三买/三卖后续与重合公式按需导入：`chan-third-buy-expanded.txt`、`chan-third-sell-expanded.txt`、`chan-third-buy-newborn.txt`、`chan-third-sell-newborn.txt`、`chan-overlap-buy.txt`、`chan-overlap-sell.txt`
- 每次粘贴后点确定

## 第四步：使用

- **看盘**：在 K 线图键盘输入 `缠论主图` 回车，就能看到笔、线段、买卖点
- **选股**：功能 → 选股器 → 条件选股 → 选 `缠论一买`（或二买/三买）→ 加入条件 → 执行选股

## 常见问题

**Q: DLL 加载失败？**
A: 检查位数是否匹配（32位通信用 CZSC.dll，64位用 CZSC64.dll），文件是否在 `T0002\dlls\` 目录下。

**Q: 图上没东西？**
A: 确保 K 线数据完整（先下载盘后数据），切换到日线周期。

**Q: 选不出股票？**
A: 信号只保留 10 天内的，10 天前的不会选中。如果市场差可能确实没有买点。

**Q: 旧公式还能用吗？**
A: 可以。`DLL:=TDXDLL1(1,H,L,0); BSP:=TDXDLL1(5,DLL,H,L);` 仍然兼容；新公式默认用 `TDXDLL1(30,H,L,40)` 一步输出买卖点。
