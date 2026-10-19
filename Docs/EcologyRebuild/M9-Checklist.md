# M9 三阵营 60×50 生态关卡手动验收

> 自动场景和 60 秒 soak 已通过。本清单验证真实键鼠输入、视觉反馈和完整回合。

## 1. 启动

1. 打开 `L_Ecology_ThreePoint`。
2. PIE，控制台执行：

```text
SlimeRunTimeScale 0
show collision
SlimeEcologyGroundProbe
```

- [x] 地面探测：A/B/C 与 5 个矿点全部 `hit=yes` 且 `nav=yes`。
- [x] HUD 显示 `TIME 180`、`SCORE 0 / 160`。

## 2. 三阵营开局

```text
SlimeRunStart
SlimeEcologyStatus
SlimeEcologyResourceLedgerCheck
```

- [x] A/B/C 各 10 只体量 1，总计 30。
- [x] 首次账本为 `mass OK (30 initial = 30 live)`。
- [x] 5 个矿点初始库存合计 22。
- [x] 三站点标签分别为青绿、橙黄、紫色。

## 3. 采集、返程与交付

运行 20 秒后执行：

```text
SlimeEcologyStatus
SlimeEcologyResourceLedgerCheck
```

- [x] 史莱姆会前往 A_Near/B_Near/C_Near 和中心两个共享矿点。
- [x] 携货个体返回自己阵营站点，不会返回其他阵营。
- [x] 交付 1 份货物只新增 1 只体量 1。
- [x] 消耗共享矿点后约 5 秒恢复 1 份，上限 6。
- [x] 资源账本和体量账本均 OK。
- [x] 60 秒内没有永久卡死、穿墙或掉出地图。

## 4. 三阵营手雷输入

1. 按 `1` 选择同化雷，`2` 选择旋风雷。
2. 按 `Z` 选择 A，`X` 选择 B，`C` 选择 C。
3. 按 `G` 投掷。

- [x] `1/2` 切换手雷类型，HUD 选择状态变化。
- [x] `Z/X/C` 切换 A/B/C，HUD 当前阵营变化。
- [x] `G` 投掷有飞行、重力和落地触发。
- [x] 两种手雷初始各 3 枚，投掷间隔约 0.7 秒。
- [x] 双点关卡中按 `C` 被忽略并记录 `has no C faction site`。

## 5. C 阵营同化

```text
SlimeGrenadeAssimilationTest 2
```

- [x] 3 只 A 阵营个体变为 C。
- [x] 个体会返程到 C 点，而不是 A 点。
- [x] 颜色、HUD 站点统计同步变为 C。
- [x] 体量、生命、核心类型和货物不被同化改变。

## 6. 阵营冲突与融合

- [x] 同阵营可以正常融合并生成核心。
- [x] 不同阵营不会非法融合。
- [x] 争夺会触发局部警戒；玩家枪击不会传播普通警戒。
- [x] 敌对核心跃击、吞噬、爆裂正常。
- [x] 吞噬后体量账本仍 OK，不出现 `RemovedMass` 虚高。

## 7. 计分与终局

```text
SlimeEcologyScoreTest
SlimeRunResult
```

- [x] 玩家击杀普通体量 8 无货=68，携货 2=80，敌对核心=40。
- [x] NPC 互杀、吞噬和 NPC 爆炸不给玩家分。
- [x] 到达 160 分不会提前结束。
- [x] 时间到、玩家死亡、灭绝三种终局均可触发。
- [x] 结算日志包含玩家/NPC 击杀、截货、交付、融合、引爆、连锁、受伤和用时。

## 8. 现有 UI 兼容

- [x] `WBP_SlimeHUD` 正常加载，分数、时间、生命、弹药不回归。
- [x] `WBP_SlimeResult` 正常加载，原有 Score/Target/Best/Passed 等字段不回归。
- [x] 新站点事件未实现时不会产生蓝图编译或运行错误。

## 9. 双点回归

打开 `L_Ecology_DoublePoint`，执行：

```text
SlimeRunStart
SlimeEcologySoakTest 12
SlimeEcologyResourceLedgerCheck
```

- [x] 20 只开局，A/B 双点正常运行。
- [x] 资源/体量账本 OK，`pathFailures=0`，`penetrations=0`。
- [x] `C` 阵营同化被拒绝。

## 10. 结论

- [x] 全部通过
- [ ] 存在问题：

```text

```

## 11. 验收结论

- 验收日期：2026-10-19
- 结论：全部通过，M9 标记为 `Passed`。

