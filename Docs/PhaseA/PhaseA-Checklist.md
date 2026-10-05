# Phase A · 编辑器步骤与 CP-1 验收

> 代码侧已经全部就位并**编译通过**（`SlimeWarEditor Win64 Development`，0 error 0 warning）。
> 下面 2~7 步只能在 UE 编辑器里做（`.uasset` / `.umap` 无法在编辑器外生成）。
> 顺序有依赖：**先第 2 步（导入表）和第 3 步（填 DA），否则玩家属性和 AI 都是 0**。

---

## 1. 打开工程

本次改了 `SlimeWar.uproject`（新增 `StateTree` / `GameplayStateTree` 插件）和 `SlimeWar.Build.cs`
（新增 `AIModule` / `NavigationSystem` / `StateTreeModule` / `GameplayStateTreeModule`）。

1. 关掉正在运行的编辑器（如果开着）。
2. 双击 `SlimeWar.uproject`。若弹出"以下插件缺失/需要重启"，选 **Enable / Restart Now**。
3. 若提示模块需要重新编译，选 **Yes**。

---

## 2. 导入追兵属性表 `DT_AggroStats`

源文件：`Content/_SlimeWar/Enemy/Data/DT_AggroStats.csv`

1. 在 Content Browser 里定位到 `Content/_SlimeWar/Enemy/Data/`，选中 `DT_AggroStats.csv`。
2. 右键 → **Import**（或 "Import to /Game/_SlimeWar/Enemy/Data"）。
3. **Row Struct** 选 `Slime Aggro Stat Row`（`FSlimeAggroStatRow`）。
4. 命名 `DT_AggroStats`，路径 `/Game/_SlimeWar/Enemy/Data/`。
5. 核对：1 行，行名 `Default`，数值 60 / 480 / 60 / 120 / 20 / 0.5 / 0.35 / 1.6。

> ⚠️ 这些数字是 **PLACEHOLDER**。`AttackDamage = 20` 策划案里没有给，属新开待确认项（Q13）。

---

## 3. 给 `DA_RunConfig` 填新增字段

打开 `/Game/_SlimeWar/Core/Data/DA_RunConfig`，把下面这些**新加**的字段填上（原来的字段不用动）。

### 3.1 Camera / 武器

| 字段 | 值 | 说明 |
|---|---|---|
| Camera Boom Length | `220` | TPS 肩后距离（cm），原来模板是 400 |
| Camera Socket Offset | `(0, 60, 40)` | Y 正方向把镜头推到角色右肩 |
| Camera Field Of View | `90` | 0 = 用引擎默认 |
| Muzzle Offset Local | `(40, 25, -15)` | 相机空间下的枪口偏移（cm），Phase A 没有武器网格 |
| Aim Assist Max Angle Deg | `6` | 辅助瞄准锥半角 |

### 3.2 移动

| 字段 | 值 |
|---|---|
| Player Acceleration | `2048` |
| Player Braking Deceleration | `2048` |

### 3.3 AI \| Normal

| 字段 | 值 | 策划案出处 |
|---|---|---|
| Spawn Wait Time | `2` | 4.2 "生成后先等待 2 秒" |
| Activity Radius | `600` | 5.4 "普通活动半径 6 米" |
| Wander Radius | `300` | 4.2 "不超过 3 米的小幅移动" |
| Wander Pause Min | `0.5` | 4.2 "停留 0.5 至 1.5 秒" |
| Wander Pause Max | `1.5` | 同上 |
| Approach Timeout | `2` | 4.6.2 "连续 2 秒无法继续靠近" |
| Fusion Mass Cap | `8` | 4.4 "合计体量不得超过 8" |

### 3.4 AI \| Aggro

| 字段 | 值 |
|---|---|
| Aggro Stat Table | `DT_AggroStats` |
| Aggro Row Name | `Default` |

> 灵敏度不在这里：仍然用 `Config/DefaultInput.ini` 的 `MouseX/MouseY = 0.07`（文本文件，好合并）。

---

## 4. 沙盒地图 `L_Sandbox_OnePoint`

打开 `/Game/_SlimeWar/Maps/L_Sandbox_OnePoint`。

1. **加 NavMesh**：Place Actors → 搜 `NavMeshBoundsVolume` → 拖进关卡 →
   缩放成覆盖整块地面（例如 Scale `(20, 20, 5)`）→ 位置 Z 抬高到地面之上（约 `200`）。
   放置后应该自动生成一个 `RecastNavMesh` Actor；按 `P` 能看到绿色可行走区域。
2. **换掉旧敌人**：地图里原来是 `SlimeEnemyBase` 实例，现在基类已变成 `abstract`，
   把它删掉，改放一个 `SlimeNormal`（可选：调试命令也能随时生成）。
3. **保存地图**。

> 沙盒地图不要设成默认地图，`Config/DefaultEngine.ini` 保持指向 `ThirdPersonMap`。

---

## 5. 建两个 StateTree 资产

放在 `/Game/_SlimeWar/Enemy/`，命名必须是 `ST_SlimeNormal` / `ST_SlimeAggro`
（C++ 里写死了这两个路径：`ASlimeNormal` / `ASlimeAggro` 构造函数）。

### 5.1 新建时的 Schema 设置（两个都一样）

- Schema：**StateTree AI Component**
- **AIController Class**：`SlimeAIController`
- **Context Actor Class**：`Character`（**不是** `SlimeEnemyBase`，见下方说明）

> ⚠️ **为什么选 `Character` 而不是 `SlimeEnemyBase`**
> `ASlimeEnemyBase` 在 Phase A 被改成了 `UCLASS(Abstract)`，而 UE 的类选择器**默认过滤抽象类**，
> 所以它根本不会出现在 `Context Actor Class` 下拉里（`ASlimeNormal` / `ASlimeAggro` 可以）。
> 这个字段只用于校验上下文 Actor 类型和属性绑定；我们的节点不依赖属性绑定
> （C++ 里是从 `AIController->GetPawn()` 取 Pawn 再 `Cast<ASlimeEnemyBase>`），
> 所以填 `Character`（或 `Pawn`）功能完全一样，校验也能通过：
> `ASlimeNormal → ASlimeEnemyBase → ACharacter`。
>
> **自检**：在下拉里搜 `Slime`。能看到 `SlimeNormal` / `SlimeAggro` → 就是抽象类的问题，选 `Character` 即可；
> 连 `SlimeNormal` 都看不到 → 编辑器还在用旧 DLL，关掉编辑器重新编译再打开。
>
> 节点参数**不用手动绑**：所有数值由 C++ 从 `DA_RunConfig` / `DT_AggroStats` 读，
> 但节点要能被拖出来，必须先在编辑器里编译过一次 C++（已编译）。
>
> **如果编译 ST 时报 `Malformed task/node, missing instance value`**：StateTree 要求**每个节点**
> （Task 和 Condition 都算）都必须声明 instance data——编辑器建节点时只会用
> `GetInstanceDataType()` 的结果去创建 `Instance` / `InstanceObject`，返回 null 就直接判为非法。
> 本项目所有节点都已声明（不需要参数的用空的 `FSlimeSTInstanceDataEmpty`）。
> 如果是在修复前建的节点，**重启编辑器**让资产重新 PostLoad 即可自动补上
> （`UStateTreeState::PostLoad` → `ConditionalUpdateNodeInstanceData`）；万一还报，
> 把那个节点删掉重新从下拉里加一次。


### 5.2 `ST_SlimeNormal` 结构

Root 下依次建 5 个子状态，每个状态挂**一个** Task：

| 状态名 | Task | 转换（Transitions） |
|---|---|---|
| `SpawnWait` | `Slime: Spawn Delay` | **On State Completed** → `Wander` |
| `Wander` | `Slime: Wander Step` | **On State Completed** → `Pause` |
| `Pause` | `Slime: Wander Pause` | **On State Completed** → `SelectTarget` |
| `SelectTarget` | `Slime: Select Fusion Target` | On State Succeeded → `Hold`；**On State Failed** → `Wander` |
| `Hold` | `Slime: Hold Position` | **Condition** 触发器 + 条件 `Slime: Has Fusion Target`，在该条件的 Details 里勾选 **Invert** → `Wander` |

行为：等 2s → 游走（≤3m）+ 停 0.5~1.5s → 找同点位、未配对、体量和 ≤8 的最近对象；
找到就把目标记在自己身上、打 `State.Enemy.Normal.Fusing` 标签并停住（**Phase A 到此为止**）。

### 5.3 `ST_SlimeAggro` 结构

| 状态名 | Task | 转换（Transitions） |
|---|---|---|
| `Chase` | `Slime: Chase Player` | ①**Condition** 触发器 + 条件 `Slime: Can Attack` → `Windup`；② **On State Completed** → `Stop` |
| `Windup` | `Slime: Attack Windup` | **On State Completed** → `Resolve` |
| `Resolve` | `Slime: Attack Resolve` | **On State Completed** → `Recover` |
| `Recover` | `Slime: Attack Recover` | **On State Completed** → `Cooldown` |
| `Cooldown` | `Slime: Attack Cooldown` | **On State Completed** → `Chase` |
| `Stop` | `Slime: Stop` | 无 |

> **触发器怎么选（很容易搞错）**：每个状态行右侧有 4 个图标——
> `Condition`（= OnTick\|OnEvent，每帧判定条件用这个）、`On State Completed`（成功**和**失败都接）、
> `On State Succeeded`、`On State Failed`。**新增转换的默认值是 On State Completed**。
>
> **关于 Invert**：StateTree **没有全局取反开关**。"Invert" 是**每个条件节点自己**提供的一个属性
> （引擎自带的 `FStateTreeCompareIntCondition` 等就是 `bInvert` + `bResult ^ bInvert`）。
> 本项目三个 C++ 条件（`Slime: Has Fusion Target` / `Slime: Player Alive` / `Slime: Can Attack`）
> 都带了这个勾选框，位置在选中条件后的 Details 面板 → **Parameter** 分类下。
> 如果看不到，说明编辑器还在用旧 DLL：关掉编辑器重新编译一次再打开。
>
> - 需要"每帧检查条件就切状态"的（`Chase` 的 CanAttack、`Hold` 的没有融合目标）→ 必须用 **Condition**，
>   用 On State Succeeded 永远不会触发。
> - 线性推进（成功失败都往下走）→ 用 **On State Completed**，比只写 Succeeded 稳。
> - 只有真正要分叉的地方（`Select Fusion Target`）才用 Succeeded / Failed 分开写。
>
> **失败没人接会怎样**：引擎会从完成的状态沿激活链往上找匹配的转换（自己→父→祖父…），
> 同一状态内按**编辑器里从上到下的顺序**判定，第一条通过的执行；整条链都没人接时，
> 会打一条 warning `Could not trigger completion transition, jump back to root state.`
> 并把**根状态重新选一次**（整棵树从头再来）。所以"只写 On Succeeded"的状态一旦失败就会静默重启整棵树。
>
> **任务一直 Running 的状态不受这条规则影响**：只有任务返回 Succeeded/Failed 时状态才"完成"，
> 完成转换（含回根兜底）才会被评估。所以 `Stop`（任务永远 Running、没有任何转换）是**真正的终态**，
> 它会一直停在那里，不会回根；`Hold Position` 同理，它靠 **Condition（OnTick）** 转换离开——
> 要是给它写"On State Succeeded"，永远不会触发（因为它压根不会完成）。
> 若想给 `Stop` 额外上保险，可加一条 `On State Completed → Stop` 的自转换。
>
> 本项目的 C++ 节点里只有 `Slime: Select Fusion Target` 会返回 Failed（找不到融合对象），
> 其余任务在异常分支都返回 Succeeded，`Hold Position` / `Stop` 永远 Running（靠 Condition 转换离开）。

行为：4.8 m/s 追击（NavMesh 绕行）→ ≤1.2m 且无遮挡 → 停移 + 0.5s 蓄势（锁定朝向）→
重新判定命中/落空 → 0.35s 收势 → 冷却到"两次攻击起始间隔 ≥1.6s" → 再追。
玩家死亡后进 `Stop`。

> 冷却不是额外的计时器：`Cooldown` 状态时长 = `AttackCooldown - AttackWindupTime - AttackRecoverTime`，
> 所以间隔天然 ≥1.6s，改数值只改 `DT_AggroStats` / `DA_RunConfig`，不用碰 StateTree 资产。

---

## 6. 输入资产

### 6.1 新建 4 个 Input Action

路径 `/Game/_SlimeWar/Player/Input/Actions/`，**Value Type 全部选 `Digital (bool)`**：

| 资产 | 键位 |
|---|---|
| `IA_Fire` | 鼠标左键（按住连射） |
| `IA_Reload` | `R` |
| `IA_Aim` | 鼠标右键（按住） |
| `IA_Pause` | `Escape` + `P` |

> `Escape` 在 PIE 里会被编辑器截走，所以同时绑 `P` 方便测试；正式打包用 `Escape` 没问题。

### 6.2 新建 `IMC_SlimeWar`

路径 `/Game/_SlimeWar/Player/Input/`，包含：

| Key | Action |
|---|---|
| `W/A/S/D` | `IA_Move`（沿用模板的，在 `Content/ThirdPerson/Input/Actions/`） |
| `Mouse XY` | `IA_Look`（模板） |
| `LMB` | `IA_Fire` |
| `RMB` | `IA_Aim` |
| `R` | `IA_Reload` |
| `Escape`、`P` | `IA_Pause` |

### 6.3 接线到 `BP_ThirdPersonCharacter`

打开 `Content/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`，在 Details 面板：

| 属性 | 设成 |
|---|---|
| Input → Default Mapping Context | `IMC_SlimeWar` |
| Input \| Ability → Fire Action | `IA_Fire` |
| Input \| Ability → Reload Action | `IA_Reload` |
| Input \| Ability → Aim Action | `IA_Aim` |
| Input \| Ability → Pause Action | `IA_Pause` |

（`Move Action` / `Look Action` 保持原样，不用改。）

---

## 6.5 准星与调试可视化（C++ 实现，无需资产）

Phase A 用 `ASlimeHUD`（C++）提供临时准星，`HUDClass` 已在 `ASlimeWarGameMode` 里设好，
Phase D 做真 HUD 时替换即可。控制台命令（PIE 里按 `` ` ``）：

| CVar | 默认 | 作用 |
|---|---|---|
| `Slime.Debug.Crosshair 1` | **开** | 屏幕中心十字准星（无 HUD 资产） |
| `Slime.Debug.DrawEnemyState 1` | 关 | 每个敌人头顶显示：状态标签、体量、当前血量、是否选中融合对象；普通史莱姆还会画 6m 活动圈，并**用绿线连到它选中的融合对象** |
| `Slime.Debug.DrawAimAssist 1` | 关 | 白线=相机准心方向，黄线=辅助瞄准修正后的实际开火方向，绿线=吸附到的目标 |
| `Slime.Debug.CombatLog 1` | 关 | 伤害/死亡/换弹等日志，**并会打印 `fusion target selected: ...`** |

### 怎么确认「AI 选中了融合对象」

三种方式，任选：

1. **最直观**：`Slime.Debug.DrawEnemyState 1` → 选中目标的史莱姆头顶会写
   `State.Enemy.Normal.Fusing   mass 1   hp 20   fusion -> mass 1`，并且有一条绿线连到目标；
   没选中的会写 `no fusion target`。
2. **看日志**：`Slime.Debug.CombatLog 1` → 选中瞬间打印
   `[SlimeNormal_0] fusion target selected: SlimeNormal_3 (mass 1 + 1, cap 8)`。
3. **看 StateTree**：打开 StateTree Debugger 面板，选中对应实例，激活状态会停在 `Hold`
   （`SelectTarget` 成功后进 `Hold`，说明已经选到对象）。

> 注意：选到目标后**不会移动过去、也不会融合**——接近与接触判定是 Phase B（PB-09~PB-11）。
> Phase A 只做到"选中 + 打上 `State.Enemy.Normal.Fusing` 标签"。

### 辅助瞄准没反应怎么排查

按顺序查这四项：

1. `DA_RunConfig → Aim Assist Max Angle Deg` 是不是 `6`（**填 0 会静默关闭辅助瞄准**）。
2. `DT_WeaponStats → AimAssistStrength` 是不是 `0.3`（0 也会关闭）。
3. **武器 `Range`**：设计文档 6.4 给的是**有效射程 18 米**，所以 CSV 已从 `10000` 改成 `1800`。
   改过 CSV 后要在编辑器里 **Reimport** `DT_WeaponStats` 才会生效。
4. 开 `Slime.Debug.DrawAimAssist 1`：按住右键，如果黄线比白线更偏向某个敌人，就是生效了。

> 按住右键时"身体朝向相机方向"是**故意的**（TPS 横移瞄准），不是辅助瞄准——辅助瞄准只修正
> 开火射线，不动相机，这是当初选定的方案（"开火射线吸附"）。

---

## 7. CP-1 验收

在 `L_Sandbox_OnePoint` 里 PIE，控制台（`` ` ``）先输入 `Slime.Debug.CombatLog 1`。

| # | 检查 | 怎么验 | 期望 |
|---|---|---|---|
| 1 | 移动 / 相机 | WASD + 鼠标 | 6 m/s；镜头在右肩后；**空格无反应** |
| 2 | 连射 | 按住左键 | 10 发/秒，8 发后自动换弹；换弹 1s 内打不出子弹，但能移动和瞄准 |
| 3 | 命中链路 | 打一枪体量 1 史莱姆 | 日志 `ApplyDamageTo` → `took 20 damage` → `died` → `[BattleDirector] OnEnemyKilled: Kind=0 Mass=1`，目标消失 |
| 4 | 枪口遮挡 | 把枪口侧贴墙、瞄准点露在墙外 | 不开火（没有 damage 日志） |
| 5 | 辅助瞄准 | 不按右键 vs 按住右键偏一点 | 不按不吸附；按住能中；伤害仍 20、移速不变 |
| 6 | 追兵 | `SlimeSpawnAggro 1` | 4.8 m/s 追击并绕开障碍；≤1.2m 无遮挡 → 停移 + 0.5s 蓄势 → 扣血 100→80；随后 0.6s 内再中**不扣血**；两次攻击起始间隔 ≥1.6s；蓄势中跑开则落空 |
| 7 | 普通 AI | `SlimeSpawnNormal 2` | 2s 后开始 ≤3m 游走 + 0.5~1.5s 停；选中最近且体量和 ≤8 的同点位对象后，`showdebug` / 日志能看到 `State.Enemy.Normal.Fusing`，且**不融合**（融合是 Phase B） |
| 8 | 死亡 | `SlimeKillPlayer` | 玩家死亡日志 + `[BattleDirector] OnPlayerDied`；不能再移动/射击 |

调试命令一览（`USlimeCheatManager`）：

```text
SlimeSpawnNormal [Count]     # 在玩家前方生成普通史莱姆
SlimeSpawnAggro  [Count]     # 生成追兵
SlimeClearEnemies            # 清掉所有史莱姆
SlimeKillPlayer              # 走正常伤害路径杀死玩家
SlimeDamageNearestEnemy 999  # 对最近敌人造成伤害（Phase 0 就有）
SlimeDumpTables              # 打印三张表（现在含 DT_AggroStats）
```

---

## 8. 提交前别忘了

`git status` 里这些应该一起进仓库（目前 `Content/_SlimeWar/**` 的 `.uasset` / `.umap` 全都没入库）：

- `Content/_SlimeWar/**`（含 CP-0 的 `DA_RunConfig` / `DA_SpawnLayout` / `DT_*.uasset` / `L_Sandbox_OnePoint.umap`）
- `Content/_SlimeWar/Enemy/Data/DT_AggroStats.csv`
- `Docs/ProgramTaskList.md`（Phase A 状态）
- 新增的 C++ 文件与 `README.md`

> ⚠️ GitHub 不支持 `.uasset` 文件锁，改动前在群里认领，改完尽快推。
