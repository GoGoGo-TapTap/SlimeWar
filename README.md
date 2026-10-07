# SlimeWar（粘液城市）

UE 5.5 单机 TPS：玩家拿枪打史莱姆，史莱姆会两两融合变大变强，180 秒内在生成点上刷分。

> **本文是 Phase C（生成点与单点闭环）的使用说明**，面向策划与程序。
> 数值全部来自 `DA_RunConfig` / DataTable / `DA_SpawnLayout`——**调数值不需要改代码**（项目铁律 5）。
>
> 进度：Phase 0 / A / B / C 代码侧已完成并编译通过；当前工作分支 `Program-Update`。

---

## 1. 快速开始

1. 双击 `SlimeWar.uproject`（提示重新编译模块时选 Yes；首次会多编译一个 `SlimeWarEditor` 编辑器工具模块）。
2. 打开 `/Game/_SlimeWar/Maps/L_Sandbox_OnePoint`，点 Play。
3. 控制台（`~`）敲 `SlimeRunTimeScale 10`——时间轴 10 倍速，约 10 秒就能跑完一整局。

**进 PIE 前先看 Output Log**：若出现 `Spawn point N is OUT OF SYNC with DA_SpawnLayout ...`，
说明你在关卡里改了生成点却没导出，游戏会用资产里的旧数据。

---

## 2. 生成点系统

### 2.1 锚点即点位

关卡里放的 `SpawnPoint` Actor 就是点位本身：

| 锚点负责 | 数据资产负责 |
|---|---|
| 点位中心（活动圈圆心）、朝向、`Point Id` | 每个槽位的相对坐标、活动半径 |

- **移动/旋转锚点 = 移动/旋转整组槽位**，不用改任何数据。
- 槽位坐标是**锚点局部坐标**（含旋转），转锚点时整组跟着转。
- **槽位的 Z 不作数**：运行时只看 X/Y，向下打射线找地面，把史莱姆放在正下方的地面上。
  把箭头拖到空中也没关系；`Snap To Ground` 只是让视口好看。

### 2.2 批量与节奏（全部来自 `DA_RunConfig`）

局内时间轴从"玩家获得控制"起算：

| 时刻 | 事件 |
|---|---|
| t=0 | 第 1 批：每个点位 8 普通 + 2 追兵 |
| 19s / 39s / 59s / 79s / 99s | 提前 1 秒广播"下一批即将到来"（第 1 批不提示） |
| 20s / 40s / 60s / 80s / 100s | 第 2~6 批 |
| 180s | 倒计时归零 → 终局（时间到） |

每个点位总量 = 6 批 ×（8 普通 + 2 追兵）= **48 普通 + 12 追兵**，这是硬上限
（融合会减少实体数，但不会因此补发）。

### 2.3 出生位校验与备用位

每个槽位在生成前要过四关，顺序判定：

1. 落在 **NavMesh** 上（否则 AI 走不动）
2. 正下方有地面
3. 地面上方 1m 处没有静态遮挡
4. 与玩家距离 ≥ `Spawn Player Min Distance`（默认 3m）

不过关 → 依次试该点位的 **Fallback Slots** → 仍不过关则进入 **2 秒重试窗口**（每 0.25s 重试一次）
→ 超时**取消这一只，不补发**。取消日志会**逐个候选位**写清原因：
`off the navmesh` / `no ground under the slot` / `blocked by static geometry` / `too close to the player`。

### 2.4 点位状态与"已清空"

```
等待投放 AwaitingDeploy → 生成中 Spawning → 耗尽未清 DepletedNotCleared → 已清空 Cleared
```

- 第 6 批（含其延迟重试）处理完后进入"耗尽未清"。
- **只有普通史莱姆参与清空判定**：杀光本点位普通目标即 `Cleared`，**存活追兵不阻止清空**
  （它们可能已经追着玩家离开点位，也不会因原点位清空而消失）。

### 2.5 活动圈（普通史莱姆只在本点位活动）

- 半径取该点位的 `Activity Radius`；填 0 则回退 `DA_RunConfig → AI Activity Radius`（当前 600cm）。
- AI 真实受其约束：游走目标会被夹回圈内；万一漂到圈外（长距离融合寻路、被推挤），下一次决策会先走回圈内。
- 追兵**不受**活动圈限制（可以跨区追击玩家）。
- `Slime.Debug.DrawEnemyState 1` 可在视口里看到活动圈与中心。

---

## 3. 配置数据

数据入口：**Project Settings → Game → Slime War → Run Config** 指向 `DA_RunConfig`。

### 3.1 `DA_RunConfig`（`/Game/_SlimeWar/Core/Data/DA_RunConfig`）

**Run（局内流程）**

| 字段 | 说明 |
|---|---|
| Run Duration | 局内秒数（180） |
| Target Score | 过关最低分（300）；**达标不会提前结束** |
| Auto Start Run | 进关即开跑（沙盒用；投放流程接进来后关掉） |
| Deploy Duration / Result Orbit Duration | 投放演出 / 结算俯瞰时长，Phase D 使用 |

**Spawn（Phase C 生成点）**

| 字段 | 说明 |
|---|---|
| Spawn Batch Count | 每个点位的批次数（6） |
| Spawn Batch Interval | 批次间隔秒（20）；第 1 批固定 t=0 |
| Spawn Normal Per Batch | 每批普通数（8） |
| Spawn Aggro Per Batch | 每批追兵数（2） |
| Spawn Player Min Distance | 出生位与玩家的最小距离，cm（300） |
| Spawn Retry Window | 出生位不可用时的最长延迟，秒（2） |
| Spawn Retry Interval | 延迟重试的节流间隔，秒（0.25） |
| Spawn Batch Warning Lead | 下一批提前提示的秒数（1） |

> ⚠️ 这几个数的 C++ 默认值是 0；**不填会拒绝开局并报错**，不会用代码里的魔法数字兜底。

**AI | Normal（普通史莱姆）**

| 字段 | 说明 |
|---|---|
| AI Spawn Wait Time | 出生后多久开始找融合对象，秒（2） |
| AI Activity Radius | 全局活动半径，cm（600）；点位半径填 0 时用它 |
| AI Wander Radius | 找不到对象时的游走步长上限，cm（300） |
| AI Wander Pause Min / Max | 游走间隙，秒（0.5 / 1.5） |
| AI Approach Timeout | 融合接近"连续多久没继续靠近"就放弃，秒（2） |
| AI Fusion Mass Cap | 融合体量上限（8） |

**AI | Fusion（融合）**

| 字段 | 说明 |
|---|---|
| Fusion Contact Time | 持续接触多久才融合，秒（0.4） |
| Fusion Contact Tolerance | 接触容差，cm（10） |
| Fusion Post Fusion Delay | 融合后多久才能再融合，秒（1） |
| Fusion Retry Delay | 取消 / 靠近失败后多久再找对象，秒（1） |
| Fusion Max Participants | 参与融合的个体数（2） |

**Player / Camera / Weapon / Data**：玩家生命与移动、TPS 相机与瞄准辅助、三张表与默认武器 Id（`Rifle`）。

### 3.2 `DA_SpawnLayout`（`/Game/_SlimeWar/Core/Data/DA_SpawnLayout`）

> 正常**不要手填**——用第 4 节的生成点编辑器摆位后导出。

每个点位一条 `FSlimeSpawnPointDef`：

| 字段 | 说明 |
|---|---|
| Point Id | 必须与关卡里锚点的 `Point Id` 一致 |
| Activity Radius | 该点位活动半径，cm；0 = 用全局值 |
| Normal Slots | 8 条，每条 `RelativeLocation` + `RelativeRotation` |
| Aggro Slots | 2 条 |
| Fallback Slots | 备用位若干（3~4 条比较稳） |

### 3.3 数值表

| 表 | 路径 | 内容 |
|---|---|---|
| `DT_SlimeStats` | `Content/_SlimeWar/Core/Data/` | 体量 1~8 的生命 / 速度 / 半径 / **分值**（同目录有 CSV，可在编辑器里 Reimport） |
| `DT_WeaponStats` | 同上 | 武器伤害 / 射速 / 弹匣 / 换弹 / 射程 / 瞄准辅助 |
| `DT_AggroStats` | `Content/_SlimeWar/Enemy/Data/` | 追兵生命 / 速度 / 攻击范围 / 伤害 / 前摇 / 收势 / 冷却 |

**当前值**：体量 1~8 生命 20~160、**分值 10~80（= 10 × 体量）**；追兵 60 血 / 4.8 m/s / 单次攻击 20 伤害；
步枪 20 伤害 / 10 发每秒 / 8 发弹匣 / 1.0s 换弹 / 射程 18m。

---

## 4. 生成点编辑器

有了它，**不用打开数据资产手填坐标**：在视口里直接摆，再一键写回。

### 4.1 摆一个点位

1. Place Actors → 搜 `SpawnPoint` → 拖到想要的位置；Details 里设 `Point Id`（每个点位唯一）。
2. 选中锚点 → Details 出现 **Spawn Point Editor** 分类：
   - **Create Default Slots**：按 `DA_RunConfig` 的每批数量生成 8 普通（绿）+ 2 追兵（红）+ 3 备用（黄）；
   - 在视口里**直接拖拽 / 旋转**每个箭头（箭头不能被框选，请单击选中，或在 World Outliner 里选）；
   - 选中单个手柄后可在它自己的 Details 里改 `Role`（改角色会换颜色）、`Slot Index`，或 **Remove Slot**；
   - 想加就点 `+ Normal` / `+ Aggro` / `+ Fallback`。
3. **Validate Layout**：在 Output Log 里逐个槽位打印 `OK / BAD` 与原因，**修到没有 BAD**。
4. **Export To Data Asset**：把本点位写进 `DA_SpawnLayout`；面板顶部状态会从 `OUT OF SYNC` 变成 `in sync`。
5. 保存关卡与数据资产（Save All）。

其他按钮：`Snap To Ground`（把手柄 Z 贴到地面，纯视觉）、`Import From Data Asset`（用资产数据重建手柄）、
`Export All Points In Level`（多点位一起导出，Phase D 用）。

### 4.2 视口里能看到什么

- 青色大箭头 = 锚点（点位中心与朝向）。选中它或其任一槽位，会画出**活动圈**、锚点到各槽位的连线，
  以及 HUD 标签（含 `Point Id` 与同步状态）。
- 绿 / 红 / 黄小箭头 = 普通 / 追兵 / 备用槽位；箭头方向就是该槽位的出生朝向
  （史莱姆一开始移动就会转向移动方向）。

### 4.3 数据流向（重要）

- **导出**：视口手柄 → `Export To Data Asset` → 写进 `DA_SpawnLayout`。
- **运行时**：生成点**只读 `DA_SpawnLayout`**；视口手柄是编辑期代理，打包时被剥离，运行时零成本。
- **反向**：`Import From Data Asset` 用资产数据重建手柄（换机器、误删手柄时用）。
- 改完手柄**必须导出**，否则游戏用旧数据；忘了导出的话，进 Play 前会收到 `OUT OF SYNC` 警告。

---

## 5. 局内规则速查

### 5.1 计分

- **只有普通史莱姆计分**，分值 = `DT_SlimeStats` 的 `KillScore`（当前 10 × 体量）；追兵击杀**永远 0 分**。
- 融合**不计分**，只做统计与表现；融合体按"剩余生命比例继承"。
- 达标（300 分）只表示"过关"，**不会提前结束**。
- **最佳分**只在"时间到且分数 ≥ 目标分"时刷新；玩家死亡或未达标**不会**覆盖它。
  最佳分存在内存里（GameInstance），重试保留、不写盘。

### 5.2 终局

只有两种情况结束：**玩家生命耗尽** 或 **180 秒倒计时归零**。
结束后立即停止生成、禁止新增得分；敌人与玩家的冻结、结算俯瞰属于 Phase D。

### 5.3 结算会用到的数据

实际得分、最低目标、普通击杀数、清空点位数、是否过关——`ASlimeRunGameState` 已全部暴露，UI 直接绑。

---

## 6. 调试：HUD、CVar 与控制台命令

### 6.1 调试 HUD

`ASlimeHUD` 全部是 C++ 绘制，不需要任何 UMG 资产：

| CVar | 默认 | 作用 |
|---|---|---|
| `Slime.Debug.DrawRun` | 1 | **Phase C 运行 HUD**：总分 / 目标 / 最佳分、倒计时、点位状态、每点位已生成 / 存活 / 待生成、场上存活与峰值 |
| `Slime.Debug.Crosshair` | 1 | 准星占位 |
| `Slime.Debug.DrawEnemyState` | 0 | 敌人状态标签、**活动圈与中心** |
| `Slime.Debug.DrawFusion` | 0 | 融合配对、接触进度、会合点、冷却 |
| `Slime.Debug.DrawAggroPath` | 1 | 追兵的计划路径（青）与真实轨迹（橙） |
| `Slime.Debug.DrawAimAssist` | 0 | 相机射线 / 辅助射线 / 吸附目标 |
| `Slime.Debug.CombatLog` | 0 | 每次伤害与死亡的详细日志 |
| `Slime.Run.TimeScale` | 1 | **时间轴倍速**：`10` 快进、`0` 冻结（只影响时间轴，不改任何数值） |

### 6.2 控制台命令

| 命令 | 作用 |
|---|---|
| `SlimeRunStart` | 立刻开始（或重开）本局 |
| `SlimeRunEnd` | 以"时间到"结束本局 |
| `SlimeRunTimeScale 10` | 时间轴 10 倍速 |
| `SlimeRunStatus` | **CP-3 全套数字**：每体量击杀明细与得分、总分与 per-mass 合计、目标分、最佳分、击杀数、清空点数、融合次数、存活 / 峰值，以及每个点位的状态与计数 |
| `SlimeKillPlayer` | 走真实伤害链杀死玩家（验证死亡终局） |
| `SlimeClearEnemies` | 清空场上敌人（配合检查清空判定） |
| `SlimeSpawnNormal [N]` / `SlimeSpawnAggro [N]` | 在玩家面前生成 N 只（调试用） |
| `SlimeSpawnNormalAtMass [Mass] [N]` | 生成指定体量的普通史莱姆（测融合） |
| `SlimeSetMass [Mass]` | 把最近的普通史莱姆改成指定体量 |
| `SlimeForceFuse` | 强制最近的同点位两只融合（测取消 / 打断） |
| `SlimeDamageNearestEnemy [Amount]` | 对最近的敌人走一次统一伤害入口 |
| `SlimeDumpTables` / `SlimeReloadTables` | 打印 / 重载数据表（改完 CSV 或 DA 后使用） |

### 6.3 排查手册

| 现象 | 原因与处理 |
|---|---|
| 一个史莱姆都不出 | ① `DA_RunConfig` 的 Spawn 字段没填（日志会报错）② 关卡里没有 `SpawnPoint` 锚点（`no ASpawnPoint anchor was found`）③ 锚点 `Point Id` 在 `DA_SpawnLayout` 里没有对应定义（日志会点名） |
| 日志 `blocked by static geometry` | 该候选位地面上方 1m 有静态物。日志会逐个候选位给出原因与坐标，据此挪槽位或补 Fallback Slots |
| 日志 `off the navmesh` | 槽位不在导航网格上：补 `NavMeshBoundsVolume` 或挪位置 |
| 日志 `too close to the player` | 出生位离玩家太近（默认 3m）：把点位挪远，或调 `Spawn Player Min Distance` |
| 进 Play 前 `OUT OF SYNC` | 视口手柄与数据资产不一致：选中锚点 → `Export To Data Asset` → Save All |
| 改了数值没生效 | 数据表用 `SlimeReloadTables`；`DA_*` 改完保存即可。代码里没有数值可改（都在数据里） |
| 史莱姆半埋 / 卡住 | 运行时按"地面 + 胶囊半高"生成，不应再出现；若复现请附 `SlimeRunStatus` 与日志 |

---

## 7. 面向程序

### 7.1 模块与目录

| 模块 | 类型 | 内容 |
|---|---|---|
| `SlimeWar` | Runtime | 游戏全部逻辑；`Public/Flow/` 是 Phase C 的流程与生成点 |
| `SlimeWarEditor` | Editor | **只有编辑器工具**：生成点编辑器（视口可视化 + 详情面板 + DA 往返 + PIE 前检查）；打包不参与 |

代码分层：`L0 Core`（类型 / 接口 / 纯函数）→ `L1 GameplayFramework` → `L2 Player / Enemy` → `L3 Flow` → `L4 UI` → `L5 Debug`。
只允许依赖层号更小的模块；`Public/Core/*` 不出现任何 GAS 类型（红线 C18）。

### 7.2 Phase C 的关键类

| 类 | 职责 |
|---|---|
| `ASpawnPoint` | 锚点即点位：批次执行、延迟重试、存活计数、四状态 |
| `URunSubsystem` | 时间轴权威：倒计时、批次、提前提示、终局；订阅 Director 事件 |
| `UScoreSubsystem` | GameInstance 级：总分、按体量明细、击杀数、清空点数、最佳分 |
| `ASlimeRunGameState` | 分数 / 时间 / 点位状态的**只读镜像**，UI 唯一绑定面 |
| `USlimeEnemyManagerSubsystem` | 唯一生成入口 + 存活 / 峰值统计 |
| `SlimeSlotResolver` | 出生位解析（NavMesh → 地面 → 遮挡 → 玩家距离）与生成 Z；**运行时与编辑器共用** |
| `SlimeActivityArea` | 活动圈的"是否在内 / 夹回"纯函数；AI 与 HUD 共用 |
| `SlimeSpawnLayoutEdit` | 手柄 → 三数组的纯函数（排序 / 往返 / 重复检测） |
| `USlimeSpawnSlotMarker` / `USlimeSpawnPointAnchor` | 编辑期手柄（`bIsEditorOnly`，运行时被剥离） |

### 7.3 跨模块契约（不要随手改）

- `IBattleDirector` 是**唯一的跨模块通信口**：敌人与武器不认识计分系统。GameMode 只做广播，Flow 侧订阅。
- **两条承伤路径**收在 `USlimeCombatSubsystem`：目标有 ASC（玩家）走 `GE_Damage`，没有（敌人）走 `HealthComponent`；
  对外只有一个 `OnDeath`。
- 改 `Public/Core/*` 或跨模块接口要单独提交并登记（计划 §7.4）。

### 7.4 自动化测试

编辑器里：`Tools → Session Frontend → Automation`，过滤 `SlimeWar.`（当前 8 个，全绿）。

```
SlimeWar.Flow.RunSchedule / ScoringRules / PointState / SpawnSupply
SlimeWar.Spawn.LayoutArrays / DuplicateSlots / AnchorSpace
SlimeWar.Enemy.ActivityArea
```

也可以不开编辑器直接跑：

```powershell
UnrealEditor-Cmd.exe "<...>\SlimeWar.uproject" -ExecCmds="Automation RunTests SlimeWar" -unattended -nopause -nullrhi -testexit="Automation Test Queue Empty"
```

---

## 8. 文档索引

| 文档 | 内容 |
|---|---|
| `Docs/ProgramTaskList.md` | 程序任务清单与模块框架（各 Phase 实现注记、契约表、变更记录） |
| `Docs/PhaseC/PhaseC-Plan.md` | Phase C 设计与决策（v1.1：锚点即点位 + 生成点编辑器） |
| `Docs/PhaseC/PhaseC-Checklist.md` | Phase C 编辑器步骤与 CP-3 实机验收清单 |
| `Docs/PhaseB/PhaseB-Plan.md` / `PhaseB-Checklist.md` | 融合（PB-09~PB-21）设计与验收 |
| `Docs/PhaseA/PhaseA-Checklist.md` | 玩家主链路与敌人 AI 的编辑器步骤 |
| `Docs/GameDesign/DesignDoc.md` | 策划案（规则与体验的唯一来源） |
| `Docs/Collaboration.md` | 版本库与协作规范（Git LFS 等） |

> 待确认数值见 `Docs/ProgramTaskList.md` §8（Q1~Q14）：体量表、玩家生命 / 速度、弹匣 / 射速 / 伤害等仍是占位值，
> Phase A/B 验收前需要策划定稿。
