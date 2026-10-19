# M9 前置：旧 Phase E 设计清退记录

日期：2026-10-19

## 目标

把 Phase E 的“警戒 + 三类分队 + 资源锁 + 波次刷点”彻底移出运行时，只保留 M1–M8 已经验收的生态个体路线。清退不删除 Git 历史，旧 uasset 统一归档。

## 删除的运行时

- 敌人：`ASlimeNormal`、`ASlimeAggro`、`ASlimeAIController`、`USlimeStateTreeAIComponent`、`SlimeStateTreeNodes`、旧 `USlimeFusionComponent`。
- 刷点：`ASpawnPoint`、`USlimeEnemyManagerSubsystem`、`SlimeSpawnLayoutEdit`、`USlimeSpawnSlotMarker`、`SlimeSlotResolver` 及其编辑器 Details/Visualizer。
- 状态：`USlimeStateComponent` 与 `State.Enemy.Normal.* / State.Enemy.Aggro.*` 标签。
- 武器：旧连发 `UGA_Fire`；M6 的 `UGA_ChargeFire` 保留。
- 契约：`ETargetKind`、`ESlimeSlotRole`、`FSlimeAggroStatRow`、`FSlimeSpawnSlot`、`FSlimeSpawnPointDef`；`IBattleDirector::OnPointStateChanged` 删除。
- Flow：`URunSubsystem` 的批次、预警、点位快照和 `ASpawnPoint` 集合；`ASlimeRunGameState` 的点位/批次镜像。

## 保留的兼容壳

为不破坏另一位程序已经完成的 `WBP_SlimeHUD` / `WBP_SlimeResult`：

- `ESpawnPointState` 暂留为 deprecated 类型。
- `USlimeHUDWidget::OnPointStateUpdated` / `OnBatchIncoming` 暂留为无运行时来源的 BlueprintImplementableEvent。
- `FSlimeRunResult::NormalKills` / `ClearedPoints` 暂留字段，当前不再产生新数据。

这些兼容壳不参与任何生态逻辑，M9 重建 HUD 绑定后删除。

## 数据与资产

- `USlimeSpawnLayout` 只保留 `DropPoints`，类名与 `DA_SpawnLayout_DoublePoint` 路径保持不变，避免破坏准备页/投放流程。
- `USlimeRunConfig` 删除 AI/Normal、AI/Aggro、旧 Fusion、Spawn 批次和 `bEcologyRun` 字段；`SpawnLayout` 继续只承载落点。
- 归档到 `Content/Developers/Junk/LegacyPhaseE/`：
  - `Enemy/ST_SlimeNormal.uasset`
  - `Enemy/ST_SlimeAggro.uasset`
  - `Enemy/Data/DT_AggroStats.uasset` / `.csv`
  - `Core/Data/DA_SpawnLayout.uasset`
  - `Core/Data/DA_RunConfig.uasset`
  - `Maps/L_Sandbox_OnePoint.umap`
- `Config/DefaultGame.ini` 的全局 `RunConfig` 改为 `DA_RunConfig_DoublePoint`。
- `DA_RunConfig_DoublePoint`、`DA_SpawnLayout_DoublePoint`、`DA_Level_DoublePoint` 已用 Python 重存，清除已删除字段的序列化残留。

## 调试命令调整

- `SlimeSpawnNormalAtMass` 现在生成 `ASlimeEcologyEnemy`（A 阵营）。
- `SlimeSetMass` 现在作用于最近的生态个体。
- `SlimeForceFuse` 删除；融合测试统一使用 `SlimeEcologyForceFuse`。
- `SlimeRunStatus` 改为输出生态站点、注册个体、待生成和交付数据。

## 编译

```text
Build.bat SlimeWarEditor Win64 Development -MaxParallelActions=4
Result: 0 error
```

## 自动化

`Config/DefaultEngine.ini` 里原有的 `+ExcludeTest=(...,Test="SlimeWar",...)` 会跳过全部 SlimeWar 自动化测试，已移除，改为普通注释。

```text
Automation RunTests SlimeWar.Flow
Found 4 automation tests
DropPointSelection: 成功
RunPhase: 成功
RunResult: 成功
ScoringRules: 成功
4 tests performed
```

日志：`Saved/Logs/LegacyCleanup-Automation.log`

## 可见窗口回归

命令：

```text
UnrealEditor.exe SlimeWar.uproject /Game/_SlimeWar/Maps/L_Ecology_DoublePoint -game -windowed -ExecCmds="SlimeRunTimeScale 0,SlimeRunStart,SlimeEcologyStatus,SlimeEcologyResourceLedgerCheck,SlimeEcologySoakTest 12"
```

日志：`Saved/Logs/LegacyCleanup-Soak.log`
截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00086.png`

关键结果：

```text
StatTableProvider: loaded run config DA_RunConfig_DoublePoint
URunSubsystem: run started - ecology mode, 150 s total.
EcologyLedger[begin run]: resource OK (18 ...); mass OK (20 ...)
EcologySummary: live=20 registered=20 penetrations=0 stuck>2s=0 pathFailures=0
EcologyLedger[periodic]: resource OK (17 stock + 3 carried ...); mass OK (20 initial ...)
EcologySummary: live=15 registered=15 penetrations=0 stuck>2s=0 pathFailures=0
```

截图确认 HUD 不再显示 `POINT` / `batch`，改为 `SITE A/B`、注册个体数和资源/体量账本。

## 备注

- 第一次回归额外执行了 `SlimeSpawnNormalAtMass 4 2` 与 `SlimeSetMass 3`，因此日志中的体量账本出现 `20 initial vs 27 live`。这是调试注入未登记初始体量导致的预期现象；无注入的 12 秒 soak 两项账本均 OK。
- `CleanLegacySpawnActors.py` 已加入，但 `L_Whitebox_CityPlaza` 二进制中没有旧刷点/旧敌人引用；旧沙盒地图已整体归档。
- 旧 StateTree 模块依赖已从 `SlimeWar.Build.cs` 移除。
