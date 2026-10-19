# M7 双点生态关卡自动测试记录

- 日期：2026-10-19
- 分支：`rebulid-program`
- 状态：`Passed`

## 编译

`SlimeWarEditor Win64 Development`：0 error。

## 内容资产

- 地图：`/Game/_SlimeWar/Maps/L_Ecology_DoublePoint`
- RunConfig：`/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint`
- SpawnLayout：`/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint`
- LevelDefinition：`/Game/_SlimeWar/Core/Data/Levels/DA_Level_DoublePoint`

地图范围 X `[-1500,1500]`、Y `[-2200,2200]`，包含 A/B 站点、4 个矿点、D1/D2、外墙、分隔墙、柱子和货架。

## 初始状态

日志：`Saved/Logs/M7-Resources2.log`

```text
EcologyLedger[begin run]:
resource OK (18 stock + 0 carried + 0 ground + 0 delivered = 18 initial + 0 regen)
mass OK (20 initial + 0 delivered = 20 live + 0 removed)
pending=0
```

结论：A/B 各 10 只体量 1，初始总库存 18。

## 30 秒资源循环

同一日志：

```text
EcologyLedger[periodic]:
resource OK (14 stock + 1 carried + 0 ground + 5 delivered = 18 initial + 2 regen)
mass OK (20 initial + 5 delivered = 25 live + 0 removed)
```

结论：

- 已完成 5 次采集交付。
- 每次交付只新增 1 只体量 1。
- 共享矿点再生 2 份。
- 资源与体量账本均平衡。

## 最终 150 秒回合

日志：`Saved/Logs/M7-FullRun-final.log`

```text
URunSubsystem: run started - ecology mode, 0 points, 150 s total.
URunSubsystem: run ended (TimeUp) after 150.0 s.
```

结束账本：

```text
resource OK (12 stock + 6 carried + 0 ground + 2 delivered = 18 initial + 2 regen)
mass OK (20 initial + 2 delivered = 22 live + 0 removed)
```

站点状态：

```text
A live=7 mass=11 delivered=1 cores=4
B live=7 mass=11 delivered=1 cores=4
```

截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00070.png`

## 矿点再生

- SouthShared / NorthShared 从初始 5 恢复到 6 后保持上限 6。
- WestNear / EastNear 库存只下降，不恢复。
- 周期账本中的 `RegeneratedStock` 与共享矿点恢复数一致。

## 携货掉落

日志：`Saved/Logs/M7-CargoDrop.log`

```text
EcologyCargo: dropped 1 at V(...)
SlimeEcologyCargoDropTest: killed ... carrying one cargo unit.
```

结论：携货个体死亡后生成 1 份地面货物。

说明：该日志来自货物掉落功能首次验证。后续已把调试命令改为从矿点库存取货，`M7-DeliveryFix.log` 用于确认调试命令不再破坏账本。

## 灭绝终局

日志：`Saved/Logs/M7-Extinction-final.log`

```text
URunSubsystem: run ended (Extinction) after 0.5 s.
resource OK (18 stock + 0 carried + 0 ground + 0 delivered = 18 initial + 0 regen)
mass OK (20 initial + 0 delivered = 0 live + 20 removed)
```

结论：无存活、无待生成、无地面货物时 0.5 秒内结束，不空等 150 秒。

## NavMesh

- NavMeshBoundsVolume 覆盖 X ±4000、Y ±5500、Z ±1000。
- 新建关卡首次运行缺少可查询导航数据。
- 在编辑器中执行 `RebuildNavigation` / `BuildPaths`，并将 RecastNavMesh 设置为 `RuntimeGeneration=Dynamic` 后，游戏运行时 NavMesh 投影成功，站点出生日志不再报 `nav` 失败。

## 待手动验收

详见 `Docs/EcologyRebuild/M7-Checklist.md`：主菜单进入、D1/D2 投放、真实资源运输观察、完整 150 秒一局和灭绝操作。

用户已于 2026-10-19 完成上述手动验收，M7 最终状态：`Passed`。

## 实机反馈修复复测

### 持续采集

日志：`Saved/Logs/M7-ResourceContinue.log`

60 秒正常时间观察结果：

```text
10 s: delivered 0, carried 6
20 s: delivered 8, carried 2
30 s: delivered 8, carried 11
40 s: delivered 15, carried 9
50 s: delivered 24, carried 3
60 s: delivered 34, carried 5
```

资源与体量账本在每个周期均为 OK。结论：不再出现“只有最初两只去采集”的问题。

### 调试交付账本

日志：`Saved/Logs/M7-DeliveryFix.log`

```text
resource OK (17 stock + 0 carried + 0 ground + 1 delivered = 18 initial + 0 regen)
mass OK (20 initial + 1 delivered = 21 live + 0 removed)
```

结论：调试交付从矿点库存取货，不再人为制造 BROKEN 账本。

### 中性核心连锁

日志：`Saved/Logs/M7-ChainFix.log`

```text
slime_0 爆炸
slime_1 40 -> 3，未死亡
bystander 20 -> 0
firstDead=yes secondDead=no
```

结论：中性核心不再递归连锁；聚集点位不会被一次击杀全部清空。

### 货物可视化

- 地面货物和携货个体均使用 `ASlimeCargoVisual`。
- 模型入口为 `CargoMeshAsset`，可替换 Blueprint/DataAsset 网格。
- 携货 Actor 会附着到携带者胶囊上方，掉落时切换为可拾取的 `ASlimeCargoPickup`。
