# R2 行为优先级自动测试

- 日期：2026-10-19
- 状态：自动场景通过，手动 Checklist 待执行

## 编译

`SlimeWarEditor Win64 Development` 0 error。

## 测试

```text
地图：L_Ecology_DoublePoint
命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyPriorityTest
日志：Saved/Logs/R2-Priority.log
截图：HighresScreenshot00098.png
```

## 结果

```text
carrier cargo=1
stage 1: behavior=AvoidPlayer cargo=1 fusion=Idle core=Idle playerDistance=224.1
stage 2: behavior=ReturnCargo cargo=1 fusion=Idle core=Idle playerDistance=670.0
```

- 携货个体在玩家接近时进入 AvoidPlayer。
- 玩家远离后恢复 ReturnCargo。
- 普通融合和核心追击没有抢占携货行为。

## 待手动

- 携货 + 敌对核心 + 玩家同时存在的场景。
- 主动跃击期间携货不打断攻击。
- 瞄准锁定/眩晕期间观察刷新和采集休息计时暂停。

对应清单：`Docs/EcologyRebuild/R2-Checklist.md`。
