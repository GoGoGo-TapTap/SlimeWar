# R5 警戒、粘液与探索自动测试

- 日期：2026-10-19
- 状态：自动场景通过，手动 Checklist 待执行
- 地图：`/Game/_SlimeWar/Maps/L_Ecology_DoublePoint`

## 编译

```text
SlimeWarEditor Win64 Development
结果：0 error
```

## 粘液、警戒休息与探索

```text
命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyR5Test
日志：Saved/Logs/R5-Behavior-final2.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00112.png
      Saved/Screenshots/WindowsEditor/HighresScreenshot00113.png
```

关键日志：

```text
stage 0 behavior=SeekPaint paintCount=6 nearestPaint=70.7 explorationVisits=0 gatherRest=4.25
stage 1 behavior=SeekPaint paintCount=6 nearestPaint=49.6 explorationVisits=0 gatherRest=1.75
stage 2 behavior=AlertWait  paintCount=0 nearestPaint=-1.0 explorationVisits=0 gatherRest=1.34
stage 3 behavior=Explore   paintCount=0 nearestPaint=-1.0 explorationVisits=1 gatherRest=3.40
stage 4 behavior=Explore   paintCount=0 nearestPaint=-1.0 explorationVisits=1 gatherRest=2.40
stage 5 behavior=Gather    paintCount=0 nearestPaint=-1.0 explorationVisits=1 gatherRest=0.00
```

结论：

- 粘液目标在采集休息期间被选择，个体进入 50 cm 停止圈（49.6 cm）。
- 警戒期间进入 `AlertWait`；清除后设置 4 秒采集休息，0.6 秒后读数为 3.40 秒。
- 休息耗尽后进入探索，并记录至少 1 个探索历史网格。
- 探索目标完成后恢复普通采集。

## 资源争夺消退

```text
命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyDisputeTest
日志：Saved/Logs/R5-Dispute-final.log
```

关键日志：

```text
SlimeEcologyDisputeTest: decay 0.75 -> 0.25, alerted=yes warning=1.00 cooldown=5.00.
```

结论：

- 争夺条件消失后累计值按时间下降。
- 达到 1.5 秒仍触发警戒。
- 触发后 5 秒冷却保持。

## 发现并修复的问题

首次测试中个体停在 66.4 cm，原因是粘液移动调用 `SetMoveGoal` 时使用默认 70 cm 到达半径。
已改为显式 `PaintStopRadius=50`，复测到 49.6 cm。

## 日志说明

`SlimeSpawnEcology` 是单场景调试命令，会清掉开局 20 只生态个体，因此日志末尾的
`mass BROKEN` 是测试装置产物，不代表资源/体量账本回归。账本门禁以 R3 soak
（`Saved/Logs/R4-R3Regression-final.log`）为准。

## 待手动

- 携货者警戒结束后继续返程。
- 粘液停留期间不重新被普通采集抢占。
- 多次探索后的重复访问惩罚和选点稳定性。

清单：`Docs/EcologyRebuild/R5-Checklist.md`。
