# M8 Ecology Rules 自动测试记录

- 日期：2026-10-19
- 分支：`rebulid-program`
- 状态：自动测试通过，手动 Checklist 待执行

## 编译

```text
Build.bat SlimeWarEditor Win64 Development -Project="E:\UE\Unreal Projects\SlimeWar\SlimeWar\SlimeWar.uproject"
```

结果：0 error。

## 警戒、争夺、敌对融合、计分、粘液

组合日志：`Saved/Logs/M8-Rules.log`

```text
SlimeEcologyDisputeTest: alerted=yes warning=1.00 cooldown=5.00
SlimeEcologyHostileFusionTest: fused=yes mass=2 core=2 hostile=yes
SlimeEcologyScoreTest: normal=68 cargo2=80 hostile=40 npc=0
SlimePaintStatus: splats=6 records=6 rt=valid size=700x1024 fields=1 debug=1
```

独立 NPC 伤害警戒：`Saved/Logs/M8-Alert.log`

```text
SlimeEcologyAlertTest: victimAlerted=yes warning=1.00 alert=10.00 alliesAlerted=2 attackerAlerted=no
```

玩家枪击无警戒：

```text
SlimeWeaponDoesNotAlertTest: alerted=no
```

## 同化雷

日志：`Saved/Logs/M8-Grenade-Assimilation.log`
截图：`HighresScreenshot00074.png`

```text
GrenadeAssimilation: faction 1 affected 3 slime(s)
SlimeGrenadeTest: stage=0 factionB=3 maxExternalSpeed=0.0
```

结论：3 只 A 阵营个体全部变为 B，未产生外力。

## 旋风雷

日志：`Saved/Logs/M8-Grenade-Tornado.log`
截图：`HighresScreenshot00075.png`

```text
SlimeGrenadeTest: stage=10 factionB=0 maxExternalSpeed=450.0
SlimeGrenadeTest: stage=11 factionB=0 maxExternalSpeed=354.7
```

结论：吸引速度达到并限制在 450 cm/s；4.5 秒后停止注入，残余外力继续由 500 cm/s² 阻尼衰减。

## 粘液

日志：`Saved/Logs/M8-Paint.log`
截图：`HighresScreenshot00077.png`

```text
SlimePaintStatus: splats=6 records=6 rt=valid size=700x1024 fields=1 debug=1
```

实现偏差：双点地图世界轴为 X=3000 cm、Y=4400 cm，因此 RenderTarget 采用 700×1024，而不是初版计划中的 1024×700。调试 splat 可在地面看到；正式材质等待美术资源。

## 30 秒资源与体量账本

日志：`Saved/Logs/M8-Soak.log`

```text
EcologyLedger[manual check]: resource OK; mass OK
EcologyLedger[periodic]: resource OK; mass OK
EcologyLedger[end run]: resource OK; mass OK
EcologyLedger[debug status]: resource OK; mass OK
```

运行期间完成 3 次交付，资源账本：

```text
12 stock + 5 carried + 0 ground + 3 delivered = 18 initial + 2 regen
```

体量账本：

```text
20 initial + 3 delivered = 23 live + 0 removed
```

## 手动反馈修复：同化站点与旋风遮挡

同化后站点/HUD/返程：

```text
SlimeGrenadeTest: stage=0 factionB=3 assignedB=3
EcologySite: A faction=0 live=0 mass=0
EcologySite: B faction=1 live=3 mass=3
SlimeGrenadeTest: carrier=SlimeEcologyEnemy_0 faction=1 site=B brain=ReturnCargo
goal=V(X=-100.00, Y=1400.00, Z=2.00) cargo=1
```

日志：`Saved/Logs/M8-AssimReturn.log`
截图：`HighresScreenshot00080.png`

结论：同化同时更新 Faction、AssignedSite、Home/活动区；携货个体返回 B 站点，HUD 按当前阵营统计。

旋风墙体遮挡：

```text
SlimeGrenadeTornadoWallTest: nearExternal=110.5 farExternal=0.0
```

日志：`Saved/Logs/M8-TornadoWall.log`
截图：`HighresScreenshot00081.png`

结论：静态墙体后的个体不再被旋风吸引。

## 跃击中被瞄准锁定

日志：`Saved/Logs/M8-PounceLock.log`
截图：`HighresScreenshot00083.png`

```text
SlimeEcologyPounceLockTest: locked at phase=Pouncing z=70.5 vz=-75.4
SlimeEcologyPounceLockTest: after 0.8 s zDelta=-42.9 vz=0.0 phase=Recovering aimLocked=yes
```

修复：瞄准锁定/眩晕时不再重复调用 `StopMovementImmediately`，改为只清除自主目标和水平速度，保留竖直速度与重力。

## 手动验收结果

用户已于 2026-10-19 完成 `Docs/EcologyRebuild/M8-Checklist.md`，以下项目均通过：

- `1/2`、`Z/X`、`G` 真实键鼠输入、手雷飞行、库存与 0.7 秒投掷间隔。
- 同化雷的阵营/颜色/HUD 同步，以及携货个体改返新阵营站点。
- 旋风雷的拉扯、速度上限、4.5 秒持续和静态墙体遮挡。
- 玩家枪击不传播警戒、NPC 伤害警戒、争夺与警戒融合敌对核心。
- 玩家/NPC 击杀计分、携货分、敌对核心半分和连锁单次结算。
- 粘液调试 splat 的持久显示与结算保留。
- 跃击中被瞄准锁定时继续受重力并自然落地。
- 完整回合内资源/体量账本平衡，结算字段齐全。

未纳入本次通过范围：正式粘液材质/RVT 与最终美术表现，等待美术资源接入后单独验收。

M8 结论：`Passed`。
