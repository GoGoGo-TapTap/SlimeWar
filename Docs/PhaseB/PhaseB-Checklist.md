# Phase B · 编辑器步骤与 CP-2 验收

> 代码侧已经完成并**编译通过**（`SlimeWarEditor Win64 Development`，0 error / 0 warning）。
> 下面第 2~4 步只能在你自己的机器上做（`.uasset` 无法在编辑器外生成）。
> 顺序有依赖：**先第 2 步（填 DA），否则融合时间是 0，会直接取消**。

---

## 1. 打开工程

1. 关掉正在运行的编辑器。
2. 双击 `SlimeWar.uproject`（若提示模块需要重新编译，选 Yes；本仓库代码已编译过）。
3. 打开时留意 Output Log，确认没有 `StatTableProvider: RunConfig is not set` 之类的报错。

> ⚠️ 改过 `Config/DefaultEngine.ini`（新增碰撞通道）。若编辑器提示"DefaultEngine.ini 已被外部修改"，
> 选 **保留磁盘上的版本 / Reload**，不要用编辑器里缓存的旧版本覆盖。

---

## 2. 给 `DA_RunConfig` 填 Phase B 新字段

打开 `/Game/_SlimeWar/Core/Data/DA_RunConfig`，展开 **AI | Fusion**：

| 字段 | 值 | 说明 |
|---|---|---|
| Fusion Contact Time | `0.4` | 策划案 4.4：持续接触 0.4 秒 |
| Fusion Contact Tolerance | `10` | 接触容差（cm），只吸收帧步长 |
| Fusion Post Fusion Delay | `1.0` | 融合后 1 秒才能再融合 |
| Fusion Retry Delay | `1.0` | 取消/靠近失败后 1 秒再找对象 |
| Fusion Max Participants | `2` | 多方融合预留；Phase B 固定 2 |

> **填 0 会怎样**：`FusionContactTime = 0` 时组件会打警告并取消融合；两个 Delay 填 0 会变成"立即重试"。
> 原有的 `AI | Normal` 字段（Spawn Wait 2 / Activity Radius 600 / Wander 300 / Pause 0.5~1.5 /
> Approach Timeout 2 / Mass Cap 8）不用动。

---

## 3. `ST_SlimeNormal`：只加 2 条转换（本 Phase 唯一必须改的资产）

打开 `/Game/_SlimeWar/Enemy/ST_SlimeNormal`。

### 3.1 为什么要改

被动方（被别人选中的那只）在**游走中**就接受了配对请求，但它的 StateTree 还在 `Wander` / `Pause` 状态里，
不会自己走进 `Hold`。所以给这两个状态各加一条"已经配对就切到 Hold"的转换。

### 3.2 操作

对 **`Wander`** 和 **`Pause`** 两个状态，各加一条转换：

| 项目 | 值 |
|---|---|
| 触发器（Transition 图标） | **Condition**（= OnTick \| OnEvent） |
| 条件 | `Slime: Has Fusion Target` |
| 条件的 `Invert` | **不勾选**（要的是"有融合目标时切过去"） |
| 目标状态 | `Hold` |

> ⚠️ **顺序很重要**：新转换必须排在该状态的**第一条**（在 `On State Completed → Pause` / `→ SelectTarget` 之前）。
> StateTree 在同一状态内**按编辑器里从上到下的顺序**判定，第一条通过的执行；排后面会被"On State Completed"抢先。

> ⚠️ **不要**给 `Hold` 加 `On State Completed` 转换。`Slime: Hold Position` 任务永远返回 `Running`，
> 靠它自己的 Condition（`Slime: Has Fusion Target` + 勾 Invert）离开。加了完成转换反而永远不会触发。

### 3.3 自检（"新代码有没有生效"）

**不需要**去核对 `Hold` 状态里的任何参数：`Slime: Hold Position` 的实例数据在本 Phase 没有变
（融合逻辑全在 C++ 里，节点在编辑器里长得和 Phase A 完全一样）。判断新代码是否生效，看这三条：

- PIE 控制台能识别 `Slime.Debug.DrawFusion 1`、`SlimeSpawnNormalAtMass`、`SlimeForceFuse`
  （控制台有自动补全；识别不到 = 编辑器还在用旧 DLL，关掉编辑器重新编译再打开）；
- Project Settings → Engine → Collision 里能看到 `SlimeFusion` 对象通道；
- 第 3.2 步的条件下拉里能选到 `Slime: Has Fusion Target`（Phase A 就有，本 Phase 只改了它的判定语义）。

> **StateTree 编辑器的操作口径**：任务不是树视图上的独立节点，而是**状态内部的 Tasks 条目**。
> 选中左侧 `Hold` 状态 → 右侧 Details → **Tasks** 分类 → 展开 `Slime: Hold Position` 那一行左侧的箭头，
> 才能看到节点类型和它的参数（本项目里是空的，数值全部来自 DA）。本 Phase 这个状态不需要动。

---

## 4. 确认碰撞通道（文本文件，已随代码改好）

`Config/DefaultEngine.ini` 末尾已有：

```ini
[/Script/Engine.CollisionProfile]
+DefaultChannelResponses=(Channel=ECC_GameTraceChannel1,DefaultResponse=ECR_Block,bTraceType=False,bStaticObject=False,Name="SlimeFusion")
```

自检：**Project Settings → Engine → Collision**，Custom Object Channels 列表里应出现 `SlimeFusion`。
`SlimeNormal` 蓝图/实例的 Capsule 组件 Object Type 应为 `SlimeFusion`。

> 这条通道的作用：普通史莱姆之间**默认互相阻挡**，只在配对期间用 `IgnoreActorWhenMoving` 放开这一对；
> 与世界、玩家、追兵、武器射线（`ECC_Visibility`）仍然阻挡。

---

## 5. CP-2 验收

在 `L_Sandbox_OnePoint` 里 PIE，控制台先开：

```text
Slime.Debug.CombatLog 1
Slime.Debug.DrawFusion 1
```

> **要等一会儿是正常的**：普通史莱姆生成后有 `Spawn Wait Time`（2s）→ 游走（≤3m）→ 停 0.5~1.5s
> 才会去选融合对象，所以生成后大约 **2.5~5 秒**才开始出现 `fusion Approaching` 连线。
> 想立刻看接触逻辑，用 `SlimeForceFuse`（直接配对，跳过选择与游走）。

| # | 场景 | 怎么造 | 期望 |
|---|---|---|---|
| 1 | 两只正常融合 | `SlimeSpawnNormalAtMass 1 2`（等 2~5s）| 头顶 `fusion Approaching → Contacting`，接触进度到 100% 后**变成一只体量 2、40 血**；日志只有 `OnEnemyFused`，**没有** `OnEnemyKilled` |
| 2 | 有伤融合 | `SlimeSpawnNormalAtMass 1 2` → 等接触前进度约一半时 `SlimeDamageNearestEnemy 10` | 融合体为 `mass 2`，血量 ≈ `40 × (30/40) = 30`（不是满血 40） |
| 3 | 三只同时接触 | `SlimeSpawnNormalAtMass 1 3` | 只有两只融合，第三只回到 `no fusion target` 继续游走；**任何一只都不会同时连到两个对象**（`DrawFusion` 的连线只有一对） |
| 4 | 接近途中被打死 | `SlimeSpawnNormalAtMass 1 6` 或 `SlimeForceFuse` 后立刻 `SlimeDamageNearestEnemy 999` | 存活方立刻回漫游（无 1s 延迟）、连线消失；**只结算被打死那一只一次分数** |
| 5 | 体量 8 锁定 | `SlimeSpawnNormalAtMass 8 1`，或反复 `SlimeSetMass 4` 配对 | 体量到 8 的融合体打上 `State.Enemy.Normal.MassLocked`，不再连线、只在本点位游走 |
| 6 | 取消后 1s 重试 | 两只都生成后立刻把其中一只 `SlimeSetMass 8`（体量和超上限） | 出现 `Cooling`，标签显示 `retry in 1.0s`，1 秒后回到 `Idle` 再找对象 |
| 7 | 追兵碰撞 | `SlimeSpawnNormalAtMass 1 4` + `SlimeSpawnAggro 4` | 追兵被史莱姆/融合体挡住时会绕行，不穿模、不瞬移、不隔墙扣血；蓄势中跑开仍落空 |
| 8 | 融合中可被射杀 | 用 `SlimeSpawnNormalAtMass 1 2` 看进度，进度到一半时开枪 | 命中正常扣血；若在结算前打死一只，按场景 4 处理；**融合体不会被"射不中"**（`SlimeFusion` 通道不影响 `ECC_Visibility`） |
| 9 | P2 死亡链路 | 换弹中（8 发打空自动换弹）立刻 `SlimeKillPlayer` | 换弹被取消、不能再射击、`ApplyDeathEffects` 只跑一次（日志一条 `player died` + 一条 `OnPlayerDied`） |
| 10 | P2 受击保护 | 让追兵打中玩家一次，0.6s 内再打中 | 第二次**不扣血**、不刷新保护窗；日志出现 `is invulnerable, X damage dropped` |

调试命令一览（本 Phase 新增的三条加粗）：

```text
SlimeSpawnNormalAtMass <Mass> <Count>   # 在玩家前方密集生成指定体量的普通史莱姆
SlimeSetMass <Mass>                    # 把最近的普通史莱姆改成指定体量
SlimeForceFuse                         # 强制最近两只同点位普通史莱姆配对
SlimeSpawnNormal [Count] / SlimeSpawnAggro [Count] / SlimeClearEnemies / SlimeKillPlayer
SlimeDamageNearestEnemy <Amount> / SlimeDumpTables / SlimeReloadTables
```

CVar：`Slime.Debug.DrawFusion 1`（配对连线、接触进度、会合点、容差圈、冷却倒计时）、
`Slime.Debug.DrawEnemyState 1`、`Slime.Debug.CombatLog 1`。

---

## 6. 提交前别忘了

| 要改的资产 | 说明 |
|---|---|
| `Content/_SlimeWar/Core/Data/DA_RunConfig.uasset` | 第 2 步的 5 个 `AI\|Fusion` 字段 |
| `Content/_SlimeWar/Enemy/ST_SlimeNormal.uasset` | 第 3 步的 2 条转换 |

其余全部是文本/代码，已在仓库里。提交前 `git status` 自查：

- [ ] 没有 `Binaries/` `Intermediate/` `Saved/` `DerivedDataCache/` `.vs/` `*.sln`
- [ ] `Docs/PhaseB/*`、`Config/DefaultEngine.ini`、`Source/SlimeWar/**` 都在
- [ ] 改了 `.uasset` 前已在群里认领（GitHub 不支持文件锁，见 `Docs/Collaboration.md`）

> 若 CP-2 有任何一项不过，先在 `Slime.Debug.DrawFusion 1` + `Slime.Debug.CombatLog 1` 下复现并记录
> 当时的 `fusion <状态>` 与日志，再回到代码定位——状态机会在标签上如实显示它卡在哪一步。

### 常见现象：两只配对了却不融合、反复重试

日志里出现重复的 `fusion pairing accepted` + `fusion approach made no progress for 2.00s`，
说明配对成功但 2 秒内没有进入接触范围。先看这条日志自带的三个数字：

| 日志现象 | 含义 | 处理 |
|---|---|---|
| `gap` 一直在缩小、只是走得久 | 两只隔得远，是**路程问题**，不是卡住 | 把 `AI | Normal → Approach Timeout` 调大（例如 `6`），这是纯数据改动，不用重编译 |
| `gap` 完全不变且 `move status 0`（Idle） | 移动请求没生效：会合点不可达或没有 NavMesh。日志里应同时有一条 `could not start a move towards the partner` 警告 | 检查关卡有没有 `NavMeshBoundsVolume` 且覆盖整块地面，按 `P` 看绿色可行走区 |
| `gap` 不变但 `move status 3`（Moving） | 被几何挡住、绕不过去 | 检查白盒是否有夹缝，或把该点的活动半径/出生位置调开 |
| `gap` 停在接触阈值上方几厘米来回抖 | 到达判定把胶囊半径算进去了（**已修**，见下） | 更新代码后重试 |

> `move status`：`0`=Idle、`1`=Waiting、`2`=Paused、`3`=Moving。
> 另外 `Slime.Debug.DrawFusion 1` 会画出会合点（小球）与容差圈，能直接看出两只是否朝着同一个点走。

> **已修的两个引擎默认值坑**（2026-10-06 第二次实机反馈）：
> 1. `FAIMoveRequest` 默认的到达判定会**加算胶囊半径**。`MoveToLocation(中点, 10cm)` 实际是
>    "进入 ~50cm 就算到了"，于是两只史莱姆都判定"已到达"而原地不动（`gap` 恒定、`move status 0`）。
>    现在改为关掉该加算、按圆心对圆心判定。
> 2. 玩家与史莱姆胶囊互相 Block 时，移动推进会把对方挤出去，表现为"史莱姆被踢飞"。现在玩家胶囊
>    对 `SlimeFusion` 通道改为 Overlap（互不阻挡），不再踢飞、也不再干扰融合。
>    仅改玩家一侧还不够：史莱姆自己的移动扫掠仍会把玩家当阻挡物而把它弹出去，所以史莱姆胶囊另外
>    对玩家做 `IgnoreActorWhenMoving`（`IgnorePlayerForMovement`），两个方向都不再产生推挤。
> 3. 玩家钻进史莱姆时镜头被拉进身体：弹簧臂的 `ProbeChannel`（`ECC_Camera`）默认被 Pawn 胶囊 Block。
>    现在史莱姆胶囊对 `ECC_Camera` 改为 Ignore，墙照常挡镜头。
> 4. 追兵被史莱姆挡住后原地磨蹭（`is stuck and failed to move!`）：`Chase` 任务在移动 Idle 且玩家
>    在攻击范围外时自动重新寻路。
>    进一步：追兵现在走 **Detour Crowd** 路径跟随（`UCrowdFollowingComponent`），会主动绕开挡路的史莱姆；
>    普通史莱姆只作为"障碍"被避开、自己不做任何避让（否则融合会被破坏）。
>    `Config/DefaultEngine.ini` 里 `[/Script/AIModule.CrowdManager] MaxAgents=100`。
> 5. 打地板/静物不再刷 Warning；追兵死亡不再进计分入口（`Kind != Normal` 在 `OnEnemyKilled` 里被挡掉）。

> **Detour Crowd 验收**：① 用 `SlimeSpawnNormalAtMass 8 8` 摆一堵人墙，`SlimeSpawnAggro 4` 后追兵能绕过
> 人墙打到玩家；② 两只普通史莱姆仍然正常融合（用时不应明显变长）；③ 不再持续刷
> `is stuck and failed to move!`；④ 180 只同屏时看帧率，必要时调大 `MaxAgents`。
>
> 判断"是否真的绕行"看 `Slime.Debug.DrawAggroPath`（默认开）：
>
> | 画面 | 含义 |
> |---|---|
> | 橙色轨迹绕开了人墙、弯出一条弧线 | ✅ 避让生效 |
> | 青色路径直穿人墙，橙色轨迹几乎原地打转、标签显示 `Idle` | ❌ 还在顶墙（看是否 `MaxAgents` 不够 / 该 agent 没进 crowd） |
> | 轨迹正常前进但没有明显弯曲 | 阻挡物自己让开了（普通史莱姆在游走），不算问题 |
>
> 青色 = 计划路径（NavMesh），橙色 = 真实轨迹；两者分叉的地方就是避让在起作用。
