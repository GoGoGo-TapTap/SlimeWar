# M6 磁性瞄准、蓄力射击与撞击分裂自动测试

- 日期：2026-10-19
- 状态：`Passed`；自动测试与用户手动 Checklist 均通过

## 编译

`SlimeWarEditor Win64 Development`：0 error / 0 warning。

## 数据资产

- `DA_Weapon_Rifle`：`/Game/_SlimeWar/Weapon/DA_Weapon_Rifle`
- 已绑定 `DA_RunConfig::WeaponRules`
- 启动日志确认：`weapon rules: DA_Weapon_Rifle`

关键值：

| 参数 | 值 |
|---|---:|
| 基础伤害 | 8 |
| 射击间隔 | 0.22 s |
| 弹匣 | 3 |
| 换弹 | 1 s |
| 射程 | 1800 cm |
| 起蓄 | 0.18 s |
| 满蓄 | 1.2 s |
| 满蓄半径 | 400 cm |
| 满蓄击退速度 | 1400 cm/s |
| 捕获半径 | 10% 屏高 |
| 保留半径 | 1.35× |
| 换锁阈值 | 55% |

## 蓄力与击退

日志：`Saved/Logs/M6-Weapon.log`

| Alpha | 伤害 | 外部速度 | 待眩晕 |
|---:|---:|---:|---:|
| 0 | 8（40→32） | 254.9 cm/s | 1.50 s |
| 0.5 | 4（40→36） | 919.2 cm/s | 1.50 s |
| 1 | 0（40→40） | 1619.2 cm/s | 1.50 s |

结论：

- 伤害随蓄力线性下降，满蓄力为 0。
- 直接推力与范围推力可以叠加。
- 受影响目标进入 1.5 秒待眩晕。
- 融合打断路径由 `InterruptFusion` 调用。

## 磁性瞄准

日志：`Saved/Logs/M6-Aim.log`

截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00062.png`

结果：

- 目标被身体胶囊候选捕获并锁定。
- 锁定后目标状态为 `AimLocked`，自主移动停止。
- 屏幕距离 88.2 px；捕获半径 72 px，进入 1.35× 保留区。

## 撞击分裂

| 父体量 | 碎片数 | 父生命 | 碎片总生命 | Cargo | Credit |
|---:|---:|---:|---:|---:|---:|
| 2 | 2 | 20 | 20 | 3 | 20 |
| 8 | 8 | 80 | 80 | 3 | 20 |

结论：

- 碎片数量等于父体量。
- 生命、货物和身体分总量守恒。
- 父体直接销毁，没有走击杀入口。

## 待手动验收

- 屏幕换锁 55% 阈值和保留区。
- 墙体遮挡与枪口遮挡。
- 体量 7/8 身体/弱点候选。
- 按住左键蓄力、松开射击和 0.22 秒间隔。
- 真实撞墙触发 0.9 秒分裂窗口。
- 位置不足时回滚且不丢体量。

对应清单：`Docs/EcologyRebuild/M6-Checklist.md`。

用户已于 2026-10-19 完成上述手动验收。M6 最终状态：`Passed`。

## 手动反馈修复复测

### 静态立方体撞墙

日志：`Saved/Logs/M6-SplitCollision.log`

截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00065.png`

结果：

```text
ImpactSplitWindow: hit StaticMeshActor_3 / StaticMeshComponent0 / WorldStatic / Static
ImpactSplit: mass 2 -> 2 fragments
ImpactSplitCollisionTest: fragments=2 remainingParents=0
```

结论：静态立方体可以触发撞击分裂，碎片位置从墙体安全半平面生成。

### 其余手动反馈

- 枪口遮挡与射击射线改为角色武器位置，解决相机越墙时可锁定/命中。
- 空弹和换弹期间不再进入蓄力；按空弹会触发换弹。
- HUD 换弹期间显示橙色 `RELOADING` 进度条，隐藏蓄力条。
- 范围推力方向在弹着点与目标中心重合时回退为玩家指向目标，确保满蓄力击退有效。
