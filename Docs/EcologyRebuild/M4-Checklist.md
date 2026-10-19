# M4 融合 v2 · 编辑器步骤与手动验收

> 自动测试已通过。用户已于 2026-10-19 在真实 UE 窗口完成第 1–10 项手动验收。

## 1. 打开工程

1. 关闭正在运行的 Unreal Editor。
2. 双击 `SlimeWar.uproject`，如提示重新编译选 Yes。
3. 打开 `/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza`。
4. PIE 控制台执行：

```text
SlimeRunTimeScale 0
show collision
```

生态史莱姆头顶默认显示调试状态块；若看不到：

```text
Slime.Debug.DrawEcologyState 1
```

状态块包含体量、生命、Cargo、Credit、融合、准备/冷却/禁用、大脑、身体、外力、眩晕和速度。

---

## 2. 自主融合

```text
SlimeEcologyAutoFusionTest
```

- [x] 两只体量 1 进入 Seeking 并向彼此靠近。
- [x] 进入 Contacting 后持续约 2.4 秒。
- [x] 融合成一只体量 2。
- [x] 生命为 40/40。
- [x] 融合后没有悬空或下陷。
- [x] 日志 `EcologyFusion:` 中 Credit 为 8。
- [x] 冷却日志约 1.8 秒递减。

---

## 3. 玩家身体推动撮合

1. 清理并布置测试对：

```text
SlimeClearEnemies
SlimeEcologyPushPairSetup
```

2. 两只体量 1 相距 180 cm，已锁定自主移动并禁止普通融合。
3. 用 WASD 顶住其中一只，把它推向另一只。

- [x] 被推史莱姆获得外力速度。
- [x] 碰撞扫掠到另一只后立即融合。
- [x] 不需要等待 2.4 秒。
- [x] 不耗弹、不造成伤害。
- [x] 融合体量正确。
- [x] 没有穿墙或异常弹飞。

---

## 4. 接触进度与分离取消

```text
SlimeClearEnemies
SlimeEcologyFusionTest
```

- [x] 两只接触后日志 `contact` 从 0 增长到 1。
- [x] 在接触未满 2.4 秒时用外力把一只推开。
- [x] 状态从 Contacting 离开，融合取消。
- [x] 取消后 5 秒内不应自主重新融合。

---

## 5. 体量 4+4 位移撮合

```text
SlimeClearEnemies
SlimeEcologyForcedFusionTest
```

- [x] 体量 4 被外力扫掠到另一只体量 4。
- [x] 立即融合为体量 8。
- [x] 生命为 160/160。
- [x] Credit 为 40。
- [x] 体量 8 仍贴地，碰撞胶囊与视觉一致。

---

## 6. 打断与 5 秒禁用

```text
SlimeClearEnemies
SlimeEcologyInterruptTest
```

- [x] Contacting 阶段被打断。
- [x] 双方显示 Blocked，剩余禁用约 5 秒。
- [x] Blocked 期间不能自主开始新融合。
- [x] Blocked 期间玩家外力撮合仍可完成。

---

## 7. 成功冷却

1. 完成任意一次融合。
2. 立即执行：

```text
SlimeEcologyFusionStatus
```

- [x] 状态显示 Cooldown。
- [x] 剩余时间从约 1.8 秒递减。
- [x] 冷却结束前不发生新的融合。
- [x] 冷却结束后恢复 Idle。

---

## 8. 体量上限

- [x] 体量 2 + 体量 1 不会自主融合。
- [x] 玩家外力撮合可以突破自动上限。
- [x] 总质量不超过 8。

---

## 9. 三只同时接触

- [x] 三只体量 1 同时接触时只形成一对融合。
- [x] 第三只不会同时连接两个对象。
- [x] 融合后没有重复销毁或残留伙伴引用。

---

## 10. 融合后状态

- [x] `Cargo` 融合前后不丢失、不复制。
- [x] `BodyScoreCredit` 为两个父体之和。
- [x] 融合后生命不是无条件满血，而是继承双方剩余生命总和。
- [x] 体量 2/4/8 的胶囊底和网格底误差 ≤2 cm。
- [x] 融合体移动、聚群、避让玩家仍然正常。

---

## 11. 手动验收结论

- [x] 全部通过
- [ ] 存在问题，问题描述：

```text

```

M4 已标记为 `Passed`。
