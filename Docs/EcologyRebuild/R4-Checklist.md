# R4 融合与核心行为手动验收

日期：2026-10-19

## 1. 启动

1. 打开 `/Game/_SlimeWar/Maps/L_Ecology_DoublePoint`。
2. PIE 后执行：

```text
SlimeRunStart
SlimeRunTimeScale 0
```

3. 确认顶部 HUD 的 `fusion` 行包含 `calm` 字段。

## 2. 外力/位移融合

- [ ] 执行 `SlimeEcologyExternalConnectionTest`，两只体量 1 在待眩晕滑行中立即融合。
- [ ] 融合后场上只剩 1 只体量 2，日志出现 `fusions=1 live=1`。
- [ ] HUD 或日志显示融合后仍有眩晕，且不会因待眩晕被拒绝。
- [ ] 融合后的 `Cargo`、`Credit`、生命不丢失、不复制。

## 3. N07 射击附加冷却

- [ ] 执行 `SlimeEcologyShotCooldownTest`。
- [ ] `[ready]` 行显示 `shotCooldown=0.80 forcedFusion=yes live=1`。
- [ ] `[old]` 行显示 `snapshot=blocked forcedFusion=no live=2`。
- [ ] 在真实射击中，射击前已经可融合的目标仍可在 3 秒撮合窗口内被推到一起。
- [ ] 射击前冷却尚未结束的目标，不会因新的 0.8 秒附加冷却被强行撮合。

## 4. 核心目标切换

- [ ] 执行 `SlimeEcologyCorePriorityTest`。
- [ ] 近身阶段日志为 `isPlayer=yes`，核心在 1.8 米内优先玩家。
- [ ] 玩家退到 5 米、敌族位于 2 米时，日志为 `isRival=yes`。
- [ ] 核心前摇开始后方向锁定，跃击阶段不会重复命中同一目标。

## 5. 分裂平复

- [ ] 执行 `SlimeImpactSplitTest 2`，制造冲击分裂。
- [ ] 碎片 HUD 的 `calm` 从约 5 秒开始递减。
- [ ] 平复期间玩家把碎片推到一起，不会立即融合。
- [ ] 平复期间碎片不接收普通警戒。
- [ ] 眩晕期间平复计时暂停，眩晕结束后继续递减。

## 6. 数值冲突检查

- [ ] 未修改 `FusionForcedCaptureGap=8`。
- [ ] 未修改 `FusionContactTolerance=18`。
- [ ] 未修改 `FusionAutoSeekSpeed=75`。
- [ ] 未新增或启用 N06 的 65 cm / 45 cm/s 冲突值。
- [ ] 未修改 N02 吞噬回血、N03 跃击高度、N09 中性核心连锁。

## 7. 结论

- [ ] 全部通过
- [ ] 存在问题：

```text

```
