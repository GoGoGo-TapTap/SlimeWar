# R1 玩家控制自动测试记录

- 日期：2026-10-19
- 状态：自动测试通过，手动输入 Checklist 待执行

## 编译

`SlimeWarEditor Win64 Development` 编译 0 error。

## 新增字段

只向 DoublePoint 和 ThreePoint RunConfig 新增：

- PlayerSprintSpeed = 900 cm/s
- PlayerAimSpeed = 300 cm/s
- PlayerJumpSpeed = 700 cm/s
- PlayerGravity = 2000 cm/s²
- CameraAimBoomLength = 220 cm
- CameraAimFieldOfView = 55°

资产更新日志确认以下既有值未改变：

- PlayerMoveSpeed = 600
- PlayerAcceleration = 2048
- CameraBoomLength = 220
- CameraFieldOfView = 0（运行时引擎默认 90）

## 自动状态测试

地图：`L_Ecology_DoublePoint`

日志：`Saved/Logs/R1-PlayerControls.log`

截图：`HighresScreenshot00096.png`

| 状态 | MaxWalkSpeed | FOV | 结果 |
|---|---:|---:|---|
| 初始 | 600 | 90 | 正常 |
| 冲刺 | 900 | 90 | 通过 |
| 冲刺中瞄准 | 300 | 55 | 瞄准优先 |
| 结束瞄准 | 900 | 90 | 恢复冲刺 |
| 结束冲刺 | 600 | 90 | 恢复步行 |
| Q 换肩 | 600 | 90 | SocketY 60 → -60 |

## 跳跃测试

日志：`Saved/Logs/R1-Jump.log`

截图：`HighresScreenshot00097.png`

结果：

- 请求跳跃后 0.2 秒：`falling=yes`，竖直速度 -100 cm/s。
- 随后阶段：`falling=no`，竖直速度 0，角色已落地。
- JumpZVelocity=700，GravityScale=2.041。

## 结论

R1 自动状态、相机、换肩和跳跃落地路径通过。真实 Shift/Space/右键/Q 输入手感仍需 `R1-Checklist.md`。
