# M1 史莱姆贴地验收记录

- 日期：2026-10-18
- 分支：`rebulid-program`
- 基线提交：`557a2de`
- 目标：修正 UE 胶囊中心原点与 Godot 脚底原点不一致导致的史莱姆悬空。

## 实现摘要

- `ASlimeEnemyBase::ApplyBodyRadius` 现在把胶囊设置为 `Radius == HalfHeight` 的球形身体。
- 改尺寸前保存脚底世界 Z，改尺寸后恢复脚底位置，避免胶囊缩短时角色悬空。
- 静态网格按包围盒缩放到 `2 × BodyRadius` 的视觉直径，并按网格自身 pivot 偏移到底部与胶囊底对齐。
- `ApplyStatRow` 先设置网格，再统一执行胶囊与视觉对齐；出生、调试改体量和融合继承使用同一路径。
- 数据表行没有配置网格时回退到 `/Game/Characters/Slime/SM_Slime_Base`，避免生成不可见个体。
- 新增 `SlimeDumpBodyMetrics` 和 `SlimeGroundingTest` 调试命令，供后续模块重复使用。

修改文件：

- `Source/SlimeWar/Public/Enemy/SlimeEnemyBase.h`
- `Source/SlimeWar/Private/Enemy/SlimeEnemyBase.cpp`
- `Source/SlimeWar/Public/Debug/SlimeCheatManager.h`
- `Source/SlimeWar/Private/Debug/SlimeCheatManager.cpp`

## 编译

命令：

```text
Build.bat SlimeWarEditor Win64 Development -Project="E:\UE\Unreal Projects\SlimeWar\SlimeWar\SlimeWar.uproject"
```

结果：编译和链接成功，0 error。

## 引擎实测

测试方式：启动可见的 UE 5.5 `UnrealEditor.exe -game` 窗口，加载真实地图，通过控制台命令生成史莱姆并输出量化数据。

### 数据结果

地图：`/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza`

日志：`Saved/Logs/M1-Whitebox-final.log`

| 状态 | 体量 | 胶囊半径 | 胶囊半高 | 视觉缩放 | 胶囊底误差 | 网格底误差 | 水平偏心 |
|---|---:|---:|---:|---:|---:|---:|---:|
| 初始 | 1 | 20.0 cm | 20.0 cm | 0.681 | 2.0 cm | 2.0 cm | 0.0 cm |
| 初始 | 4 | 37.0 cm | 37.0 cm | 1.259 | 2.0 cm | 2.0 cm | 0.0 cm |
| 初始 | 8 | 60.0 cm | 60.0 cm | 2.042 | 2.0 cm | 2.0 cm | 0.0 cm |
| 运行时体量 1 → 8 | 8 | 60.0 cm | 60.0 cm | 2.042 | 2.0 cm | 2.0 cm | 0.0 cm |

地面高度：`-0.5 cm`；所有测试体在调整前后均保持约 `2 cm` 离地间隙，没有下陷。

### 截图证据

- 体量 1 / 4 / 8 初始可见状态：
  `Saved/Screenshots/WindowsEditor/HighresScreenshot00009.png`
- 体量 1 在运行时变为体量 8：
  `Saved/Screenshots/WindowsEditor/HighresScreenshot00010.png`
- `show collision` 下的胶囊/网格对齐：
  `Saved/Screenshots/WindowsEditor/HighresScreenshot00005.png`
  `Saved/Screenshots/WindowsEditor/HighresScreenshot00006.png`

截图确认三种体量均落在地面上，视觉直径随体量变化，改变体量后脚底位置不跳变。

## 回归检查

- `L_Sandbox_OnePoint`：同样得到体量 1/4/8 胶囊底和网格底误差均为 `2.0 cm`，水平偏心 `0.0 cm`。
- 日志：`Saved/Logs/M1-Sandbox-grounding.log`
- 运行时从体量 1 改到 8 后，脚底误差仍为 `2.0 cm`。

## 融合路径说明

使用旧 `SlimeForceFuse` 在 `-game` 空闲态测试时，两只体量 1 在 2.5 秒后仍处于旧 StateTree 接近阶段，没有完成融合。该流程属于计划中 M4 要替换的旧玩法逻辑，不作为 M1 贴地实现的替代验收项。

M1 的体量变化路径已通过 `ApplyStatRow` 的运行时 1 → 8 验证；后续 M4 完成新融合组件后，必须再次验证融合体量 2/8 的脚底误差。

## 结论

M1 通过：史莱姆悬空问题已修复，体量 1/4/8 和运行时体量变化均保持稳定贴地。可以进入 M2 前等待模块门禁确认。
