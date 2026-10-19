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
| M4 | 融合 v2：自主寻找、接触融合、位移撮合 | `Passed`；自动测试与用户手动 Checklist 均通过 |
| M5 | 中性/敌对核心、跃击、吞噬、爆裂、弱点 | `Passed`；自动测试与用户手动 Checklist 均通过 |
| M6 | 磁性瞄准、蓄力射击、击退、撞墙分裂 | `Passed`；自动测试与用户手动验收均通过 |
| M7 | 双点 P0：地图、矿点、采集交付、150 秒/200 分 | `Passed`；自动验收与用户手动 Checklist 均通过 |
| M8 | 警戒、争夺、敌对核心、吞噬、手雷、粘液、完整计分 | `Passed`；自动场景与用户手动 Checklist 均通过 |
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

### M4：融合 v2（Passed）

新增独立 `USlimeEcologyFusionComponent`，旧 StateTree 融合保持不动。

三条路径：

- 自主融合：出生或成功融合后等待 14–22 秒；400 cm 内寻找同阵营、双方体量和 ≤2 的最近个体；75 cm/s 走向中点；接触 2.4 秒后融合。
- 接触融合：两个合格个体物理接触时直接进入 2.4 秒计时，不要求准备时间，但仍受自动上限 2 限制。
- 玩家位移撮合：M3 的 3 秒撮合窗口内，个体上一位置到当前位置的扫掠距离 ≤双方半径和 +8 cm 时立即融合，可突破自动上限并达到总质量 8。

时序：

- 自主寻伴超时 10 秒。
- 成功冷却 1.8 秒。
- 打断禁用自主融合 5 秒；玩家位移撮合不受该禁用影响。
- 成功冷却期间拒绝所有新融合。
- 眩晕、待眩晕、死亡和分离取消普通配对；瞄准锁定禁止自主寻伴，但外力路径保持独立。

融合结果：

- 保留发起方 Actor/控制器，销毁另一方。
- 新体量为双方体量之和，最高 8。
- 生命按双方当前生命总和继承。
- `Cargo` 与 `BodyScoreCredit` 均为双方之和。
- 使用 `ApplyStatRow` 更新尺寸、生命和贴地。
- 调用现有 `OnEnemyFused` 统计入口，不产生击杀。

新增规则参数：

- `FusionAutoPrepMin = 14`
- `FusionAutoPrepMax = 22`
- `FusionAutoSeekRange = 400`
- `FusionAutoSeekSpeed = 75`
- `FusionContactTime = 2.4`
- `FusionContactTolerance = 18`
- `FusionForcedCaptureGap = 8`
- `FusionSuccessCooldown = 1.8`
- `FusionInterruptedBlock = 5`
- `FusionAutoMassCap = 2`
- `FusionTotalMassCap = 8`
- `FusionSeekTimeout = 10`

M4 调试命令：

- `SlimeEcologyFusionStatus`
- `SlimeEcologyFusionReady`
- `SlimeEcologyForceFuse`
- `SlimeEcologyInterruptFusion`
- `SlimeEcologyFusionTest`
- `SlimeEcologyForcedFusionTest`
- `SlimeEcologyInterruptTest`
- `SlimeEcologyPushPairSetup`

头顶调试显示：

- `Slime.Debug.DrawEcologyState` 默认开启。
- 每只生态史莱姆显示阵营、体量、生命、Cargo、Credit、融合状态/路径、接触进度、伙伴、准备/冷却/禁用时间、大脑行为、身体/眩晕状态、外力、撮合窗口、速度和位置。

M4 引擎门禁：

1. 编译 0 error。
2. 两只体量 1 在 2.4 秒接触后融合成体量 2。
3. 体量 2 + 体量 1 不会自主融合。
4. 体量 4 + 体量 4 经位移撮合立即成为体量 8。
5. 打断后双方 5 秒禁用；玩家撮合仍可用。
6. 成功冷却 1.8 秒内拒绝再次融合。
7. 融合后体量 2/4/8 保持贴地。
8. `Cargo` 和 `BodyScoreCredit` 融合前后守恒。
9. 完成 `Docs/EcologyRebuild/M4-Checklist.md` 后 M4 才标记 `Passed`。

#### M4 实现与自动测试记录

已实现：

- `USlimeEcologyFusionComponent`
- 自主寻伴、物理接触和玩家位移撮合三条路径
- 14–22 秒准备、2.4 秒接触、10 秒寻伴超时
- 自动融合上限 2、总质量上限 8
- 1.8 秒成功冷却、5 秒打断禁用
- 融合生命、Cargo、BodyScoreCredit 继承
- `FusionSeek` 大脑让权和配对期间移动碰撞忽略
- 融合后体量/尺寸/贴地更新

关键修正：

- 移动目标新增独立到达半径；融合寻伴使用 10 cm，避免探索的 70 cm 到达半径让双方提前停住。
- 最终路径航点使用目标自身到达半径，不再固定用 40 cm 航点半径。
- `Blocked` 状态现在允许玩家外力撮合，只禁止自主/普通接触融合。

自动测试结果：

| 场景 | 结果 |
|---|---|
| 接触融合 | 体量 1+1 → 体量 2；生命 40/40；Cargo 0；Credit 8 |
| 自主融合 | 双方进入 Seeking → Contacting → 体量 2 |
| 成功冷却 | 融合后日志剩余 1.63 s，按 1.8 s 计时 |
| 位移撮合 | 体量 4+4 → 体量 8；生命 160/160；Credit 40 |
| 打断 | 双方进入 Blocked，剩余禁用 4.59 s |
| 打断中玩家撮合 | Blocked 3.59 s 时仍可撮合为体量 2 |
| 贴地 | 体量 2 底误差 2.0 cm；体量 8 底误差 2.0 cm |

日志与截图：

- 接触融合：`Saved/Logs/M4-ContactFusion.log`
- 自主融合：`Saved/Logs/M4-AutoFusion-final.log`
- 位移撮合：`Saved/Logs/M4-ForcedFusion-final.log`
- 打断撮合：`Saved/Logs/M4-InterruptFusion-final.log`
- 截图：`HighresScreenshot00029.png`、`00036.png`、`00037.png`、`00040.png`

仍待手动验收：

- 玩家身体推动小史莱姆触发即时撮合。
- 大体量阻挡与真实接触手感。
- 观察 2.4 秒进度、1.8 秒冷却和 5 秒禁用。
- 确认无重叠、弹飞、穿墙和融合后悬空。

详细记录：`Docs/EcologyRebuild/TestLogs/M4-fusion.md`。

M4 已完成 `Docs/EcologyRebuild/M4-Checklist.md` 手动验收，标记为 `Passed`。

用户手动验收补充：

- 三只体量 1：只有一对互相持有 partner，第三只保持 Idle；融合后为一只体量 2 + 一只体量 1。
- 受伤撮合：一只 10/20 与一只 20/20 融合为体量 2，生命 30/40，Credit 8，Cargo 0。
- 融合冷却：融合后日志读取 0.28 秒，随后归零。
- 体量 2 胶囊底/网格底误差 2.0 cm。
- 解除锁定后恢复 Explore，速度 115 cm/s。
- 体量 4+4 → 体量 8，生命 160/160，Credit 40，贴地误差 2.0 cm。

### M5：核心、跃击、吞噬、爆裂与弱点（Passed）

M5 在 M4 融合结果之上增加统一核心状态和核心战斗。普通个体、中性核心、敌对核心继续共用 `ASlimeEcologyEnemy`。

新增：

- `ESlimeCoreState {None, Neutral, Hostile}`
- `USlimeCoreComponent`
- `USlimeWeakpointComponent`
- `USlimeHealthComponent::GetLastDamageCauser`
- `USlimeCombatSubsystem::ApplyDamageToComponent`

融合交接：

- 融合成功后存活方成为 `Neutral` 核心；任一父体已敌对则成为 `Hostile`。
- 普通个体保持 `CoreState=None`。

规则：

- 敌对核心苏醒 1.2 秒；感知 900 cm；战斗记忆 6 秒；追击速度 380 cm/s；牵引距离 1900 cm。
- 玩家距离 <240 cm 或被玩家攻击后 6 秒内可成为目标；否则优先敌对阵营个体。
- 生命 <30% 且看见威胁时撤退 2.5 秒。
- 0.65 秒前摇后跃击：水平 1050 cm/s、向上 300 cm/s、持续 0.4 秒、收势 0.4 秒、攻击间隔 1.6 秒。
- 单次跃击对同一目标最多命中一次；玩家伤害 20，敌对个体伤害 16。
- 敌对核心跃击击杀敌对个体时吞噬，体量上限 8，每吸收 1 体量恢复 5 生命。
- 玩家击杀核心时爆裂：半径 `300 + 25 × mass` cm，中心伤害 42、边缘 0.65、推力 `800 / mass^0.45`。
- 爆裂链每个核心只触发一次；墙体遮挡不伤害。
- 体量 7/8 弱点位于身体顶部，正伤害处决，0 伤害不处决。

M5 调试命令：

- `SlimeSpawnEcologyCore <Mass> <CoreState>`
- `SlimeEcologySetCoreState <CoreState>`
- `SlimeEcologyProvokePlayer <Seconds>`
- `SlimeEcologyCoreStatus`
- `SlimeEcologyPounceTest`
- `SlimeEcologyDevourTest`
- `SlimeEcologyDetonationTest`
- `SlimeEcologyWeakpointTest`

M5 门禁：编译 0 error；中性核心不追玩家；敌对核心苏醒/追击/撤退；跃击时序和单目标单次命中；吞噬体量/生命/Credit；爆裂链只结算一次；弱点正伤害处决且 0 伤害不处决。自动记录写入 `TestLogs/M5-core.md`，真实观感写入 `M5-Checklist.md`。

#### M5 实现与自动测试记录

已实现：

- `ESlimeCoreState`
- `USlimeCoreComponent`
- `USlimeWeakpointComponent`
- `USlimeHealthComponent::GetLastDamageCauser`
- `USlimeCombatSubsystem::ApplyDamageToComponent`
- 融合后自动成为 Neutral 核心；敌对父体继承 Hostile
- 苏醒、战斗记忆、牵引、低血撤退
- 0.65 秒前摇、0.4 秒跃击、收势和攻击冷却
- 敌对核心吞噬
- 玩家击杀核心爆裂和核心连锁
- 体量 7/8 弱点与正伤害处决

编译：0 error。

自动结果：

- 融合体量 1+1 后日志出现 `Core: ... state -> Neutral`。
- 跃击阶段按 `Chasing → Windup → Pouncing → Recovering` 运行，每次命中玩家 20 伤害。
- 吞噬体量 2+2：体量 4，生命 50/80，Credit 16，攻击者 Cargo 保留。
- 两个中性核心相邻时，玩家击杀第一个后第二个也触发爆裂；日志中两个核心各出现一次 `CoreExplosion`。
- 体量 6 弱点禁用；体量 7 弱点启用；0 伤害后生命 140/140 不变，1 伤害立即处决。
- 用户反馈修正：移动目标只在 25 cm 重建阈值到达时同时更新缓存 Goal 与路径，解决沿玩家历史路径绕圈。
- 用户反馈修正：爆裂增加 0.8 秒橙色范围球、黄/红连线和逐目标伤害/推力日志；普通旁观个体验证为生命 20→0、推力 800 cm/s。

日志与截图：

- 跃击：`Saved/Logs/M5-Pounce-final.log`，`HighresScreenshot00055.png`
- 吞噬：`Saved/Logs/M5-Devour.log`
- 爆裂链：`Saved/Logs/M5-Detonation-Visible.log`
- 弱点：`Saved/Logs/M5-Weakpoint.log`
- 融合核心：`Saved/Logs/M5-FusionCore.log`，`HighresScreenshot00054.png`

详细记录：`Docs/EcologyRebuild/TestLogs/M5-core.md`。

用户已于 2026-10-19 完成 `Docs/EcologyRebuild/M5-Checklist.md` 全部手动验收。M5 标记为 `Passed`。

后续 M7 实机反馈修正：中性核心爆炸不再递归引爆其他核心，只对范围内其他核心造成普通爆裂伤害；只有敌对核心保留递归连锁。这样避免一次击杀把聚集在同一点位的全部中性核心清空。M5 的“两核心连锁”场景相应更新为“敌对核心连锁、中性核心不连锁”。

### M6：磁性瞄准、蓄力射击、击退与撞击分裂（Passed）

M6 复用现有生态个体、身体外力/眩晕、融合、核心和弱点系统。

武器数据：

- 新增 `USlimeWeaponRules` 和 `/Game/_SlimeWar/Weapon/DA_Weapon_Rifle`。
- 伤害 8、射击间隔 0.22 秒、弹匣 3、换弹 1 秒、射程 1800 cm。
- 起蓄 0.18 秒、满蓄 1.2 秒、满蓄半径 400 cm、满蓄击退速度 1400 cm/s。
- 普通直接推力 400 cm/s，质量指数 0.65，范围边缘衰减 0.4。
- 捕获半径 10% 屏高、保留半径 1.35 倍、换锁阈值 55%、扫描间隔 0.05 秒。

磁性瞄准：

- `USlimeAimAssistComponent` 始终在玩家可控时工作；右键瞄准不再作为吸附开关。
- 通过生态空间哈希查询射程内目标，身体中心与有效弱点都是候选点。
- 当前目标在保留半径、射程、相机与枪口无遮挡时保持锁定。
- 新目标屏幕距离必须小于当前目标 55% 才换锁。
- 锁定/解锁直接调用 `USlimeBodyInteractionComponent::SetAimLocked`。
- 射击解算输出 `FSlimeShotSolution`，包含瞄准点、枪口终点、命中结果、目标、命中组件和弱点标记。

蓄力射击：

- 保留旧 `UGA_Fire` 兼容，但玩家改为授予 `UGA_ChargeFire`。
- 左键按下开始蓄力，松开才发射；按住不自动连射。
- `s = clamp((hold - 0.18) / (1.20 - 0.18), 0, 1)`。
- 伤害 `8 × (1 - s)`，满蓄力伤害 0，不触发弱点处决。
- 范围击退半径 `400 × s`，速度 `1400 × s × (1 - 0.4 × distance / radius)`。
- 直接推力与范围推力叠加；受影响个体进入 1.5 秒待眩晕、打断融合并禁用 5 秒。

撞击分裂：

- 范围击退速度 ≥600 cm/s 时打开 0.9 秒撞击分裂窗口。
- 只有体量 ≥2 且撞到 `ECC_WorldStatic` 才尝试分裂，撞玩家、史莱姆和地面不触发。
- 碎片数等于父体量，全部为体量 1。
- 先生成并验证全部位置，任一失败则销毁已生成碎片并保持父体不变。
- 生命、货物和身体分预算守恒分配；碎片清空核心/战斗/警戒，获得 2 秒眩晕和 5 秒融合禁用。
- 父体直接销毁，不走击杀入口，不加玩家击杀数或玩家分数。

M6 调试命令：

- `SlimeWeaponStatus`
- `SlimeAimAssistStatus`
- `SlimeChargeTest <Alpha>`
- `SlimeKnockbackTest`
- `SlimeImpactSplitTest <Mass>`

M6 引擎门禁：

1. 编译 0 error。
2. 屏幕捕获、保留、55% 换锁、相机/枪口遮挡。
3. 体量 7/8 身体/弱点候选正确；零伤害不处决弱点。
4. 短按 8 伤害、半蓄力 4 伤害、满蓄力 0 伤害且强击退。
5. 直接推力与范围推力叠加，1.5 秒待眩晕和 5 秒融合禁用。
6. 体量 2→2 碎片、体量 8→8 碎片，生命/货物/身体分守恒。
7. 位置不足时不分裂、不丢体量、不增加击杀或分数。
8. 完成 `Docs/EcologyRebuild/M6-Checklist.md` 后 M6 才标记 `Passed`。

#### M6 实现与自动测试记录

已实现：

- `USlimeWeaponRules` / `DA_Weapon_Rifle`
- `USlimeAimAssistComponent`
- `UGA_ChargeFire`
- 磁性瞄准、保留、换锁、身体/弱点候选、相机/枪口遮挡
- 蓄力伤害、直接推力、范围击退、待眩晕和融合打断
- `FSlimeShotSolution`
- `USlimeBodyInteractionComponent::OpenImpactSplitWindow`
- `ASlimeEcologyEnemy::TryImpactSplit`
- C++ 调试 HUD：捕获圈、锁定点、弱点颜色、蓄力条

编译：`SlimeWarEditor Win64 Development` 0 error / 0 warning。

自动武器结果：

```text
地图：L_Whitebox_CityPlaza
命令：SlimeRunTimeScale 0,SlimeWeaponStatus,SlimeChargeTest 0,
      SlimeChargeTest 0.5,SlimeChargeTest 1,SlimeKnockbackTest,
      SlimeImpactSplitTest 2,SlimeImpactSplitTest 8
日志：Saved/Logs/M6-Weapon.log
```

| 蓄力 | 伤害 | 外部速度 | 待眩晕 |
|---:|---:|---:|---:|
| 0 | 8（40→32） | 254.9 cm/s | 1.50 s |
| 0.5 | 4（40→36） | 919.2 cm/s | 1.50 s |
| 1 | 0（40→40） | 1619.2 cm/s | 1.50 s |

分裂结果：

- 体量 2 → 2 碎片；总生命 20→20，Cargo 3→3，Credit 20→20。
- 体量 8 → 8 碎片；总生命 80→80，Cargo 3→3，Credit 20→20。

磁性瞄准结果：

```text
命令：SlimeRunTimeScale 0,SlimeRunStart,SlimeAimAssistScreenTest
日志：Saved/Logs/M6-Aim.log
截图：HighresScreenshot00062.png
```

- 目标进入屏幕捕获后被锁定，命中组件为身体胶囊。
- 目标状态变为 `AimLocked`，屏幕距离 88.2 px，进入保留区。
- `StatTableProvider` 日志确认加载 `DA_Weapon_Rifle`。

详细记录：`Docs/EcologyRebuild/TestLogs/M6-weapon.md`。

手动验收项：真实准星换锁、墙体/枪口遮挡、体量 7 弱点、按住蓄力/松开射击、0.22 秒射击间隔、撞墙触发分裂和位置不足回滚。用户已于 2026-10-19 全部确认通过。

手动反馈修正：

- 枪口遮挡改为从角色身上的武器位置发射遮挡/命中射线，而不是相机附近假枪口。相机越墙可见但角色本体在墙后时不能再锁定或命中。
- 空弹匣不再进入蓄力；打出最后一发后自动换弹，空弹时按左键也会补触发换弹。换弹期间禁止蓄力、射击和射击表现。
- 换弹进度由 `UGA_Reload` / Character 的换弹视觉状态驱动，HUD 显示独立橙色 `RELOADING` 进度条；蓄力条在此期间隐藏。
- 手动按 R 换弹会取消当前蓄力。
- 静态立方分裂不再只检查 `ECC_WorldStatic` 对象类型，也接受 StaticMeshActor / Static Mobility 静态网格，并排除 Pawn。
- 准星正对目标导致范围推力方向为零时，回退为玩家指向目标，确保满蓄力真正把目标推向墙体。
- 碎片候选改到墙体法线的安全半平面，并使用多圈候选筛位；位置不足仍整次回滚。

静态立方体自动测试：

```text
命令：SlimeRunTimeScale 0,SlimeRunStart,SlimeImpactSplitCollisionTest 2
日志：Saved/Logs/M6-SplitCollision.log
截图：HighresScreenshot00065.png
```

结果：静态立方体命中、分裂窗口触发、体量 2 → 2 碎片、父体销毁、无残留父体。

#### 新场景墙体设置规范（撞击分裂）

后续导入新场景时，默认使用 **StaticMeshActor + Static Mobility + BlockAll**，这是当前代码明确保证可触发撞击分裂的配置。

墙体必须满足：

| 设置 | 推荐值 |
|---|---|
| Actor 类型 | `StaticMeshActor`；Blueprint 墙体也可，但碰撞组件必须满足下方条件 |
| Mobility | `Static` |
| Collision Presets | `BlockAll` |
| Collision Enabled | `Query and Physics` |
| Object Type | `World Static` |
| Response To Pawn | `Block` |
| Response To Visibility | `Block` |
| Can Ever Affect Navigation | 需要寻路时开启 |
| 碰撞几何 | 必须有 Simple Collision，不能只有 Complex Collision |

当前分裂代码接受以下任一静态判定：

- 碰撞组件 Object Type 为 `WorldStatic`；
- 碰撞组件 Mobility 为 `Static`；
- 命中 Actor 为 `StaticMeshActor`；
- 命中对象不能是 Pawn。

不推荐：

- Collision Enabled 为 `No Collision`；
- 只有 Complex Collision，没有 Simple Collision；
- Blueprint 墙体使用 `WorldDynamic + Movable` 碰撞组件；
- 用纯视觉网格代替碰撞；
- 碰撞组件不阻挡 Pawn。

分裂位置额外要求：

- 碎片在墙体法线的安全半平面生成，也就是墙的来向一侧。
- 体量 2 建议墙前至少留约 1.5 m 空地。
- 体量 8 建议墙前至少留约 2.5–3 m 空地。
- 墙前有其他道具或其他史莱姆时，位置预检可能失败并按设计整次回滚。

新场景快速检查：

1. 执行 `show collision`，确认墙体有 Simple Collision。
2. 生成体量 2：`SlimeSpawnEcology 1 0 2`。
3. 在距墙约 2–3 m 处满蓄力射击，将史莱姆推向墙体。
4. 日志应出现：

```text
ImpactSplitWindow: ... objectType=0 mobility=0
ImpactSplit: ... mass 2 -> 2 fragments
```

其中 `objectType=0` 表示 `WorldStatic`，`mobility=0` 表示 `Static`。

也可先执行构建自检：

```text
SlimeRunStart
SlimeImpactSplitCollisionTest 2
```

预期：

```text
ImpactSplitCollisionTest: parentMass=2 fragments=2 remainingParents=0
```

M6 手动验收：用户已于 2026-10-19 完成 `Docs/EcologyRebuild/M6-Checklist.md`，包含墙后锁定/命中、空弹与换弹进度、静态立方体分裂和其余手感检查。M6 结论：`Passed`。

### M7：双点生态关卡与资源闭环（Passed）

M7 新建 Godot 11 `ecology_double_point` 对应的 UE 双点白盒，并实现从资源到交付增员的完整闭环。

新增运行时类型：

- `FSlimeSiteStatus`：站点、阵营、存活数、存活体量、交付数、核心数。
- `FSlimeMineralStatus`：名称、库存、初始库存、是否再生、再生倒计时。
- `FSlimeEcologyLedger`：资源与体量守恒账本。
- `ASlimeFactionSite`
- `ASlimeMineralPatch`
- `ASlimeCargoPickup`

`ASlimeEcologyEnemy` 增加：

- `SetCargo`、`AddCargo`、`ConsumeCargo`
- 所属站点弱引用
- 死亡时掉落携货；融合和撞击分裂不额外掉落

大脑行为新增 `RecoverCargo`、`ReturnCargo`、`Gather`。资源链路优先级：

1. 既有暂停、核心战斗、融合、瞄准锁定。
2. 玩家避让。
3. 携货返程与交付。
4. 回收附近地面货物。
5. 前往有库存矿点并采集。
6. 聚群/探索。

资源参数：

- 感知 800 cm、记忆 28 秒。
- 采集距离 120 cm、采集 2 秒。
- 交付距离 130 cm、拾取距离 110 cm。
- 携货与返程速度 115 cm/s。
- 交付后休息 2–4 秒。
- 携货个体和地面货物不参与普通自主融合。

生态子系统扩展：

- 注册站点、矿点、货物和个体。
- `TryGather` 原子扣库存并增加 1 携货。
- `TryDeliver` 扣 1 携货、增加交付数并排队生成 1 只体量 1。
- 每帧最多处理 8 个生成请求，检查 NavMesh、玩家 270 cm 安全距离和个体重叠。
- 维护资源账本：
  `矿点库存 + 携货 + 地面货物 + 已交付 = 初始库存 + 再生库存`
- 维护体量账本：
  `初始体量 20 + 交付体量 = 存活体量 + 已移除体量`

关卡：

- 地图 `/Game/_SlimeWar/Maps/L_Ecology_DoublePoint.umap`。
- UE 范围 X `[-1500,1500]`、Y `[-2200,2200]`。
- A 站点 `(-100,-1400)`，B 站点 `(-100,1400)`，各开局 10 只体量 1。
- 落点 D1 `(-1100,-1800)`、D2 `(1100,1800)`。
- 矿点：WestNear `(500,-700)` 库存 4 不再生；EastNear `(-500,700)` 库存 4 不再生；SouthShared `(600,0)`、NorthShared `(-600,0)` 初始 5，每 5 秒恢复到最多 6。
- 新资产：`DA_RunConfig_DoublePoint`、`DA_SpawnLayout_DoublePoint`、`DA_Level_DoublePoint`。
- `DA_Level_DoublePoint` 注册到 `USlimeGameSettings::Levels`。

运行流程：

- `USlimeRunConfig` 新增 `bEcologyRun`。
- 生态关卡跳过旧 SpawnPoint/批次校验，调用 `EcologySubsystem::BeginRun()`。
- `EndRun` 调用 `EcologySubsystem::EndRun()`。
- 新增 `ERunEndReason::Extinction`：无存活个体、无待生成请求、无地面货物时结束。

调试命令：

- `SlimeEcologyResourceStatus`
- `SlimeEcologyResourceSoakTest <Seconds>`
- `SlimeEcologyDeliveryTest`
- `SlimeEcologyResourceLedgerCheck`

引擎门禁：

1. 编译 0 error。
2. 地图尺寸、站点、矿点、落点和 NavMesh 完整。
3. A/B 各 10 只体量 1，4 矿点总库存 18。
4. 完整观察采集、返程、交付和 1:1 出生。
5. 资源与体量账本锁定后仍守恒。
6. 共享矿点再生到 6，近源矿点不再生。
7. 携货个体死亡生成地面货物，每次拾取只转移 1 份。
8. 交付出生避开玩家、墙体和已有个体。
9. 灭绝立即结束。
10. 主菜单、D1/D2、150 秒倒计时和结算可走通。
11. 自动结果写 `TestLogs/M7-double-point.md`，手动验证写 `M7-Checklist.md`。

#### M7 实现与自动测试记录

已实现：

- `ASlimeFactionSite`
- `ASlimeMineralPatch`
- `ASlimeCargoPickup`
- `FSlimeSiteStatus`、`FSlimeMineralStatus`、`FSlimeEcologyLedger`
- 生态子系统资源注册、采集、交付、掉落、拾取、生成队列和守恒账本
- 大脑资源行为 `RecoverCargo`、`ReturnCargo`、`Gather`
- `bEcologyRun` 回合分支和 `Extinction` 终局
- C++ 调试 HUD 的站点、矿点和账本显示
- 双点白盒、LevelDefinition、RunConfig 和 SpawnLayout 资产

内容资产：

- `/Game/_SlimeWar/Maps/L_Ecology_DoublePoint`
- `/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint`
- `/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint`
- `/Game/_SlimeWar/Core/Data/Levels/DA_Level_DoublePoint`

地图与数据已通过 Python 工具创建；`DA_Level_DoublePoint` 已注册到关卡选择列表。新地图直接打开时，`URunSubsystem` 会按地图名反查 LevelDefinition，因此开发者 PIE 也会加载 150 秒生态配置。

实现偏差：

- 新建关卡的 RecastNavMesh 由 Python 放置后默认没有运行时导航数据；通过编辑器 `RebuildNavigation` / `BuildPaths` 重建并设置 `RuntimeGeneration=Dynamic` 后验证通过。
- 站点出生保留 NavMesh 失败时的地面/静态碰撞回退，避免导航数据尚未构建时永久阻塞初始人口。

自动结果：

```text
资源观察日志：Saved/Logs/M7-Resources2.log
完整回合日志：Saved/Logs/M7-FullRun-final.log
灭绝日志：Saved/Logs/M7-Extinction-final.log
货物流失日志：Saved/Logs/M7-CargoDrop.log
最终截图：HighresScreenshot00070.png
```

- 初始：A/B 各 10 只，4 矿点库存合计 18。
- 30 秒资源观察：完成 5 次采集交付和 5 只 1:1 增员；资源与体量账本持续平衡。
- 最终 150 秒回合同样完成交付增员，`TimeUp` 在 150.0 秒正常结束。
- 共享矿点正常再生；近源矿点不恢复。
- 携货个体死亡日志出现 `EcologyCargo: dropped 1`。
- 清空生态后 0.5 秒触发 `Extinction`，资源与体量账本平衡。

详细记录：`Docs/EcologyRebuild/TestLogs/M7-double-point.md`。

手动验收：`Docs/EcologyRebuild/M7-Checklist.md`。

#### M7 实机反馈修复

1. 采集停摆

根因：站点到最近矿点超过 800 cm，大脑只按 `ResourceSenseRange` 找矿；携货送回站点后，附近没有矿点，于是永久失去资源目标。

修复：新增 `ResourceExplorationRange = 2000 cm`。先用 800 cm 感知范围，失败后再用远距离探索范围选择矿点。

验证：`Saved/Logs/M7-ResourceContinue.log` 中 60 秒持续采集，交付从 0 增长到 34，当前仍有 5 只携货，资源与体量账本全程平衡。

2. 货物不可视

新增 `ASlimeCargoVisual`，使用可替换的 `CargoMeshAsset`、`UStaticMeshComponent` 和数量缩放：

- 地面货物由 `ASlimeCargoPickup` 继承该可视化 Actor。
- 携货个体在头顶/身体上方挂同一个 `ASlimeCargoVisual`。
- 货物数量和模型可通过 Blueprint/DataAsset 替换，不写死为单一立方体逻辑。

3. 调试命令破坏账本

`SlimeEcologyDeliveryTest` 和 `SlimeEcologyCargoDropTest` 不再凭空增加货物，而是调用 `DebugGrantCargoFromStock` 从矿点库存扣 1 份。

验证：`Saved/Logs/M7-DeliveryFix.log` 中调试交付后库存 17、交付 1，资源与体量账本均平衡。

4. 中性核心清空站点

根因：多个 mass 2 中性核心聚集在站点，击杀一个后递归引爆所有相邻中性核心。

修复：新增 `HostileCoresChainOnly = true`。中性核心爆炸仍造成普通范围伤害和击退，但伤害来源改为核心自身，不再触发被击核心的玩家击杀爆裂；敌对核心保留连锁。

验证：`Saved/Logs/M7-ChainFix.log` 中第二个 mass 2 中性核心从 40 血降到 3 血但没有死亡或递归爆炸。

用户已于 2026-10-19 完成 `Docs/EcologyRebuild/M7-Checklist.md` 手动验收，包括持续采集、货物可视化、携货掉落、调试交付账本和中性核心爆炸修正。M7 标记为 `Passed`。

### M8：警戒、争夺、手雷、粘液与完整计分（Passed）

M8 沿用 M3–M7 的生态个体、融合、核心和资源闭环，补齐以下运行时规则。旧 StateTree/Normal/Aggro 清退推迟到 M9 前，不在 M8 内执行。

#### 警戒与争夺

新增 `USlimeAlertComponent`：

- 普通警戒持续 10 秒，首次触发先等待 1 秒预警。
- 同阵营传播半径 500 cm，只通知可见同族，不接力传播。
- 只有 NPC 实际伤害或资源争夺会触发普通警戒。
- 玩家枪击不创建、不刷新、不传播普通警戒；击中已有敌对核心时只刷新 6 秒 `PlayerProvoked`。
- 单只普通个体不会直接变成攻击性核心；警戒期间完成融合才生成敌对核心。
- 已有敌对核心、中性核心、融合/分裂保护中的个体不接收普通警戒。
- 资源争夺距离 340 cm，条件累计 1.5 秒触发，条件消失时累计值下降，触发后冷却 5 秒。

大脑新增 `AlertWait`、`AvoidHostileCore`、`AlertAvoidEnemy`、`AlertRegroup`。警戒退避速度 154 cm/s；携货/空载个体分别在 220/180 cm 内避开敌对核心。

#### 手雷

新增 `ASlimeEcologyGrenade`：

- `1/2` 选择同化/旋风，`Z/X` 选择 A/B，`G` 投掷；C 阵营留到 M9。
- M8 使用 C++ 直接键位绑定，不新增 Enhanced Input 资产。
- 两种手雷各 3 枚，投掷间隔 0.7 秒。
- 初速度为准星方向 1300 cm/s + 向上 400 cm/s，重力 2000 cm/s²，最长飞行 2 秒。
- 同化雷半径 450 cm，只检查距离；改变阵营并清理融合、核心战斗、警戒、资源目标与记忆，不改变体量/生命/核心类型/货物/身体分。
- 旋风雷半径 600 cm、持续 4.5 秒、吸引加速度 1200 cm/s²、速度上限 450 cm/s、中心 20 cm 停止吸引；只打开短暂接触融合资格，不自动添加玩家撮合。

#### 击杀上下文与计分

新增 `FSlimeKillContext` 和 `FSlimeFusionEvent`；`IBattleDirector` 改为：

- `OnEnemyKilled(const FSlimeKillContext&)`
- `OnEnemyFused(const FSlimeFusionEvent&)`

计分：

```text
Body = max(mass × 4 + max(mass - 2, 0)², BodyScoreCredit)
Full = Body + Cargo × 6
Award = HostileCore ? floor(Full × 0.5) : Full
```

- 只有玩家造成的击杀给玩家分数；NPC 互杀、吞噬和 NPC 爆炸为 0 玩家分。
- 玩家击杀携货个体直接结算货物分，不落地；NPC 击杀仍掉货。
- 所有死亡只结算一次；玩家来源的敌对核心连锁每只只结算一次。
- 结算新增玩家击杀、NPC 击杀、交付、截获货物、融合、引爆、连锁、受伤和用时。

#### 粘液 RenderTarget

采用 RenderTarget/RVT 路线：

- `USlimePaintSubsystem` 创建 1024×700 `UTextureRenderTarget2D`，覆盖双点地图 44×30 m。
- 记录死亡和核心爆炸 splat，最多保留 2048 次写入；只在新一局开始重置。
- 提供 `GetPaintRenderTarget`、`RegisterSplat`、`GetSplatCount`、`ResetPaint`。
- `ASlimePaintField` 使用单块贴地平面和可替换 `PaintSurfaceMaterial`，材质参数默认 `SlimePaint`。
- `PaintSplatMaterial` / `PaintSurfaceMaterial` 由后续美术提供；M8 用 `Slime.Debug.Paint 1` 绘制临时地面 splat，保证逻辑和位置可验收。

#### M8 调试与门禁

新增命令：

- `SlimeEcologyAlertTest`
- `SlimeEcologyDisputeTest`
- `SlimeEcologyHostileFusionTest`
- `SlimeWeaponDoesNotAlertTest`
- `SlimeGrenadeStatus`
- `SlimeThrowGrenade <Type>`
- `SlimeGrenadeAssimilationTest`
- `SlimeGrenadeTornadoTest`
- `SlimeEcologyScoreTest`
- `SlimePaintStatus`
- `SlimePaintTest`

自动测试写入 `Docs/EcologyRebuild/TestLogs/M8-ecology-rules.md`；手动验证写入 `Docs/EcologyRebuild/M8-Checklist.md`。M8 Checklist 未完成前 M8 保持 `Active`。

#### M8 实现与自动测试记录

已实现：

- `USlimeAlertComponent`、NPC 伤害传播、玩家枪击无警戒、资源争夺、警戒融合敌对核心。
- 玩家手雷输入与库存：`1/2` 选择，`Z/X` 选择 A/B，`G` 投掷。
- `ASlimeEcologyGrenade` 同化与旋风效果。
- `FSlimeKillContext` / `FSlimeFusionEvent` 及新的 BattleDirector 事件契约。
- 玩家/NPC 击杀区分、敌对核心半分、携货分、连锁和结算统计。
- `USlimePaintSubsystem`、运行时 `ASlimePaintField`、700×1024 RenderTarget 与调试 splat。

关键自动结果：

```text
NPC damage alert: victim warning=1.00 alert=10.00 alliesAlerted=2
Dispute: alert=yes cooldown=5.00
Hostile fusion: mass=2 core=Hostile
Score: normal=68 cargo2=80 hostile=40 npc=0
Assimilation: 3/3 affected, faction B
Tornado: maxExternalSpeed=450 cm/s
Paint: 6 records, RT 700x1024, field=1
Ledger: resource OK, mass OK
```

实现偏差：

- 双点地图实际轴为 X=3000、Y=4400，粘液纹理改为 700×1024。
- 地图 Python 编辑触发编辑器崩溃，因此 PaintField 改为运行时自动生成。
- 正式粘液材质/RVT 参数仍等待美术资源；M8 使用 `Slime.Debug.Paint 1` 调试验收。

手动反馈修复：

- 同化雷现在同时更新 `Faction`、`AssignedSite`、`HomeLocation`、活动中心和资源记忆；HUD 站点统计改为按当前阵营计算。
- 自动验证：3 只 A 全部变 B 且 `assignedB=3`；A live 0、B live 3；携货个体状态为 `ReturnCargo`，目标 B 点 `(-100,1400)`。
- 旋风每帧从旋风中心到目标做 `WorldStatic` 视线检查，墙体后目标不再受吸引。
- 自动验证：墙前个体外部速度 110.5 cm/s，墙后个体 0。
- 瞄准锁定/眩晕改为 `SuspendVoluntaryMovement`：只清自主目标和水平速度，保留竖直速度与重力，解决跃击中被锁定悬空缓降。
- 自动验证：跃击 `Pouncing` 阶段锁定后 0.8 秒 Z 下降 42.9 cm，正常进入 Recovering。

详细记录：`Docs/EcologyRebuild/TestLogs/M8-ecology-rules.md`。手动验收：`Docs/EcologyRebuild/M8-Checklist.md`。

#### M8 手动验收与结论

用户已于 2026-10-19 完成 `Docs/EcologyRebuild/M8-Checklist.md` 全部手动验收，覆盖：

- `1/2`、`Z/X`、`G` 真实键鼠输入、两种手雷库存与 0.7 秒投掷间隔。
- 同化雷的阵营/颜色/HUD 站点同步，以及携货个体改返新阵营站点。
- 旋风雷的 6 米拉扯、4.5 秒持续与静态墙体遮挡。
- 玩家枪击不传播警戒、NPC 伤害警戒、争夺与警戒融合敌对核心。
- 玩家/NPC 击杀计分、携货分、敌对核心半分与连锁只结算一次。
- 粘液调试 splat 的持久显示与结算保留。
- 跃击中被瞄准锁定后继续受重力自然落地。
- 完整回合内资源与体量账本平衡，结算统计字段齐全。

M8 结论：`Passed`。旧 StateTree/Normal/Aggro 归档清退作为 M9 前置步骤单独执行，不在 M8 内完成。

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
