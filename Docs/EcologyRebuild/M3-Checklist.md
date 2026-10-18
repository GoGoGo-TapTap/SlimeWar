# M3 身体交互 · 编辑器步骤与手动验收

> 自动测试已通过。下面必须在真实 UE 窗口中用键盘/鼠标完成，确认物理推动、阻挡和手感。

> 2026-10-18 用户确认：第 3 项体量 1 推动正常（日志中的 `external=0` / `bodyPushWindow=0` 是停止推动后读取的正常结果）；第 4–9 项目测或日志通过。

## 1. 打开工程

1. 关闭正在运行的 Unreal Editor。
2. 双击 `SlimeWar.uproject`；若提示重新编译，选 Yes。
3. 打开 `/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza`。
4. PIE 后打开控制台（`` ` ``）并执行：

```text
SlimeRunTimeScale 0
show collision
```

---

## 2. 玩家胶囊

- [ ] 打开 `BP_ThirdPersonCharacter` 或运行时观察碰撞胶囊。
- [ ] 胶囊半径约 43 cm，总高约 180 cm。
- [ ] 玩家与生态史莱姆相撞时有实体阻挡，不再直接穿过。

---

## 3. 体量 1 推动

1. 清空敌人：

```text
SlimeClearEnemies
```

2. 生成一只体量 1：

```text
SlimeSpawnEcology 1 0 1
```

3. 锁定它，避免它自主走开：

```text
SlimeEcologyAimLock 1
```

4. 用 WASD 走向它并持续顶住约 1 秒。
5. 执行：

```text
SlimeEcologyBodyStatus
```

- [ ] 史莱姆被推走，不是把玩家弹飞。
- [ ] 弹药数不减少。
- [ ] 史莱姆生命不减少。
- [ ] `external` 大于 0。
- [ ] `bodyPushWindow` 大于 0（约 3 秒内）。
- [ ] 玩家可以持续推着它前进。

---

## 4. 体量 2、3 推动

重复第 3 节，分别执行：

```text
SlimeSpawnEcology 1 0 2
SlimeSpawnEcology 1 0 3
```

- [ ] 体量 2 可推动。
- [ ] 体量 3 可推动，但比体量 1 更慢。
- [ ] 不耗弹、不造成伤害。

---

## 5. 体量 4、8 阻挡

1. 清空敌人后生成大体量：

```text
SlimeSpawnEcology 1 0 4
SlimeEcologyAimLock 1
```

2. 正面顶住它 2 秒。

- [ ] 玩家无法穿过。
- [ ] 史莱姆没有被玩家身体推动。
- [ ] `SlimeEcologyBodyStatus` 中 `external=0.0`。

3. 再测试体量 8：

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 8
SlimeEcologyAimLock 1
```

- [ ] 体量 8 稳定阻挡玩家。
- [ ] 不出现嵌入、弹飞或瞬间穿透。

---

## 6. 外力、阻尼和撞墙

1. 清空并生成体量 1：

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 1
SlimeEcologyExternalPush 400
```

2. 在 0.3 秒和 1 秒左右分别执行：

```text
SlimeEcologyBodyStatus
```

- [ ] 外部速度随时间下降。
- [ ] 史莱姆沿地面滑行，没有悬空。
- [ ] 撞到墙/方块时不会穿墙。
- [ ] 撞墙后可以沿墙面滑动。

---

## 7. 瞄准锁定

1. 生成一只体量 1，记录它的位置。
2. 执行：

```text
SlimeEcologyAimLock 1
```

- [ ] 史莱姆立即停止自主移动。
- [ ] 状态显示 `AimLocked`。
- [ ] 重力仍生效，没有悬空。
- [ ] 外力仍可推动。

3. 解除锁定：

```text
SlimeEcologyAimLock 0
```

- [ ] 史莱姆恢复探索/聚群。

---

## 8. 停稳眩晕

1. 清理并生成体量 1：

```text
SlimeClearEnemies
SlimeSpawnEcology 1 0 1
```

2. 执行：

```text
SlimeEcologyStunTest 1.5
```

- [ ] 初始状态为 `StunPending`。
- [ ] 仍在移动时眩晕计时不减少。
- [ ] 停稳约 0.1 秒后进入 `Stunned`。
- [ ] 眩晕约 1.5 秒后恢复 `Normal`。

3. 在眩晕中再次执行：

```text
SlimeEcologyStunTest 1.5
```

- [ ] 眩晕时间取最大值，不叠加成 3 秒。
- [ ] 若再次被推动，会重新等待停稳。

---

## 9. 自动复现

如需一次跑完自动路径：

```text
SlimeRunTimeScale 0
SlimeEcologyBodyTest
```

日志应包含：

- `StunPending` 且外部速度仍大于 0；
- 随后 `Stunned`；
- 最后 `Normal`；
- `AimLocked` 期间位置变化接近 0。

---

## 10. 手动验收结论

- [x] 全部通过
- [ ] 存在问题，问题描述：

```text

```

完成后把本文件勾选状态或问题记录发回，才能将 M3 标记为 `Passed` 并进入 M4。
