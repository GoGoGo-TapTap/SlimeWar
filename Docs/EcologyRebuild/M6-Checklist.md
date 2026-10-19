# M6 磁性瞄准、蓄力射击与撞击分裂 · 手动验收

> 自动测试已通过。以下项目必须在真实 UE 窗口中使用实际鼠标和键盘完成。

## 1. 准备

1. 打开 `/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza`。
2. PIE 后执行：

```text
SlimeRunStart
SlimeRunTimeScale 0
Slime.Debug.DrawAimAssist 1
show collision
```

3. 清空旧敌人：

```text
SlimeClearEnemies
```

---

## 2. 屏幕捕获与锁定

1. 生成一只体量 1：

```text
SlimeSpawnEcology 1 0 1
```

2. 移动鼠标，让准星靠近史莱姆屏幕投影。

- [ ] 屏幕中心出现白色捕获圈。
- [ ] 进入捕获圈后出现蓝色身体锁定框。
- [ ] 目标头顶状态显示 `AimLocked`。
- [ ] 目标停止自主移动，但仍受重力和外力影响。

---

## 3. 保留与换锁

1. 锁定第一个目标后，再生成第二个目标：

```text
SlimeSpawnEcology 2 0 1
```

2. 保持当前目标在捕获圈 1.35 倍以内，再把准星慢慢移向第二个目标。

- [ ] 当前目标在保留区内不会因为轻微偏离立即解锁。
- [ ] 新目标只有明显更接近准星时才换锁。
- [ ] 换锁后旧目标解除 `AimLocked`。

---

## 4. 遮挡

1. 让玩家身体完全在墙后，但第三人称相机从墙上/侧面仍能看到史莱姆。

- [ ] 相机能看见目标时，准星靠近也不会锁定。
- [ ] 左键射击不会伤害墙后的目标。

2. 把枪口侧贴住墙，让相机仍能看到目标。

- [ ] 枪口被遮挡时不会锁定。
- [ ] 枪口遮挡时射击命中墙体，不伤害目标。

---

## 5. 弱点候选

1. 生成体量 7：

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 7
```

2. 将准星分别靠近身体中心和顶部黄色弱点。

- [ ] 身体候选显示蓝色 `LOCK BODY`。
- [ ] 弱点候选显示黄色 `LOCK WEAKPOINT`。

3. 短按左键射击弱点。

- [ ] 正伤害命中弱点时立即处决。

4. 重新生成体量 7，按住左键直到满蓄力后松开。

- [ ] 满蓄力伤害为 0。
- [ ] 满蓄力不触发弱点处决。

---

## 6. 蓄力输入

1. 生成体量 2：

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 2
```

2. 按住左键：

- [ ] 按住期间不会立即发射。
- [ ] 底部蓝色蓄力条随时间增长。
- [ ] 0.18 秒后才开始有明显蓄力。

3. 松开左键：

- [ ] 松开瞬间发射。
- [ ] 0.22 秒内不能再次成功发射。
- [ ] 弹匣从 3 发逐发减少。
- [ ] 打空后换弹约 1 秒。

4. 打空弹匣后继续按住左键：

- [ ] 不会显示蓄力条。
- [ ] 不会播放射击动作或发射。
- [ ] 最后一发命中后自动进入换弹。

5. 换弹期间持续按/松开左键：

- [ ] 不显示蓄力。
- [ ] 不发射。
- [ ] HUD 显示独立橙色 `RELOADING` 进度条。
- [ ] 换弹完成后恢复正常射击。

6. 蓄力过程中按 R：

- [ ] 当前蓄力立即取消。
- [ ] 进入换弹进度显示。

---

## 7. 伤害与击退

分别测试短按、约半蓄力、满蓄力：

- [ ] 短按伤害 8。
- [ ] 半蓄力伤害约 4。
- [ ] 满蓄力伤害 0。
- [ ] 击退距离随蓄力增加。
- [ ] 满蓄力产生约 4 米范围击退。
- [ ] 被击退目标进入待眩晕，停稳后眩晕 1.5 秒。

可随时执行：

```text
SlimeWeaponStatus
SlimeAimAssistStatus
SlimeEcologyBodyStatus
```

---

## 8. 撞墙分裂

1. 生成体量 2，并把它推向墙面或方块：

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 2
```

2. 满蓄力将目标击退到静态墙体。

- [ ] 目标撞墙后分裂为 2 只体量 1。
- [ ] 两只碎片都贴地。
- [ ] 碎片进入约 2 秒眩晕。
- [ ] 碎片融合禁用约 5 秒。
- [ ] 玩家击杀数不增加。
- [ ] 玩家分数不增加。

3. 体量 8 重复测试：

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 8
```

- [ ] 分裂为 8 只体量 1。
- [ ] 生命、货物和 body credit 总量守恒。

4. 体量 1：

- [ ] 不会分裂。

5. 狭窄墙角测试：

- [ ] 合法位置不足时保持父体不变。
- [ ] 不产生部分碎片。
- [ ] 不丢失生命、货物或分数。

6. 静态立方体测试：

```text
SlimeRunStart
SlimeImpactSplitCollisionTest 2
```

- [ ] 体量 2 被满蓄力推向临时静态立方体后分裂为 2 只。
- [ ] 日志出现 `ImpactSplitWindow` 和 `ImpactSplit`。
- [ ] `fragments=2 remainingParents=0`。

---

## 9. 自动复现

```text
SlimeRunTimeScale 0
SlimeWeaponStatus
SlimeChargeTest 0
SlimeChargeTest 0.5
SlimeChargeTest 1
SlimeKnockbackTest
SlimeImpactSplitTest 2
SlimeImpactSplitTest 8
```

预期：

- 伤害 8 / 4 / 0；
- 击退随 Alpha 增大；
- 体量 2→2、体量 8→8；
- 生命/货物/Credit 守恒。

---

## 10. 验收结论（2026-10-19：Passed）

- [x] 全部通过
- [ ] 存在问题，问题描述：

```text

```

完成本清单后，M6 才可从 `Active` 标记为 `Passed`。
