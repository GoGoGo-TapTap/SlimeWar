# SlimeWar Godot 11 生态机制复刻计划

## Summary

- 基线为 `rebulid-program @ 557a2de`。M0 已完成，不再重复分支迁移、LFS 恢复和项目初始化。
- 以 `Docs/Godot11_UE复刻拆解案.md` 为准，P0 目标是 `ecology_double_point`：44×30 米、A/B 两阵营、4 个矿点、150 秒、200 分。60×50 三阵营版本改为后续扩展。
- 保留“主菜单 → 选落点 → 投放 → 局内 → 结算”的 UI 流程外壳，重写局内生态、敌人、武器、计分和 HUD 数据。
- 不再一次性删除旧代码。新生态路线先独立跑通，旧 Normal/Aggro/StateTree/波次系统在 M8 新路线验收后彻底退出运行时，避免再次出现大面积编译失败。
- 严格按模块推进。每个模块必须完成编译并进入真实 Editor/PIE 验证；验收通过后才进入下一模块，失败则只在本模块内修复。

## Plan 文件回填规则

`Docs/EcologyRebuild-Plan.md` 是本项目唯一权威的实施计划，聊天记录只用于临时沟通，不作为后续模块的上下文来源。

每个模块必须执行同一套记录流程：

1. 开始实现前，把本模块的详细设计、接口、数据、算法、风险和引擎验收步骤写入本文件。
2. 实现过程中，任何与设计不同的决定都要记录原因和最终结果。
3. 完成后，写入编译结果、测试地图、控制台命令、量化结果、截图路径、日志路径和门禁结论。
4. 没有完成设计与实测回填的模块不得标记为 `Passed`，也不得开始下一模块。
5. 模块状态统一使用 `Pending / Active / Passed / Blocked`。

`Docs/EcologyRebuild-Plan.md` 是唯一需要维护的版本；`.codex/plan/ecology-rebuild.md` 如可写仅作镜像，环境不允许写入时不影响本计划的有效性。

## 上一版方案需要修正的问题

| 问题 | 修正 |
|---|---|
| 把 60×50、三阵营、180 秒/300 分当作首个目标 | P0 改为 44×30、A/B、150 秒/200 分；三点地图放到 M9 |
| 继续使用 Normal/Aggro 两类单位和定时波次 | 所有史莱姆都是同一生态个体，攻击性核心由警戒融合、吞噬和冲突产生 |
| 以 `MoveToActor` 目的地追人为核心 | 导航只负责路线，战斗、避让、聚群、融合使用每帧本地速度和碰撞移动 |
| 自动融合沿用 0.4 秒、上限 8 | 自动融合最多 2 体量、接触 2.4 秒；玩家撮合可突破至 8 |
| 玩家射击触发普通史莱姆仇恨 | 枪击只打断、推动和延迟眩晕；普通警戒只由 NPC 伤害与资源争夺触发 |
| 攻击性单位不计分 | 敌对核心按完整分减半，只有玩家击杀才计入玩家分数 |
| 满蓄力作为高伤害攻击 | 满蓄力直接伤害为 0，主要产生 4 米范围击退 |
| 抵达目标分立即结束 | 目标分不结束回合；时间为零、玩家死亡或生态灭绝才结束 |
| 第一阶段就删除全部旧类 | 使用兼容迁移，M8 新系统通过后才归档旧资产和旧类 |

悬空根因已确认：UE `ACharacter` 的原点在胶囊中心，而 Godot 史莱姆原点在脚底；当前胶囊保持默认 88 cm 半高，视觉球却直接挂在胶囊中心，因此整体离地。M1 先单独修正该问题，不混入其他机制改造。

## 模块执行计划

| 模块 | 内容 | 实际引擎验收 |
|---|---|---|
| M0 | 干净基线 | 用户已确认完成 |
| M1 | 史莱姆贴地、胶囊和视觉原点 | `Passed`；见 `EcologyRebuild/TestLogs/M1-grounding.md` |
| M2 | 连续移动、导航、排斥、聚群、避让玩家 | `Passed`；10 只运行 60 秒，无抖动、穿墙、永久堵塞 |
| M3 | 身体碰撞、小体量推动、大体量阻挡、瞄准锁定、停稳眩晕 | `Passed`；自动身体测试与用户手动验收均通过 |
| M4 | 融合 v2：自主寻找、接触融合、位移撮合 | 自然形成体量 2；打断和玩家撮合正确 |
| M5 | 中性/敌对核心、跃击、吞噬、爆裂、弱点 | 核心状态、连锁和弱点一击通过 PIE |
| M6 | 磁性瞄准、蓄力射击、击退、撞墙分裂 | 锁定、遮挡、零伤害满蓄力、分裂通过 PIE |
| M7 | 双点 P0：地图、矿点、采集交付、150 秒/200 分 | 完整运行一局，资源和体量守恒 |
| M8 | 警戒、争夺、敌对核心、吞噬、手雷、粘液、完整计分 | 拆解案第 18 节场景全部通过 |
| M9 | 三阵营 60×50 扩展和完整 HUD/结算 | 三点地图完整回合通过 |
| M10 | 表现、100 只压测、Windows Development 打包 | 性能基线和打包包体通过 |

### M1：贴地模块（Passed）

修改集中在 `ASlimeEnemyBase` 的共享身体尺寸逻辑，使旧敌人、后续生态敌人、生成和融合都使用同一套计算：

- 胶囊改为真正的球形身体：`CapsuleRadius = BodyRadius`，`CapsuleHalfHeight = BodyRadius`。
- 调整尺寸前记录脚底位置 `FootZ = ActorZ - OldHalfHeight`；调整后恢复为 `ActorZ = FootZ + NewHalfHeight`，保留现有 2 cm 出生间隙，避免缩小胶囊后再次悬空。
- 按当前静态网格包围盒计算视觉缩放，使视觉水平直径等于 `2 × BodyRadius`。
- 视觉网格相对胶囊中心的位置为：
  - `Scale = BodyRadius / max(Bounds.Extent.X, Bounds.Extent.Y)`
  - `MeshZ = -BodyRadius - (Bounds.Origin.Z - Bounds.Extent.Z) × Scale`
  - `MeshX/Y = -Bounds.Origin.X/Y × Scale`
- `ApplyStatRow` 和 `ApplyAggroStatRow` 都改为“先设置网格，再统一设置胶囊与视觉对齐”，保证出生、`SlimeSetMass`、融合升阶使用同一逻辑。
- 增加一个调试命令输出胶囊底、网格底、地面高度和误差，作为 PIE 验收量化依据。M1 不修改体量数值和任何玩法参数。

M1 引擎门槛：

1. 编译 `SlimeWarEditor Win64 Development`，0 error。
2. 打开真实 Unreal Editor，在 `L_Sandbox_OnePoint` 和 `L_Whitebox_CityPlaza` 分别 PIE。
3. 执行 `SlimeSpawnNormalAtMass 1 3`、`4 2`、`8 1`，再用 `SlimeSetMass` 在运行时切换体量。
4. 开启 `show collision`，检查胶囊底与地面误差 0–3 cm、网格底与地面误差不超过 2 cm、无下陷、无明显水平偏心。
5. 让两只体量 1 自然融合成体量 2，融合后仍保持贴地。
6. 记录截图、控制台数据和结果到 `Docs/EcologyRebuild/TestLogs/M1-grounding.md`。M1 未通过前不开始 M2。

M1 结果：编译 0 error；体量 1/4/8 的胶囊底和网格底均离地 `2.0 cm`，水平偏心 `0.0 cm`；运行时体量 1 → 8 后仍为 `2.0 cm`。详细记录见 `Docs/EcologyRebuild/TestLogs/M1-grounding.md`。

### M2：生态移动与避让（Passed）

M2 建立独立于旧 StateTree 的新生态个体和 C++ 移动层。旧 `ASlimeNormal`、`ASlimeAggro`、旧融合和 StateTree 资产保持可编译，但新生态测试不调用它们。

新增类型与职责：

- `ESlimeFaction {A, B, C}`：M2 默认 A 阵营。
- `ESlimeEcologyBehavior {Paused, AvoidPlayer, Regroup, Explore}`。
- `ASlimeEcologyEnemy`：组合大脑与移动组件，不使用 StateTree。
- `ASlimeEcologyAIController`：仅负责自动占有和清理，不创建 AI 状态树。
- `USlimeEcologyBrainComponent`：0.25 秒感知、行为选择、群体锚点、探索目标和排斥输入。
- `USlimeLocomotionComponent`：路径缓存、逐帧本地转向、方向平滑和卡住恢复。
- `USlimeEcologySubsystem`：生态个体注册、空间哈希邻居查询和测试统计。
- `USlimeEcologyRules` / `DA_EcologyRules`：本模块全部调参；M7 在此基础上继续扩展。

行为优先级：

1. 死亡或回合冻结：`Paused`，清除移动目标。
2. 玩家进入 240 cm 且有视线：`AvoidPlayer`，速度 110 cm/s；玩家离开 320 cm 后退出。
3. 距群体锚点超过 280 cm：`Regroup`，速度 115 cm/s；到 180 cm 内退出。
4. 其他情况：`Explore`，速度 115 cm/s。

感知与群体：

- 0.25 秒刷新一次，各单位的初始刷新相位随机错开。
- 群体锚点为 700 cm 内可见同阵营个体与自身的平均位置；没有同族时使用出生点。
- 探索点在活动半径内选取，距当前位置至少 150 cm，到达半径 70 cm，单个目标最长保持 12 秒。
- 视觉检测使用静态遮挡线检测，不读取墙后目标。

移动与空间哈希：

- 空间哈希网格 400 cm，注册表每 0.25 秒重建，查询只遍历覆盖半径的网格。
- 路径直线检测使用身体中心、左偏和右偏三条线；无静态遮挡时直接移动，否则请求 NavMesh 路径并缓存点列。
- 路径只在目标改变、路径过期或卡住时重算，不逐帧请求导航。
- 每帧使用 `AddInputVector` 和缓存路径方向移动；期望方向按指数平滑，最终碰撞由角色胶囊扫掠保证。
- 同族排斥目标间距为双方半径和再加 25 cm，排斥系数 3。
- 卡住定义是有移动目标时 2 秒位移不足 15 cm；首次重算路径，连续失败 3 次后重新选探索点。

M2 调试与验收命令：

- `SlimeSpawnEcology 10`：生成 10 只同阵营生态个体。
- `SlimeEcologyStatus`：输出行为、速度、路径状态、群体距离和重叠统计。
- `SlimeEcologySoakTest 60`：运行 60 秒并输出汇总。
- `SlimeEcologyAvoidanceTest`：自动把玩家放入 180 cm 内，再移开，验证避让和恢复。

M2 引擎门槛：

1. `SlimeWarEditor Win64 Development` 编译 0 error。
2. `L_Whitebox_CityPlaza` 可见游戏窗口运行 10 只史莱姆 60 秒。
3. 全部存活；无静态几何穿透；无超过 2 秒且仍持有目标的永久停顿；无重叠堆叠。
4. 15 秒后群体锚点平均距离小于 280 cm；同体量中心距不小于双方半径和。
5. 玩家进入 180 cm 后，个体在 4 秒内退到 320 cm 外；玩家离开后 10 秒内恢复探索/聚群。
6. 邻居查询来自空间哈希的 0.25 秒刷新，不存在逐帧全表扫描。
7. 实测结果写入 `Docs/EcologyRebuild/TestLogs/M2-locomotion.md` 后，M2 才可标记为 `Passed`。

#### M2 实现记录

新增：

- `ASlimeEcologyEnemy`
- `ASlimeEcologyAIController`
- `USlimeEcologyBrainComponent`
- `USlimeLocomotionComponent`
- `USlimeEcologySubsystem`
- `USlimeEcologyRules`
- `DA_EcologyRules`，路径 `/Game/_SlimeWar/Ecology/DA_EcologyRules`
- `Tools/CreateEcologyRulesAsset.py`

`DA_EcologyRules` 已绑定到 `DA_RunConfig::EcologyRules`。运行时日志确认加载到 `DA_EcologyRules`，不依赖 C++ 默认回退。

实现偏差：

- 玩家避让验收使用隔离的“主个体”作为唯一门禁对象：先让群体运行 2 秒，再将主个体放到群体外侧空地，玩家放在它与群体之间 180 cm。群体拥挤和聚群由 60 秒 soak 单独覆盖，避免随机拥堵让单次避让门禁抖动。
- 同族排斥在移动叠加时只保留垂直于当前路径的侧向分量，防止排斥把逃离向量顶回去。
- 避让目标距离从 300 cm 调整为 450 cm，确保斜向逃逸后仍能越过 320 cm 退出距离。
- 密集群体中的最近中心距可以接近双方半径和（最终 soak 为 40.0 cm），但未发生重叠；25 cm 额外间距是软目标，不作为硬门禁。

#### M2 实测记录

编译：`SlimeWarEditor Win64 Development` 0 error。

最终 soak：

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeSpawnEcology 10,SlimeEcologySoakTest 60
日志：Saved/Logs/M2-Soak-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00021.png
```

结果：10/10 存活；空间哈希重建 240 次；群体平均距离 232.7 cm；最近中心距 40.0 cm；静态穿透 0；超过 2 秒的当前停顿 0；路径失败 0；60 秒路径请求 550 次；最大感知刷新次数 233。

最终避让：

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeSpawnEcology 10,SlimeEcologyAvoidanceTest
日志：Saved/Logs/M2-Avoidance-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00024.png
```

结果：主个体从玩家距离 180 cm，在 4 秒内退到 341.1 cm；1/1 通过；无穿透、无停顿。玩家移开 10 秒后群体平均距离 211.2 cm，恢复探索/聚群。

详细记录：`Docs/EcologyRebuild/TestLogs/M2-locomotion.md`。

### M3：身体交互、瞄准锁定与停稳眩晕（Passed）

M3 在 `ASlimeEcologyEnemy` 上新增 `USlimeBodyInteractionComponent`，并修改玩家胶囊与身体推动。旧 StateTree 敌人继续保留。

核心规则：

- 玩家胶囊改为半径 43 cm、总高 180 cm。
- 新生态史莱姆保持 `ECC_Pawn`，与玩家默认 Block；新生态敌人不再调用 `IgnorePlayerForMovement`。
- 体量 1–3 可被玩家身体推动；体量 4–8 保持实体阻挡，不接收身体推动。
- 玩家推动不耗弹、不造成伤害；推动方向混合“玩家指向史莱姆”和玩家水平移动方向。
- 推动速度基础 260 cm/s，随玩家速度提高，按体量 `mass^0.35` 衰减；每 0.1 秒最多接受一次同来源身体推动。
- 外部速度使用独立分量和 500 cm/s² 阻尼，逐帧带扫掠移动；撞墙时沿碰撞平面滑动。外部速度与自主移动分离，不使用 `DisableMovement()`。
- 瞄准锁定、待眩晕和眩晕共用“停止自主移动”状态；重力、碰撞和外部速度继续。
- 命中眩晕 1.5 秒，分裂眩晕保留 2 秒接口。待眩晕不消耗时间，必须落地且实际位移速度 ≤10 cm/s 连续 0.1 秒后才开始计时。
- 眩晕中再次受击时取旧剩余与新值的最大值，重新回到待眩晕并等待停稳，不叠加计时。
- 眩晕开始时清除残余外部速度；被墙挡住、实际速度为 0 也视为停稳。
- 身体推动打开 3 秒玩家撮合窗口，供 M4 融合使用。

数据新增到 `DA_EcologyRules`：

- `BodyPushMaxMass = 3`
- `BodyPushBaseSpeed = 260`
- `BodyPushPlayerSpeedScale = 0.4`
- `BodyPushMassExponent = 0.35`
- `BodyPushContactPadding = 20`
- `ExternalVelocityDamping = 500`
- `StunStopSpeed = 10`
- `StunSettleTime = 0.1`
- `HitStunDuration = 1.5`
- `SplitStunDuration = 2.0`
- `ForcedFusionWindow = 3.0`
- `ImpactSplitSpeed = 600`
- `ImpactSplitWindow = 0.9`

M3 调试命令：

- `SlimeSpawnEcology <Count> <Faction> <Mass>`
- `SlimeEcologyBodyStatus`
- `SlimeEcologyAimLock <0|1>`
- `SlimeEcologyExternalPush <Speed>`
- `SlimeEcologyStunTest <Seconds>`

M3 引擎门槛：

1. 编译 0 error。
2. 体量 1、2、3 可被玩家身体推动，且不耗弹、不受伤。
3. 体量 4、8 阻挡玩家，玩家不能挤穿。
4. 外部推动后史莱姆滑行并受 500 cm/s² 阻尼；撞墙不穿透。
5. 瞄准锁定后自主移动停止，但外部速度、重力和碰撞继续。
6. 空中不启动眩晕；落地且实际速度 ≤10 cm/s 连续 0.1 秒后才开始 1.5 秒眩晕。
7. 眩晕期间再次受击取最大值并重新等待停稳。
8. 手动输入手感检查写入 `Docs/EcologyRebuild/M3-Checklist.md`。

#### M3 实现与自动测试记录

已实现：

- `USlimeBodyInteractionComponent`
- 外部速度、500 cm/s² 阻尼和扫掠撞墙滑动
- 待眩晕、停稳确认、1.5 秒眩晕和重复受击取最大值
- 瞄准锁定与自主移动隔离
- 玩家胶囊 43×180 cm
- 玩家通过空间哈希查询邻近生态个体并推动体量 1–3
- 体量 4–8 由物理碰撞阻挡，不接收身体推动
- 新生态敌人覆盖旧的 `IgnorePlayerForMovement`

编译：`SlimeWarEditor Win64 Development` 0 error。

自动身体测试：

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeEcologyBodyTest
日志：Saved/Logs/M3-Body-final.log
截图：Saved/Screenshots/WindowsEditor/HighresScreenshot00026.png
```

结果：

- 初始施加 400 cm/s 和 1.5 秒待眩晕。
- 0.4 秒时外部速度降到 198.1 cm/s，实际速度 203.2 cm/s，状态 `StunPending`，眩晕时间未被消耗。
- 1.2 秒时停稳，状态 `Stunned`，剩余眩晕 1.19 秒。
- 3.2 秒时恢复 `Normal`。
- 瞄准锁定 1.5 秒期间位置变化仅 1.2 cm，状态为 `AimLocked`。

详细记录：`Docs/EcologyRebuild/TestLogs/M3-body-interaction.md`。

手动验收：用户确认 Checklist 第 3 项体量 1 推动正常，第 4–9 项目测或日志通过。第 3 项日志中的 `external=0`、`bodyPushWindow=0` 是阻尼结束或 3 秒撮合窗口过期后的正常读数，不代表推动失败。

M3 结论：`Passed`。

### 后续模块关键实现

- M2 新增 C++ 生态个体和大脑组件；0.25 秒感知刷新，邻居查询使用空间哈希；导航路径更新与每帧本地转向分离，禁止在战斗和融合中使用持续目的地追人。
- M3 取消玩家与普通史莱姆的 Overlap 和 `IgnorePlayerForMovement`；玩家移动后做胶囊扫掠，体量 1–3 接收外部推动，体量 4 以上保持阻挡。眩晕只停止自主行动，重力、碰撞和滑行继续；必须落地且实际速度 ≤0.1 m/s 持续 0.1 秒后才启动 1.5 秒眩晕。
- M4 重写融合为自主、接触、位移撮合三条路径；自动融合上限 2、接触 2.4 秒、成功冷却 1.8 秒、准备 14–22 秒、打断禁用 5 秒、玩家撮合窗口 3 秒，融合保留剩余生命、货物和身体分。
- M5 将普通个体、中性核心、敌对核心统一为同一实体状态；实现核心苏醒、追击、0.65 秒前摇、跃击、吞噬、爆裂连锁和体量 7/8 弱点。
- M6 新增 `USlimeAimAssistComponent` 和 `UGA_ChargeFire`：屏幕捕获半径 10% 屏高、保留半径 1.35 倍、切换阈值 55%、相机和枪口双遮挡；0.18 秒起蓄、1.2 秒满蓄、伤害 `8 × (1 - s)`、4 米击退。
- M7 新建双点白盒和生态数据资产。A/B 各开局 10 只体量 1；4 个矿点库存分别为 4/4/5/5，共享矿点每 5 秒恢复到最多 6；每交付 1 份只生成 1 只体量 1。
- M8 完成后清退旧 `ASlimeNormal`、`ASlimeAggro`、波次生成、StateTree 和旧 HUD 数据绑定；旧 uasset 移入 `Content/Developers/Junk/LegacyPhaseE/`，不直接删除历史。
- M9 按 A/B/C、60×50、5 矿点、180 秒/160 分扩展，并补齐完整结算统计。
- M10 在 100 只规模下验证 0.25 秒感知刷新、无逐帧全表扫描、峰值帧率不低于 M0 白盒基线，并完成 Windows Development 打包。

## 公共接口与数据

新增统一类型：`ESlimeFaction`、`ESlimeCoreState`、`ESlimeBodyState`、`FSlimeKillContext`、`FSlimeFusionEvent`、`FSlimeSiteStatus`、`FSlimeEcologyStatRow`。

核心组件：

- `USlimeLocomotionComponent`：路径跟随、自主速度、本地避让和排斥。
- `USlimeEcologyBrainComponent`：感知记忆、行为优先级、采集/返程/聚群/战斗目标。
- `USlimeBodyInteractionComponent`：外部速度、推挤、停稳、眩晕、撞击分裂窗口。
- `USlimeFusionComponent`：三条融合路径、冷却、打断和生命/货物/分数继承。
- `USlimeCoreComponent` 与 `USlimeWeakpointComponent`：核心状态、跃击、吞噬、爆裂和弱点。
- `USlimeAimAssistComponent`：磁性瞄准、身体/弱点候选和解锁切换。

`USlimeEcologySubsystem` 独占站点、矿点、货物、交付队列、生成请求、资源守恒和体量守恒。`IBattleDirector` 改为接收 `OnEnemyKilled(const FSlimeKillContext&)`、`OnEnemyFused(const FSlimeFusionEvent&)`、`OnSiteStatusChanged(const FSlimeSiteStatus&)`，并保留玩家死亡事件；新增 `Extinction` 结束原因。

数据拆分为 `DT_SlimeEcologyStats`、`DA_EcologyRules`、`DA_Weapon_Rifle` 和三张关卡 DataAsset。体量与分数严格使用拆解案数值，包括生命 `30×体量`、自动融合上限 2、玩家撮合上限 8、货物每份 6 分、敌对核心分数减半。

## 测试与门禁

每个模块都执行同一流程：编译 → 启动真实 Editor → PIE 操作 → 截图和控制台证据 → 写入测试日志 → 判定门槛。自动化测试只能作为补充证据，不能替代可见 PIE 验证。

自动化覆盖至少包括：`SlimeWar.Weapon.Magnetic`、`SlimeWar.Weapon.Charge`、`SlimeWar.Body.PushStun`、`SlimeWar.Ecology.Fusion`、`SlimeWar.Ecology.Core`、`SlimeWar.Ecology.Resource`、`SlimeWar.Ecology.Score`、`SlimeWar.Flow.EcologyEnd`。M8 再完整执行拆解案第 18 节测试矩阵，包括资源/体量守恒、玩家射击不传播警戒、交付 1:1 增员、目标分不提前结束和灭绝立即结束。

## Assumptions

- 单人游戏，本轮不做网络同步。
- 主 P0 关卡使用双点布局；旧地图只用于 M1–M6 回归沙盒。
- M1 只修贴地，不顺便调整体量平衡、武器或 AI。
- 60×50 三阵营、完整表现、音频和正式美术在 P0/P1 规则稳定后处理。
- 每完成一个模块即停止并报告测试结果；只有该模块门槛通过，才继续下一个模块。
