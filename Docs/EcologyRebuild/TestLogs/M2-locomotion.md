# M2 生态移动与避让验收记录

- 日期：2026-10-18
- 分支：`rebulid-program`
- 状态：`Passed`
- 目标：验证连续导航、本地排斥、聚群、探索和玩家避让。

## 编译

```text
Build.bat SlimeWarEditor Win64 Development -Project="E:\UE\Unreal Projects\SlimeWar\SlimeWar\SlimeWar.uproject"
```

结果：编译和链接成功，0 error。

## 数据资产

- `DA_EcologyRules`：`/Game/_SlimeWar/Ecology/DA_EcologyRules`
- 已绑定 `DA_RunConfig::EcologyRules`
- 启动日志：`StatTableProvider: loaded run config DA_RunConfig (..., ecology rules: DA_EcologyRules)`
- 创建工具：`Tools/CreateEcologyRulesAsset.py`

## 60 秒 Soak

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeSpawnEcology 10,SlimeEcologySoakTest 60
日志：Saved/Logs/M2-Soak-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00021.png
```

最终汇总：

```text
live=10 registered=10 rebuilds=240 visitedCells=25
avgGroupDistance=232.7 minPairDistance=40.0 expectedGap=65.0
penetrations=0 stuck>2s=0 maxStuck=0.17
pathFailures=0 pathRequests=550 refreshMax=233
```

结论：

- 10 只全部存活。
- 无静态几何穿透。
- 没有超过 2 秒且仍持有目标的永久停顿。
- 最近中心距 40.0 cm，不小于两只体量 1 的半径和，未发生重叠。
- 群体平均距离 232.7 cm，小于 280 cm。
- 空间哈希约 4 次/秒重建；单只感知刷新约 4 次/秒，没有逐帧全表扫描。
- 60 秒内路径请求 550 次，平均约 0.92 次/秒/只。

## 玩家避让

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeSpawnEcology 10,SlimeEcologyAvoidanceTest
日志：Saved/Logs/M2-Avoidance-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00024.png
```

流程：

1. 生成 10 只生态史莱姆并等待 2 秒稳定。
2. 选取最外层主个体，将其放到群体外侧空地。
3. 玩家放在主个体与群体之间，距主个体 180 cm。
4. 等待 0.6 秒感知，确认主个体进入 `AvoidPlayer`。
5. 测量 4 秒退出窗口。
6. 玩家移开 700 cm，等待 10 秒恢复。

结果：

```text
primary entry distance=180 cm
4 s result=1 / 1 outside 320 cm
minimum subject distance=341.1 cm
recovery avgGroupDistance=211.2 cm
penetrations=0 stuck>2s=0
```

结论：玩家避让、退出距离和后续恢复均通过。

## 调试命令

- `SlimeSpawnEcology 10`
- `SlimeEcologyStatus`
- `SlimeEcologySoakTest 60`
- `SlimeEcologyAvoidanceTest`

## 说明

密集群体中 25 cm 额外间距是软排斥目标，不保证任意帧都达到 65 cm 中心距；硬门禁仍是不得小于双方碰撞半径和。最终 soak 的最小中心距为 40.0 cm，未重叠。

M2 完成后停在此处，未开始 M3。
