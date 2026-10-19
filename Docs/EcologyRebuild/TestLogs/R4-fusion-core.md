# R4 融合与核心自动测试

- 日期：2026-10-19
- 状态：自动场景通过，手动 Checklist 待执行
- 地图：`/Game/_SlimeWar/Maps/L_Ecology_DoublePoint`

## 编译

```text
SlimeWarEditor Win64 Development
结果：0 error
```

## 外力扫掠融合

```text
命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyExternalConnectionTest
日志：Saved/Logs/R4-External-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00107.png
```

关键日志：

```text
SlimeEcologyExternalConnectionTest: pair 150 cm apart; first pushed at 500 cm/s with 4 s stun.
EcologyFusion: displacement SlimeEcologyEnemy_20 + SlimeEcologyEnemy_21 -> mass 2, health 40.0/40.0, cargo 0, credit 8.
EcologyExternalConnection: fusions=1 live=1 survivorStun=2.57
```

结论：

- 待眩晕/眩晕不再拒绝位移融合。
- 扫掠命中后立即提交，不再等待 2.4 秒。
- 融合体继承了双方眩晕，2 秒后仍有 2.57 秒剩余。

## N07 射击附加融合冷却

```text
命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyShotCooldownTest
日志：Saved/Logs/R4-ShotCooldown-final2.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00114.png
```

关键日志：

```text
EcologyFusion: displacement SlimeEcologyEnemy_20 + SlimeEcologyEnemy_21 -> mass 2, health 40.0/40.0, cargo 0, credit 8.
EcologyShotCooldown[ready]: shotCooldown=0.80 forcedFusion=yes live=1
EcologyShotCooldown[old]: shotCooldown=0.80 snapshot=blocked forcedFusion=no live=2
```

结论：

- 射击前旧冷却已满足时，0.8 秒附加冷却不会阻止 3 秒玩家撮合。
- 射击前旧冷却未结束时，快照为 blocked，不能借附加冷却绕过。

## 核心近身玩家优先

```text
命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyCorePriorityTest
日志：Saved/Logs/R4-CorePriority-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00109.png
```

关键日志：

```text
SlimeEcologyCorePriorityTest: player 120 cm, rival 220 cm.
EcologyCorePriority[near]: target=BP_ThirdPersonCharacter_C_0 isPlayer=yes phase=Pouncing.
EcologyCorePriority[far]: target=SlimeEcologyEnemy_21 isRival=yes phase=Chasing.
```

结论：

- 玩家进入 180 cm 优先圈后成为目标。
- 玩家退出优先圈后，核心切回更近的可见敌族。

## 数值与偏差

- N06 的 0.65 m / 0.45 m/s 仍为 `Recorded – No Change`。
- 本轮删除了临时新增的 65/45 字段；位移融合使用既有 8 cm 容差。
- `Forced` 仅是内部枚举名，运行日志已改为 `displacement`。

## 回归

普通接触融合首次回归失败：伙伴在 `Contacting` 阶段被下一帧普通采集目标带走，最终分离并进入 5 秒打断禁用。

修复：

- 已建立的融合连接（`Fusion->IsEngaged()`）优先于 `UpdateResourceBehavior()`。
- 资源行为仍会在没有连接时先于“开始新融合”执行，符合 R2 的“采集优先于开始融合”，同时满足“采集不得抢占已建立连接”。

复测：

```text
命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyFusionTest
日志：Saved/Logs/R4-FusionRegression-final2.log
结果：contact stage 1 contact=1.00 -> 融合 mass 2，health 40/40，credit 8

命令：SlimeRunStart,SlimeRunTimeScale 0,SlimeEcologyForcedFusionTest
日志：Saved/Logs/R4-ForcedRegression-final2.log
结果：mass 8，health 160/160，credit 40
```

R2/R3 回归：

```text
R2：Saved/Logs/R4-R2Regression-final.log
结果：stage 1 AvoidPlayer cargo=1；stage 2 ReturnCargo cargo=1

R3：Saved/Logs/R4-R3Regression-final.log
结果：20 秒完成 8 次交付；resource OK（18 initial + 2 regen）；mass OK（20 + 8 = 28 live）
```

## 待手动

- 真实射击输入触发 N07。
- 分裂平复期间拒绝玩家撮合和警戒。
- 大体量核心跃击与路径命中去重。

清单：`Docs/EcologyRebuild/R4-Checklist.md`。
