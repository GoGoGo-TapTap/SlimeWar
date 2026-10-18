# SlimeWar Godot 11 生态机制复刻计划

## Summary

- 基线为 `rebulid-program @ 557a2de`。M0 已完成，不再重复分支迁移、LFS 恢复和项目初始化。
- 以 `Docs/Godot11_UE复刻拆解案.md` 为准，P0 目标是 `ecology_double_point`：44×30 米、A/B 两阵营、4 个矿点、150 秒、200 分。60×50 三阵营版本改为后续扩展。
- 保留“主菜单 → 选落点 → 投放 → 局内 → 结算”的 UI 流程外壳，重写局内生态、敌人、武器、计分和 HUD 数据。
- 不再一次性删除旧代码。新生态路线先独立跑通，旧 Normal/Aggro/StateTree/波次系统在 M8 新路线验收后彻底退出运行时，避免再次出现大面积编译失败。
- 严格按模块推进。每个模块必须完成编译并进入真实 Editor/PIE 验证；验收通过后才进入下一模块，失败则只在本模块内修复。

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
| M1 | 史莱姆贴地、胶囊和视觉原点 | 体量 1/4/8 在 PIE 中贴地，误差 ≤2 cm |
| M2 | 连续移动、导航、排斥、聚群、避让玩家 | 10 只运行 60 秒，无抖动、穿墙、永久堵塞 |
| M3 | 身体碰撞、小体量推动、大体量阻挡、瞄准锁定、停稳眩晕 | PIE 可见推动、阻挡、滑行后眩晕 |
| M4 | 融合 v2：自主寻找、接触融合、位移撮合 | 自然形成体量 2；打断和玩家撮合正确 |
| M5 | 中性/敌对核心、跃击、吞噬、爆裂、弱点 | 核心状态、连锁和弱点一击通过 PIE |
| M6 | 磁性瞄准、蓄力射击、击退、撞墙分裂 | 锁定、遮挡、零伤害满蓄力、分裂通过 PIE |
| M7 | 双点 P0：地图、矿点、采集交付、150 秒/200 分 | 完整运行一局，资源和体量守恒 |
| M8 | 警戒、争夺、敌对核心、吞噬、手雷、粘液、完整计分 | 拆解案第 18 节场景全部通过 |
| M9 | 三阵营 60×50 扩展和完整 HUD/结算 | 三点地图完整回合通过 |
| M10 | 表现、100 只压测、Windows Development 打包 | 性能基线和打包包体通过 |

### M1：当前立即执行的贴地模块

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
