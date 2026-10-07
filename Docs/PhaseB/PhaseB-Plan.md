# Phase B 实现计划 —— 融合与追兵完整

> 状态：**代码侧已落地并编译通过**（`SlimeWarEditor Win64 Development`，0 error / 0 warning）。
> 剩下的是编辑器步骤与 CP-2 实机验收，见 [PhaseB-Checklist.md](PhaseB-Checklist.md)。
> 数值全部来自 `DA_RunConfig` / DataTable，代码与资产里不写数字（铁律 5）。

---

## 1. 范围与依赖

| 任务 | 内容 | Owner | 状态 |
|---|---|---|---|
| PB-09 | `USlimeFusionComponent`：配对请求/接受/拒绝握手 | P1 | 代码完成 |
| PB-10 | 双方朝彼此之间空地接近（不穿墙）+ 靠近失败检测 | P1 | 代码完成 |
| PB-11 | 持续接触 0.4s 判定，任一条件失效即取消 | P1 | 代码完成 |
| PB-12 | 融合结算：体量求和 + 按剩余生命比例继承 | P1 | 代码完成 |
| PB-13 | 融合后 1s 才能再融合；体量 8 锁死 | P1 | 代码完成 |
| PB-14 | 融合过程中被杀 → 取消，只结算被打死那只一次 | P1 | 代码完成 |
| PB-15 | 三只同时接触只融合两只；同一只不参与两次 | P1 | 代码完成 |
| PB-16 | 追兵被普通史莱姆挡住但绕行 | P1 | 代码完成 |
| PA-10 | 死亡时 `CancelAbilities`（取消换弹、禁止射击） | P2 | 代码完成（终局半边留 Phase D） |
| PA-11 | `GA_Die`：加 `State.Player.Dead` → 取消能力 → `OnPlayerDied` | P2 | 代码完成 |
| PA-16 | 受击保护验证（`State.Player.Invulnerable` 期间不再扣血） | P2 | 待 CP-2 验收 |

依赖：PB-03 的目标选择（Phase A 已有）、Phase A 的 StateTree AI 与 `USlimeCombatSubsystem`。
**不依赖**：生成点（Phase C）、HUD（Phase D）、对象池（PC-08）。

## 2. 设计决策（已确认）

1. **逻辑落点**：每只普通史莱姆挂 `USlimeFusionComponent`；StateTree 任务只负责移动与驱动，
   组件持有配对真相、接触计时与结算。
2. **融合体生成**：**发起方就地变身**（保留 Actor / AIController / StateTree），被动方 `Destroy()`；
   Phase C 再把它换成回池。
3. **接近与接触**：融合中的两只由专用碰撞通道 + `IgnoreActorWhenMoving` **互相忽略**（允许重叠，
   但保持与地面、墙壁、玩家、追兵、武器射线的碰撞）；接触判定为"球心距 ≤ 两胶囊半径和 + 容差"，
   累计满 `FusionContactTime` 才结算。
4. **追兵碰撞**：实体阻挡 + NavMesh 路径绕行，不启用 Crowd 避让。
5. **StateTree 改动**：最小化 —— 只加 2 条转换，`Hold` 状态与节点显示名不动。
6. **取消与重试**：一切非死亡取消 → 回漫游并等 `FusionRetryDelay`（1s）再找对象；
   死亡取消 → 存活方立即恢复，无延迟、不拉黑。

## 3. 状态机

```
Idle ──BeginPairing(对方接受)──► Approaching ──进入接触距离──► Contacting
  ▲                                   │                          │
  │                                   │ 超时/条件失效/分开        │ 累计满 FusionContactTime
  │                                   ▼                          ▼
  └────────── Cooling(0 / 1s) ◄────── CancelPairing          ResolvePairing
                                                             (发起方执行)
```

- `IsEngaged()` = `Approaching | Contacting | Cooling` → 期间**拒绝一切新配对**，这是 PB-15 的实现手段。
- `Cooling` 期间 `Slime: Has Fusion Target` 仍为真，`Hold` 状态得以覆盖"融合后等待 1s"，不需要新状态。
- 死亡/被销毁：`ASlimeEnemyBase::HandleDeath` 与组件的 `EndPlay` 都会通知对方，存活方立即释放。

## 4. 接触与移动

- 会合点 = 双方配对瞬间位置的中点，投影到 NavMesh（失败则退回中点）；**双方用同一个点**，
  由发起方计算并通过 `RespondToRequest` 传给被动方。
- 移动请求用 `FAIMoveRequest` 显式**关掉引擎默认的到达判定加算**（`FReachTest` 默认会把
  胶囊半径算进"到达"，使 `MoveToLocation(中点, 10cm)` 实际变成 ~50cm，两只间距 1m 时会直接返回
  `AlreadyAtGoal`，谁都不动），改为**圆心对圆心**判定，半径 = `FusionContactTolerance`；
  会合点若不可达，退化为直接朝对方移动，而不是原地站着等超时。
- `Hold` 任务的 `Tick` 在下达的移动不是 `Moving` 且离会合点仍大于到达半径时重新下达请求。
- 接触距离 = `胶囊半径A + 胶囊半径B + FusionContactTolerance`。
- 接近超时复用 `AIApproachTimeout`（2s），语义是**"连续 2 秒没有继续靠近"**（策划案 4.6.2），
  不是"配对后 2 秒内必须到"：只要间隔仍在缩小就重置计时器（缩小量 ≥ 到达半径才算进展）。
  超时日志会打印当前间隔、接触阈值、历史最近间隔与移动状态，用来区分"路太长"和"根本没动"。

## 5. 结算（PB-12 / PB-13）

```
NewMass      = Σ Mass(participants)                      // 上限 AIFusionMassCap
Fraction     = Σ Health / Σ MaxHealth                    // 分母 ≤ 0 时取 1
NewHealth    = 新体量的 MaxHealth × Fraction
```

- 与策划案 4.4 一致：无伤融合满血、有伤融合不会完全恢复、绝对生命池可能增加（明确的合并成长效果）。
- 结算后：`OnEnemyFused(NewMass)`（只统计，不计分）；体量到 cap 时打
  `State.Enemy.Normal.MassLocked`，只在本点位活动。
- **融合不产生击杀分**；只有被打死的那一只走 `OnEnemyKilled`，且只结算一次。

## 6. 碰撞通道

- `Config/DefaultEngine.ini` 新增对象通道 `SlimeFusion` = `ECC_GameTraceChannel1`，默认响应 Block。
- `ASlimeNormal` 胶囊改用它；`ASlimeAggro` 保持 `ECC_Pawn` → 对史莱姆 Block（PB-16）。
- 配对期间双方 `IgnoreActorWhenMoving(true)`，取消/融合完成后恢复 `false`。
- 玩家胶囊对 `SlimeFusion` 通道改为 **Overlap**：玩家与普通史莱姆互不阻挡，因此不会把史莱姆
  "踢飞"（两个胶囊互撞时移动推进会把对方挤出去），也不会干扰正在建立的融合。追兵仍保持
  `ECC_Pawn` 的 Block 响应，PB-16"追兵被史莱姆挡住并绕行"不受影响。
  若要恢复"玩家被史莱姆挡住"，改 `ASlimeWarCharacter` 构造函数里那一行为 `ECR_Block` 即可，
  代价是重新出现互相推挤。
- 同时 **史莱姆胶囊对玩家做 `IgnoreActorWhenMoving`**（`ASlimeEnemyBase::IgnorePlayerForMovement`）：
  只靠玩家一侧的 Overlap 不够——史莱姆朝会合点走、玩家胶囊嵌在它身上时，史莱姆自己的移动扫掠
  仍会把玩家当阻挡物，起步即穿透 → 引擎沿穿透法线把它弹出去，这就是"顶飞"。该调用是移动级别的
  忽略（不查表、不改通道响应），在 `BeginPlay` / `InitializeFromSpawn` / 融合配对建立 / 追兵进入
  Chase 时各刷新一次，玩家重生也能覆盖。
- **相机**：史莱姆胶囊对 `ECC_Camera` 改为 **Ignore**。弹簧臂用 `ProbeChannel = ECC_Camera` 做防穿墙探测，
  而 Pawn 预设只覆盖了 `Visibility`、`Camera` 走通道默认的 Block，于是玩家钻进史莱姆时镜头会被拉进身体。
  墙仍然是 `WorldStatic`，照常挡镜头。
- **追兵卡住自愈**：`Slime: Chase Player` 的 `Tick` 在"移动已 Idle 且玩家仍在攻击范围外"时重新下达
  `MoveToActor(玩家)`。此前路径被史莱姆挡住后 move 结束、任务却一直 Running，追兵会原地磨蹭
  （日志里的 `is stuck and failed to move!`）。符合策划案 4.6.4"暂时无路则原地等待并再尝试"。

### 3.1 寻路与局部避让（Detour Crowd）

- 寻路本身仍是 **NavMesh A***（`MoveToActor` / `FAIMoveRequest` → 路径跟随组件），只认识烘焙的静态几何；
  对"别的史莱姆"这类动态阻挡，靠 Detour Crowd 的 agent 避让来绕。
- `ASlimeAIController` 的路径跟随组件已换成 `UCrowdFollowingComponent`
  （构造里 `ObjectInitializer.SetDefaultSubobjectClass<...>("PathFollowingComponent")`），
  并在 `OnPossess` 里按类型配置：
  - **追兵**：`SimulationState = Enabled` + `SetCrowdObstacleAvoidance(true)` + `AvoidanceQuality = Good`
    → 会绕开挡路的史莱姆与玩家（分离力保持引擎默认的关闭，避免贴近攻击距离时被推开）。
  - **普通史莱姆**：保持注册（这样追兵才"看得见"它们），但 `ObstacleAvoidance / Separation /
    AnticipateTurns / PathOffset / OptimizeVisibility / OptimizeTopology` **全部关闭**
    → 自己不做任何避让，仍然可以互相重叠完成融合。
    > 注意：`ECrowdSimulationState::ObstacleOnly` 在 UE 5.5 里**只是注册开关**（该枚举在引擎里没有
    > 被别处读取），所以真正生效的是上面这些 per-agent flag，不能只设枚举。
- `Config/DefaultEngine.ini` 新增 `[/Script/AIModule.CrowdManager] MaxAgents=100`（默认 50，
  装不下 144 普通 + 36 追兵的理论峰值）；超出上限的 agent 不会参与避让，压测时如仍不够只需改这个数字。
  另外 `MaxAgentRadius` 默认 100cm，融合体量 8 的半径是 60cm，在限内。
- **调试可视化 `Slime.Debug.DrawAggroPath`（默认开）**：每个追兵画两样东西——**青色**是路径跟随
  当前计划走的 NavMesh 路径（含拐点小球），**橙→灰渐变**是它过去约 8 秒真实走过的轨迹，头顶标签是
  `名字 / 移动状态(Idle|Waiting|Paused|Moving) / 当前速度`。避让是"计划之外"的偏移，所以只有轨迹能
  回答"到底绕过去了没有"。
- ⚠️ 按对象类型查敌人的代码必须补这个通道。本项目只有辅助瞄准一处
  （`ASlimeWarCharacter::GetAimDirectionWithAssist`），已同步补上。

## 7. 数值（`DA_RunConfig` → `AI|Fusion`）

| 字段 | 建议值 | 出处 |
|---|---|---|
| `FusionContactTime` | 0.4 | 策划案 4.4 |
| `FusionContactTolerance` | 10（cm） | 新增占位（Q14），只吸收帧步长 |
| `FusionPostFusionDelay` | 1.0 | 策划案 4.4 |
| `FusionRetryDelay` | 1.0 | 策划案 4.6.2 |
| `FusionMaxParticipants` | 2 | 多方融合的预留口（Q14） |

## 8. 涉及文件

- 新增：`Public/Enemy/SlimeFusionComponent.h`、`Private/Enemy/SlimeFusionComponent.cpp`、
  `Public/Core/SlimeWarCollisionChannels.h`
- 改动：`SlimeEnemyBase`、`SlimeNormal`、`SlimeStateTreeNodes`（Hold / Select / HasFusionTarget）、
  `SlimeHealthComponent`、`SlimeRunConfig`、`SlimeWarCVars`、`SlimeHUD`、`SlimeCheatManager`、
  `SlimeWarCharacter`、`GA_Die`、`Config/DefaultEngine.ini`
- **没有**改动：`IBattleDirector` 与 `Public/Core` 的任何 GAS 边界（C18 红线保持）

## 9. 测试与验收

见 [PhaseB-Checklist.md §CP-2 验收](PhaseB-Checklist.md)：两只正常融合 / 有伤融合 / 三只接触 /
接近途中被打死 / 体量 8 锁定 / 取消后 1s 重试 / 追兵被挡绕行 / 融合中仍可被射杀，
外加 P2 的死亡取消换弹与受击保护验证。

## 10. 假设与已知边界

- 关卡需有 NavMesh 与可站立地面（Phase A 已要求）；无 NavMesh 时会合点退化为中点并可能接近失败。
- 被动方目前 `Destroy()`，回池留到 PC-08；池化时需要把 `EndPlay` 的配对清理一并接上。
- `PA-10` 只覆盖死亡路径；终局（`State.Player.Result`）随 `URunSubsystem` 在 Phase C/D 接线。
- 目标选择（PB-03）每次选择会遍历全场敌人，180 只时是"每次选择 O(n²)"、非每帧；CP-2 后按需优化。

## 11. 已知问题与后续改进（延后，不阻塞 CP-2）

> 全部来自 2026-10-06 的实机日志；**Detour Crowd 已确认生效**（日志 `crowd: obstacle only` /
> `crowd: enabled`），下列问题与 Crowd 无关。

### 已知问题 1：普通史莱姆之间会深度穿插并永久卡死（最严重 → PB-19）

- 证据：`SlimeNormal_19 stuck ... PenetrationDepth:59.714 Actor:SlimeNormal_10`、
  `SlimeNormal_16 ... 52.715 Actor:SlimeNormal_25`、`SlimeNormal_20 ... 19.008 Actor:SlimeNormal_11`
  （带 "58 other events"＝持续卡住）。mass 8 半径 60，穿透 50+cm 说明球心只隔 ~65cm，而正常接触距离是 120cm。
- 根因：两处都会造成"先重叠、再恢复阻挡"的顺序——
  ① 配对期间双方 `IgnoreActorWhenMoving`，配对**被取消**时在重叠状态下恢复 Block；
  ② 融合时胶囊**变大**（mass 4→8 半径 37→60），把紧邻的个体"裹"进去。
  深度穿插一旦形成，`ResolvePenetration` 推不开，双方都动不了（追兵同样会被裹住：日志里
  `SlimeAggro_1 ... PenetrationDepth:12.132 Actor:SlimeNormal_21`）。
- 建议做法：`ASlimeNormal` 胶囊把 `SlimeFusion` 通道响应设为 **Ignore**，普通史莱姆彼此不再有任何物理交互。
  融合靠**距离判定**（球心距 ≤ 半径和 + 容差），不依赖碰撞；追兵仍被史莱姆挡住
  （`ECC_Pawn` ↔ `SlimeFusion` 是另一对通道），**PB-16 不受影响**；配对期的 `IgnoreActorWhenMoving`
  变为冗余但无害。代价：普通史莱姆之间可能视觉重叠（设计未要求它们互相阻挡，且融合时本来就要重叠）。

### 已知问题 2：超远距离配对导致接近空转（→ PB-20）

- 证据：`[SlimeNormal_5] ... gap to SlimeNormal_8 is 496cm (contact at 67cm, closest 499cm, move status 0)`；
  同类还有 `gap 184cm (closest 169cm, move status 0)`（先靠近 15cm 后被顶住）。
- 根因：目标选择只按"最近"挑、不限制距离，同一活动区（半径 6m）两端的个体也会配对上。之后要么
  会合点不可达（空路径、每帧重下请求 → 空转），要么中途被别的史莱姆顶住（`move status 0`）。
- 建议做法：`DA_RunConfig` 增加 `FusionMaxPairDistance`（建议取活动半径的一半左右），
  `Slime: Select Fusion Target` 跳过超出该距离的对象。

### 已知问题 3：接近失败只能等满 `AIApproachTimeout`（→ PB-21）

- 现状：`AdvanceApproach` 用"连续 N 秒无进展"判定，被顶住时要浪费满 2 秒才换目标。
- 建议做法：连续若干帧"重下移动请求后位置仍无位移"就立即取消换目标；同时在超时日志里补上
  `MoveTo` 的返回值（`Failed / AlreadyAtGoal / RequestSuccessful`）与移动目标，便于区分"空路径"与"被实体顶住"。
