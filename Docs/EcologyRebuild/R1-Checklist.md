# R1 玩家控制 · 编辑器手动验收

## 1. 启动

1. 打开 `/Game/_SlimeWar/Maps/L_Ecology_DoublePoint`。
2. PIE 后执行：

```text
SlimeRunStart
SlimeRunTimeScale 0
```

## 2. 步行与冲刺

- [ ] 正常 WASD 移动，`PlayerControl` 显示 `maxWalk=600`。
- [ ] 按住 Shift 移动，`maxWalk=900`。
- [ ] 松开 Shift 回到 600。
- [ ] 冲刺时没有穿墙或异常加速。

## 3. 瞄准减速与优先级

- [ ] 按住右键，`aim=yes`、`maxWalk=300`。
- [ ] 冲刺中按住右键，速度仍为 300，瞄准优先于冲刺。
- [ ] 松开右键后，如果 Shift 仍按住，速度恢复 900。
- [ ] 瞄准时相机距离和 FOV 切换；退出后恢复。

## 4. 跳跃

- [ ] 按 Space 可以起跳。
- [ ] 空中 `falling=yes`，竖直速度受重力变化。
- [ ] 落地后可以再次跳。
- [ ] 空中再次按 Space 不会二段跳。

## 5. 换肩

- [ ] 按 Q 后相机换到另一侧，`socketY` 正负变化。
- [ ] 再按 Q 恢复原侧。
- [ ] 换肩不影响射击方向和移动方向。

## 6. 数值冲突检查

- [ ] 没有修改 `PlayerMoveSpeed=600`。
- [ ] 没有修改 `PlayerAcceleration=2048`。
- [ ] 没有修改 `CameraBoomLength=220`。
- [ ] 没有修改 `CameraFieldOfView=0`。

## 7. 自动复现

```text
SlimePlayerControlStatus
SlimePlayerSprint 1
SlimePlayerControlStatus
SlimePlayerAim 1
SlimePlayerControlStatus
SlimePlayerAim 0
SlimePlayerJumpTest
SlimePlayerSwitchShoulder
SlimePlayerControlStatus
```

## 8. 结论

- [ ] 全部通过
- [ ] 存在问题：

```text

```
