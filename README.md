# 简介

缠论可视化交易插件

# 风险警示

本软件仅旨在实现最贴近缠师原文所的线段、中枢概念的可视化结果，软件免费分享使用，没有任何限制。

市场有风险，使用者自行承担任何由本软件而导致的买卖交易后果，作者本人不承担因为使用本软件而导致的任何直接或间接后果。

# 安装方法

目前仅支持通达信软件使用。安装时，首先将CZSC.dll复制到通达信安装目录下的T0002\dlls目录之中，并在通达信公式管理器中将本dll加载到1号dll插件之中。

# 编译方法

通达信插件需要 32 位 Windows DLL。推荐在 WSL2 中使用 MinGW-w64 交叉编译：

```bash
sudo apt update
sudo apt install make g++ mingw-w64 gcc-mingw-w64-i686 g++-mingw-w64-i686 binutils-mingw-w64-i686
make mingw32
```

也可以直接运行仓库脚本安装工具链：

```bash
sh scripts/bootstrap-wsl-mingw.sh
```

运行核心回归测试：

```bash
make mingw32-test
```

在 WSL2 中，`make mingw32-test` 只适合有 Wine 或 Windows 执行环境时使用，因为它产出的是 Windows PE 可执行文件。常规回归测试使用 WSL 原生 `g++`：

```bash
make test
```

检查工具链、运行测试并构建 DLL：

```bash
sh scripts/build-mingw32.sh
```

# 通达信端代码

```text
DLL:=TDXDLL1(1,H,L,5);
SEG:=TDXDLL1(9,H,L,5);
HIB:=TDXDLL1(2,DLL,H,L);
LOB:=TDXDLL1(3,DLL,H,L);
SIG:=TDXDLL1(4,DLL,H,L);
BSP:=TDXDLL1(5,DLL,H,L);
QLT:=TDXDLL1(10,DLL,H,L);
SLP:=TDXDLL1(8,DLL,H,L);
IF(HIB,HIB,DRAWNULL), COLORYELLOW;
IF(LOB,LOB,DRAWNULL), COLORYELLOW;
STICKLINE(SIG,LOB,HIB,0,0), COLORYELLOW;
DRAWLINE(DLL=-1,L,DLL=+1,H,0), COLORYELLOW;
DRAWLINE(DLL=+1,H,DLL=-1,L,0), COLORYELLOW;
DRAWLINE(SEG=-1,L,SEG=+1,H,0), COLORRED;
DRAWLINE(SEG=+1,H,SEG=-1,L,0), COLORRED;
DRAWNUMBER(DLL=+1,H,SLP), COLORYELLOW, DRAWABOVE;
DRAWNUMBER(DLL=-1,L,SLP), COLORYELLOW;
BUY(BSP=1,LOW);
BUY(BSP=3,LOW);
SELL(BSP=12,HIGH);
BUYSHORT(BSP=2,LOW);
SELLSHORT(BSP=11,HIGH);
SELLSHORT(BSP=13,HIGH);
DRAWICON(BSP>0 AND QLT=2,LOW,1);
```

其中 `QLT`（10 号函数）输出每个买卖点的信号质量：`0` 观察、`1` 确认、`2` 标准背驰。
`2` 表示价格背驰之外还得到 MACD 柱子面积缩小的动力学确认（第24课标准背驰），可据此重点标注最强的买卖点。
