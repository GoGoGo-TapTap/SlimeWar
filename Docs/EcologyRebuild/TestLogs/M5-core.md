# M5 核心、跃击、吞噬、爆裂与弱点自动测试记录

- 日期：2026-10-19
- 分支：`rebulid-program`
- 状态：自动测试与用户手动 Checklist 均通过

## 编译

```text
Build.bat SlimeWarEditor Win64 Development -Project="E:\UE\Unreal Projects\SlimeWar\SlimeWar\SlimeWar.uproject"
```

结果：0 error。

## 融合生成中性核心

```text
命令：SlimeRunTimeScale 0,SlimeEcologyFusionTest
日志：Saved/Logs/M5-FusionCore.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00054.png
```

- 体量 1+1 融合为体量 2。
- 日志出现 `Core: SlimeEcologyEnemy_0 state -> Neutral`。
- 中性核心继续 Explore，不追踪玩家。
- 体量 2 贴地误差 2.0 cm。

## 跃击

```text
命令：SlimeRunTimeScale 0,SlimeEcologyPounceTest
日志：Saved/Logs/M5-Pounce-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00055.png
```

- 敌对核心依次经历 Chasing → Windup → Pouncing → Recovering。
- 前摇时间约 0.65 秒。
- 每次命中玩家 20 伤害。
- 实际两次命中间隔约 2.6 秒，由 1.6 秒攻击冷却、追击和前摇共同决定。
- 连续 5 次命中后玩家死亡。
- HUD 显示 `core Hostile/Windup`、目标名、前摇和冷却。

## 吞噬

```text
命令：SlimeRunTimeScale 0,SlimeEcologyDevourTest
日志：Saved/Logs/M5-Devour.log
```

```text
before: attacker mass=2 hp=40 credit=8 cargo=0; victim mass=2 credit=8 cargo=0
after:  attacker mass=4 hp=50 credit=16 cargo=0
```

- 吸收体量 2，体量从 2 到 4。
- 恢复生命 2×5=10。
- BodyScoreCredit 从 8 继承到 16。
- 攻击者 Cargo 保留。

## 爆裂与连锁

```text
命令：SlimeRunTimeScale 0,SlimeEcologyDetonationTest
日志：Saved/Logs/M5-Detonation-Visible.log
```

- 第一个体量 2 核心被玩家击杀。
- 日志出现第一个核心 `CoreExplosion ... radius 350`。
- 120 cm 内的第二个核心直接进入连锁。
- 日志出现第二个核心 `CoreExplosion ... radius 350`。
- 两个核心最终均为 dead/pending kill。
- 普通旁观个体生命 20→0，获得 800 cm/s 径向推力。
- 每个受影响目标输出伤害前后生命和实际推力日志。
- 场景绘制 0.8 秒橙色爆裂范围球、黄/红命中连线和目标红圈。

## 追踪路径修复

用户反馈敌对核心会在玩家绕圈时沿历史路径绕圈。

根因是移动组件每帧覆盖缓存 `Goal`，但只有目标变化超过 25 cm 才重建路径，导致目标一直在变而路径指向旧点。

修正为只有达到重建阈值时同时更新缓存目标和路径。玩家改变方向后，核心会重新朝当前目标位置追击。

## 弱点

```text
命令：SlimeRunTimeScale 0,SlimeEcologyWeakpointTest
日志：Saved/Logs/M5-Weakpoint.log
```

- 体量 6：`weakpoint=no`。
- 体量 7：`weakpoint=yes`。
- 0 伤害命中：生命保持 140/140。
- 1 伤害弱点命中：立即处决。
- 弱点只阻挡武器 Visibility，不参与身体物理碰撞。

## 用户手动验收

- 真实玩家瞄准弱点并开枪处决：通过。
- 跃击方向锁定、空中轨迹和单次命中观感：通过。
- 低血核心撤退：通过。
- 墙后目标不被爆裂伤害：通过。
- 单核心爆裂和双核心连锁的视觉与反馈：通过。
- 玩家绕圈时直接追击、不沿历史路径绕圈：通过。

对应清单：`Docs/EcologyRebuild/M5-Checklist.md`。
