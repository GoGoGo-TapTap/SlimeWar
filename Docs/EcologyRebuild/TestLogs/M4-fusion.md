# M4 融合 v2 自动测试记录

- 日期：2026-10-19
- 分支：`rebulid-program`
- 状态：自动测试与手动 Checklist 均通过

## 编译

```text
Build.bat SlimeWarEditor Win64 Development -Project="E:\UE\Unreal Projects\SlimeWar\SlimeWar\SlimeWar.uproject"
```

结果：0 error。

## 接触融合

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeEcologyFusionTest
日志：Saved/Logs/M4-ContactFusion.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00029.png
```

- 1.2 秒接触进度 0.40。
- 2.4 秒接触进度 0.90。
- 完成融合：体量 2，生命 40/40，Cargo 0，Credit 8。
- 融合完成后冷却读取 0.84 秒（1.8 秒窗口的剩余值）。
- 体量 2 网格底与地面误差 2.0 cm。

## 自主融合

```text
命令：SlimeRunTimeScale 0,SlimeEcologyAutoFusionTest
日志：Saved/Logs/M4-AutoFusion-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00036.png
```

- 两只体量 1 进入 Seeking，向同一中点移动。
- 随后进入 Contacting，2.4 秒后融合为体量 2。
- 融合结果：生命 40/40，Credit 8。
- 1.8 秒成功冷却生效。
- 体量 2 网格底与地面误差 2.0 cm。

修正记录：融合移动最终航点必须使用 10 cm 到达半径；探索默认的 70 cm 会让双方在接触前停止。

## 位移撮合到体量 8

```text
命令：SlimeRunTimeScale 0,SlimeEcologyForcedFusionTest
日志：Saved/Logs/M4-ForcedFusion-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00037.png
```

- 体量 4 + 体量 4 通过外力扫掠立即融合。
- 结果：体量 8，生命 160/160，Cargo 0，Credit 40。
- 成功冷却读取 0.90 秒。
- 体量 8 网格底与地面误差 2.0 cm。

## 打断与 Blocked 中玩家撮合

```text
命令：SlimeRunTimeScale 0,SlimeEcologyInterruptTest
日志：Saved/Logs/M4-InterruptFusion-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00040.png
```

- Contacting 阶段打断后双方状态为 Blocked，剩余禁用 4.59 秒。
- Blocked 3.59 秒时施加玩家外力，仍成功融合为体量 2。
- 融合后进入 1.8 秒成功冷却。
- 证明 5 秒禁用只阻止自主/接触融合，不阻止玩家外力撮合。

## 其他规则

- 自主/接触融合都通过 `CanPairWith` 检查自动体量上限 2。
- 融合结果使用 `ApplyStatRow` 更新生命与尺寸。
- 伙伴死亡、分离和超时都会清理配对。
- 三只同时接触时由互锁握手保证每只只能进入一个配对；该场景将在手动 Checklist 中观察。

## 待手动验证

- 真实玩家身体推动小史莱姆触发撮合。
- 大体量阻挡和接触手感。
- 2.4 秒接触、1.8 秒冷却、5 秒禁用的可见反馈。
- 融合后无重叠、弹飞、穿墙或悬空。

对应清单：`Docs/EcologyRebuild/M4-Checklist.md`。

## 用户手动验收

- 三只体量 1 同时准备：只有两只形成 Contacting 配对，第三只保持 Idle；最终一只体量 2 + 一只体量 1。
- 玩家身体推动撮合正常。
- 受伤融合：10/20 + 20/20 → 体量 2，生命 30/40。
- 融合后 Credit 8，Cargo 0。
- 融合后冷却读取 0.28 秒并归零。
- 体量 2 贴地误差 2.0 cm；解除锁定后恢复 Explore，速度 115 cm/s。
- 体量 4+4 → 体量 8，生命 160/160，Credit 40，贴地误差 2.0 cm。

## 头顶调试状态

- `Slime.Debug.DrawEcologyState` 默认开启。
- 生态史莱姆头顶显示 4 行状态，并带半透明底板。
- 可实时查看融合状态/路径、接触进度、准备/冷却/禁用、体量、生命、Cargo、Credit、大脑、身体、眩晕、外力、速度和位置。
- 验证截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00049.png`。
