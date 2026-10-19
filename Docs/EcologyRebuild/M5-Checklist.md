# M5 核心、跃击、吞噬、爆裂与弱点 · 编辑器验收

> 自动测试已通过，用户已于 2026-10-19 完成全部手动验收。

## 1. 打开工程

1. 关闭正在运行的 Unreal Editor。
2. 双击 `SlimeWar.uproject`，提示重新编译时选 Yes。
3. 打开 `/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza`。
4. PIE 控制台执行：

```text
SlimeRunTimeScale 0
show collision
Slime.Debug.DrawEcologyState 1
```

---

## 2. 融合生成中性核心

```text
SlimeClearEnemies
SlimeEcologyFusionTest
```

- [x] 体量 1+1 融合后变成体量 2。
- [x] HUD 显示 `core Neutral`。
- [x] 中性核心继续探索/聚群，不主动追玩家或敌对个体。
- [x] 体量 2 不悬空。

---

## 3. 敌对核心苏醒与追击

```text
SlimeClearEnemies
SlimeSpawnEcologyCore 2 2
SlimeEcologyProvokePlayer 10
```

- [x] 核心先显示 `Waking`，约 1.2 秒内不移动。
- [x] 苏醒后进入 `Chasing`。
- [x] 玩家进入 240 cm 或被挑衅时成为目标。
- [x] 没有玩家目标时优先追敌对阵营个体。
- [x] 玩家围绕核心转圈时，核心朝当前玩家方向追击，不沿玩家历史路径绕圈。
- [x] 玩家改变方向后，核心在约 25 cm 路径更新阈值内纠正方向。

---

## 4. 跃击

```text
SlimeClearEnemies
SlimeEcologyPounceTest
```

- [x] 前摇约 0.65 秒，期间水平方向锁定。
- [x] 跃击有真实向上和向前位移，不是瞬移。
- [x] 每次跃击对玩家只扣 20 生命一次。
- [x] 落空时仍沿原方向完成冲刺。
- [x] 结束后进入收势和冷却。
- [x] 两次跃击起始间隔至少 1.6 秒。

---

## 5. 低血撤退

1. 保持一个敌对核心。
2. 将其生命打到 30% 以下，例如：

```text
SlimeDamageNearestEnemy 120
```

- [x] 核心显示 `Retreating`。
- [x] 停止追击并返回出生/阵营中心。
- [x] 约 2.5 秒后恢复普通行为。

---

## 6. 吞噬

```text
SlimeClearEnemies
SlimeEcologyDevourTest
```

- [x] 攻击者从体量 2 成长到体量 4。
- [x] 生命从 40 增加到 50。
- [x] Credit 从 8 增加到 16。
- [x] 攻击者 Cargo 保留。
- [x] 受害者被吞噬后不产生玩家击杀分数。

---

## 7. 单核心爆裂

1. 生成一个中性核心：

```text
SlimeClearEnemies
SlimeSpawnEcologyCore 2 1
```

2. 用武器击杀核心。

- [x] 死亡时产生约 350 cm 爆裂范围。
- [x] 范围内普通个体受到伤害和径向推力。
- [x] 墙后目标不受伤害。
- [x] 每个核心只爆裂一次。

---

## 8. 爆裂连锁

```text
SlimeClearEnemies
SlimeEcologyDetonationTest
```

- [x] 第一个核心爆裂后，第二个核心也进入连锁。
- [x] 两个核心各只出现一次爆裂。
- [x] 无重复爆炸、重复伤害或无限连锁。
- [x] 普通旁观个体生命下降并被径向推开。
- [x] 画面能看到橙色爆裂范围球、连线和目标红圈。

---

## 9. 弱点

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 6
```

- [x] 体量 6 HUD 显示 `weakpoint=no`。
- [x] 头顶没有黄色弱点球。

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 7
```

- [x] 体量 7 头顶出现黄色弱点球。
- [x] 弱点位置在身体顶部，不阻挡身体移动。
- [x] 普通身体命中只造成正常伤害。
- [x] 正伤害命中弱点立即处决。

自动零伤害检查：

```text
SlimeClearEnemies
SlimeEcologyWeakpointTest
```

- [x] 体量 7 的 0 伤害命中不处决。

---

## 10. 核心 HUD

- [x] HUD 显示 core state、phase、target、wake、windup、pounce、cooldown、exploded。
- [x] 敌对核心使用红色状态文字。
- [x] 体量 7/8 显示黄色弱点球。

---

## 11. 手动验收结论

- [x] 全部通过
- [ ] 存在问题，问题描述：

```text

```

手动验收已完成，M5 标记为 `Passed`。
