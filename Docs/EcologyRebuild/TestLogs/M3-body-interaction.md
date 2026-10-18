# M3 身体交互自动测试记录

- 日期：2026-10-18
- 分支：`rebulid-program`
- 状态：`Passed`；自动测试和用户手动验收均通过

## 编译

```text
Build.bat SlimeWarEditor Win64 Development -Project="E:\UE\Unreal Projects\SlimeWar\SlimeWar\SlimeWar.uproject"
```

结果：0 error。

## 自动身体测试

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeEcologyBodyTest
日志：Saved/Logs/M3-Body-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00026.png
```

阶段结果：

| 时间 | 状态 | 外部速度 | 实际速度 | Stun Pending | Stun Remaining |
|---|---:|---:|---:|---:|---:|
| 0.4 s | `StunPending` | 198.1 cm/s | 203.2 cm/s | 1.50 s | 0 |
| 1.2 s | `Stunned` | 0 | 0 | 0 | 1.19 s |
| 3.2 s | `Normal` | 0 | 115 cm/s | 0 | 0 |

结论：

- 滑行期间待眩晕不消耗。
- 停稳后进入眩晕。
- 眩晕结束后恢复自主移动。
- 全程没有调用 `DisableMovement()`。

## 瞄准锁定

在身体测试后对同一目标开启瞄准锁定 1.5 秒：

- 状态：`AimLocked`
- 实际速度：0
- 位置变化：1.2 cm

结论：锁定停止自主移动，同时身体组件仍保持独立外力处理路径。

## 手动测试确认

- 用户确认体量 1 身体推动正常。
- 用户确认 Checklist 第 4–9 项目测或日志通过。
- 第 3 项状态日志中 `external=0`、`bodyPushWindow=0` 是检查时已经停止推动、阻尼完成或 3 秒窗口过期后的正常结果。

对应清单：`Docs/EcologyRebuild/M3-Checklist.md`。
