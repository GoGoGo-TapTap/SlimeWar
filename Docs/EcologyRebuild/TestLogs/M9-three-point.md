# M9 三阵营 60×50 生态关卡测试记录

日期：2026-10-19

## 内容

- 新增 `L_Ecology_ThreePoint`：60×50 米、A/B/C 三阵营、5 个矿点、180 秒/160 分。
- 新增 `DA_RunConfig_ThreePoint`、`DA_SpawnLayout_ThreePoint`、`DA_Level_ThreePoint`。
- 保留 `L_Ecology_DoublePoint` 作为回归关卡。
- 现有 WBP 未修改；HUD 新增可选 `OnEcologySitesUpdated`。

## 编译

```text
Build.bat SlimeWarEditor Win64 Development -MaxParallelActions=4
Result: 0 error
```

## 自动化

```text
Automation RunTests SlimeWar.Flow
Found 4 automation tests
DropPointSelection: 成功
RunPhase: 成功
RunResult: 成功
ScoringRules: 成功
```

日志：`Saved/Logs/M9-Automation.log`

## 地图与导航

实际坐标：

```text
A (-1000,-1300)  B (-1000,1300)  C (1500,0)
D1 (-1800,-2200) D2 (1800,2200)
A_Near (-1400,-1300) 4/4
B_Near (-1400,1300)  4/4
C_Near (1900,0)      4/4
Center_North (0,700) 5/6
Center_South (0,-700) 5/6
```

实现过程中发现并修复：

- 地板 X 尺寸第一次误写成 25 米，C 点和 A_Near 在碰撞外，导致个体坠落；改为 50×60 米后地面探测全部命中。
- 命令行 `RebuildNavigation` 不会完成 Recast 烘焙；新增编辑器控制台 `SlimeBakeNav`，在真实编辑器 tick 中构建并保存。
- `SlimeEcologyGroundProbe` 在游戏内验证 3 站点 + 5 矿点，全部 `hit=yes`、`nav=yes`。

地面/导航探测日志：`Saved/Logs/M9-GroundProbe3.log`、`Saved/Logs/M9-NavProbe5.log`

## 60 秒 soak

命令：

```text
UnrealEditor.exe SlimeWar.uproject /Game/_SlimeWar/Maps/L_Ecology_ThreePoint -game -windowed
-ExecCmds="SlimeRunTimeScale 0,SlimeRunStart,SlimeEcologyStatus,SlimeEcologyResourceLedgerCheck,SlimeEcologySoakTest 60"
```

日志：`Saved/Logs/M9-ThreePoint-Final.log`
截图：`HighresScreenshot00093.png`

关键结果：

```text
EcologyLedger[begin run]: resource OK (22 ...); mass OK (30 initial = 30 live)
EcologySummary: live=30 registered=30 penetrations=0 stuck>2s=0 pathFailures=0
EcologySummary: live=39 registered=39 penetrations=0 stuck>2s=0 pathFailures=0
SoakTest: finished after 60.0 s; path requests during soak=730
EcologyLedger[periodic]: resource OK (...); mass OK (30 initial + 35 delivered = 65 live + 0 removed)
```

HUD 截图显示三站点、五个矿点、`TIME 180`、`SCORE 0 / 160`。

## C 阵营同化

命令：

```text
SlimeGrenadeAssimilationTest 2
```

日志：`Saved/Logs/M9-AssimilationC.log`
截图：`HighresScreenshot00094.png`

```text
GrenadeAssimilation: faction 2 affected 3 slime(s)
SlimeGrenadeTest: targetFaction=2 faction=3 assigned=3
carrier=... faction=2 site=C brain=ReturnCargo goal=V(X=1500.00, Z=2.00) cargo=1
```

## 结算字段

命令：

```text
SlimeRunTimeScale 0
SlimeRunStart
SlimeKillPlayer
SlimeRunResult
```

日志：`Saved/Logs/M9-ResultStats.log`

```text
end reason PlayerDied
score 0 / target 160
player kills 0   npc kills 0 ...
elapsed 0.0 s   player damage taken 100.0
```

## 双点回归

日志：`Saved/Logs/M9-DoublePoint-Regression2.log`

```text
EcologyLedger[begin run]: resource OK (18 ...); mass OK (20 ...)
EcologySummary: live=14 registered=14 penetrations=0 stuck>2s=0 pathFailures=0
SoakTest: finished after 12.0 s; path requests during soak=27
```

双点关卡 C 测试被正确拒绝：

日志：`Saved/Logs/M9-DoublePoint-CWarning.log`

```text
SlimeGrenadeAssimilationTest: faction 2 has no site in this level.
```

## 手动验收结果

用户已于 2026-10-19 完成 `Docs/EcologyRebuild/M9-Checklist.md`，全部项目通过：

- 三点地图地面与 NavMesh 探测：A/B/C 和 5 个矿点全部 `hit=yes nav=yes`。
- 30 只开局、三站点 HUD、180 秒/160 分。
- 采集/返程/交付、共享矿点再生、资源与体量账本。
- `Z/X/C` 阵营选择、手雷类型与库存、C 阵营同化及返程。
- 融合、警戒、敌对核心、跃击、吞噬和爆裂。
- 计分、三种终局、结算新字段。
- 现有 WBP 兼容和双点关卡回归。

M9 结论：`Passed`。

## 实现偏差

- 三点地图从双点地图复制以继承导航系统，再替换全部布局 Actor；没有新增 `DA_EcologyLayout`。
- 命令行的导航重建不可靠，因此保留 `SlimeBakeNav` 编辑器控制台命令；以后重建三点导航必须走真实编辑器会话。
- 现有 WBP 未修改；新站点事件和结算字段只保证 C++ 数据面可用。
