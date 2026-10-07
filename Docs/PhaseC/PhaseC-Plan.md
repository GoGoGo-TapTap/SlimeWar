# Phase C 实现计划 —— 生成点与单点闭环

> 状态：**代码侧已落地并编译通过**（`SlimeWarEditor Win64 Development` 与 `SlimeWar Win64 Development`，0 error）。
> 剩下的是编辑器资产与 CP-3 实机验收，见 [PhaseC-Checklist.md](PhaseC-Checklist.md)。
> 数值全部来自 `DA_RunConfig` / `DA_SpawnLayout`，代码与蓝图里不写数字（铁律 5）。
>
> **v1.1（2026-10-07）修订**：锚点即点位 + 槽位改为锚点局部坐标 + 新增生成点编辑器（Editor 模块）。
> 本次修订同时修掉了 v1.0 实机暴露的出生位解析缺陷，详见 §3.3 与 §6。

---

## 1. 范围与依赖

| 任务 | 内容 | 状态 |
|---|---|---|
| PC-01 | `ASpawnPoint`：出生位、活动区、备用位 | 代码完成 |
| PC-02 | 批次 0/20/40/60/80/100s，6 批 ×（8 普通 + 2 攻击） | 代码完成 |
| PC-03 | 出生位校验 + 备用位 + 逐只 2s 延迟重试 + 超时取消不补发 | 代码完成 |
| PC-04 | 后续批次提前 1s 提示（委托先出，UI 后挂） | 代码完成 |
| PC-05 | 点位四状态机；清空只看普通目标 | 代码完成 |
| PC-06 | `URunSubsystem`：开局、180s 倒计时、驱动批次、最小终局 | 代码完成 |
| PC-07 | `UScoreSubsystem`：按体量计分、总分、通关最佳分 | 代码完成 |
| PC-08 | **口径变更**：不做对象池，改轻量管理 + 存活/峰值统计 | 代码完成 |
| PC-09 | `ASlimeRunGameState`：分数/时间/点位状态只读镜像 | 代码完成 |
| PC-10 | **生成点编辑器**（Editor 模块：视口手柄 + DA 往返 + 校验 + PIE 前一致性检查） | 代码完成 |
| PA-13 | 调试 HUD 显示分数/倒计时/点位状态/存活数 | 代码完成 |
| PA-12 | 计分 UI + 图标飞入动画 | **延后到 Phase D/E** |
| CP-3 | 单点 6 批闭环，总分与手算一致 | 待实机验收 |

依赖：Phase A 的 StateTree AI 与 `USlimeCombatSubsystem`、Phase B 的融合与 `OnEnemyDied`。

## 2. 已确认的设计决策

1. **锚点即点位**：`ASpawnPoint` 的 Transform 就是点位中心与活动圈圆心；移动/旋转锚点 = 移动/旋转整组槽位，不用改数据。
2. **槽位是锚点局部坐标**：`FSlimeSpawnSlot` 只存 `RelativeLocation` + `RelativeRotation`（含旋转参考系）；DA 里不再有 `Center`。
3. **槽位 Z 不作数**：运行时只信 XY，沿 Z 向下打射线找地面，生成点 = 地面 + 胶囊半高（从生成类 CDO 读）。把箭头拖到空中也能落在正下方的地上。
4. **DA 仍是运行时唯一数据源**：视口里的槽位手柄（`USlimeSpawnSlotMarker`）是**编辑期镜像**，`bIsEditorOnly`，打包被剥离，运行时零成本。
5. **编辑器工具放独立 Editor 模块**（`SlimeWarEditor`，Type=Editor）：运行时模块不含任何编辑器代码；将来升级成 ComponentVisualizer 拖拽手柄（B）或 `UEdMode`（C）只动这个模块。
6. **同步方式 = 手动按钮 + PIE 前不一致警告**：不自动改资产；进 Play 前若"关卡手柄 ≠ 资产定义"就指名点位报警。
7. **活动半径留在 DA（0 = 回退 `AIActivityRadius`），并由 AI 强制执行**——数值不进 `.umap`。
   生成时把该点位的**有效半径**交给 `ASlimeEnemyBase::InitializeFromSpawn`；`Slime: Wander Step` 用
   `SlimeActivityArea`（Core 纯函数，与编辑器画圈同一份规则）把随机游走目标夹回活动圈，若已经漂到圈外
   则先走回最近的圈内点。设计 5.1「普通目标只在本点位活动」由此从"只是画出来的圈"变成真实约束。
8. **点位数据在 DA、地图只放锚点**（§7.3）；生成位解析只有一份实现（`SlimeSlotResolver`），运行时与编辑器工具共用，避免两边规则漂移。
9. **不做 Actor 对象池**：全局 180 只的成本来自同时在场，池化不降低它；改为 `USlimeEnemyManagerSubsystem` 统一生成 + 统计。
10. **GameMode 只广播**：Flow 侧订阅 4 个委托，GameMode 不 include 任何 Flow 头（守住 L1 不依赖 L3）。
11. **最佳分跨重试保留**：存 GameInstance，不落盘（符合策划案"不做存档进度"）。
12. **终局只做"停生成 + 锁计分 + 广播"**：敌人/玩家冻结与结算演出属 Phase D。

## 3. 类与职责

| 类 | 模块 | 职责 |
|---|---|---|
| `ASpawnPoint` | Runtime（Flow） | 锚点 + 一个点位的批次执行、延迟重试、存活计数与四状态 |
| `USlimeSpawnSlotMarker` | Runtime（Flow） | **编辑期**槽位手柄（箭头）+ `Role` / `SlotIndex`；运行时只被剥离 |
| `USlimeSpawnPointAnchor` | Runtime（Flow） | **编辑期**点位图标；给编辑器可视化一个挂点 |
| `SlimeSlotResolver` | Runtime（Public/Flow） | 出生位解析（NavMesh → 地面 → 遮挡 → 玩家距离）、生成 Z——运行时与编辑器共用 |
| `SlimeSpawnLayoutEdit` | Runtime（Public/Flow） | 纯函数：`(Role, Index)` → 三个数组、往返展平、重复位置检测 |
| `SlimeActivityArea` | Runtime（Public/Core） | 纯函数：活动圈的"是否在内 / 夹回圈内"——敌人 AI 与调试 HUD 共用 |
| `URunSubsystem` | Runtime（Flow） | 时间轴（倒计时/批次/提前提示）、订阅 Director 事件、终局 |
| `UScoreSubsystem` | Runtime（Flow） | 按体量计分、按体量明细、总分、击杀数、清空点数、通关最佳分 |
| `ASlimeRunGameState` | Runtime（Flow） | 只读镜像 + UI 绑定委托 |
| `USlimeEnemyManagerSubsystem` | Runtime（Flow） | 唯一生成入口、存活/峰值统计、清场 |
| `FSlimeSpawnPointVisualizer` | Editor | 视口画活动圈、锚点→槽位连线、槽位点、HUD 标签（含同步状态） |
| `FSlimeSpawnPointDetails` | Editor | 工具按钮：补齐默认槽位 / 增删槽位 / 吸附地面 / 校验 / 导出 / 导入 + 同步状态行 |
| `FSlimeSpawnSlotMarkerDetails` | Editor | 单个手柄的「Remove Slot」与按角色改色 |
| `FSlimeWarEditorModule` | Editor | 注册可视化与详情面板；PIE 前一致性检查 |

### 3.3 出生位解析（`SlimeSlotResolver::Resolve`）

1. NavMesh 投影（范围 ±200cm）失败 → `NoNavMesh`
2. 由 `Desired + 300cm` 向下打到 `-1000cm` 的 `ECC_WorldStatic` 射线，取命中点；
   未命中 → `NoGround`
3. 命中点上方 100cm、半径 42cm 的球体与 `ECC_WorldStatic` 重叠 → `Blocked`
4. 与玩家 2D 距离 < `SpawnPlayerMinDistance` → `TooCloseToPlayer`
5. 全部通过 → 生成点 = `(命中X, 命中Y, 命中Z + 胶囊半高 + 2cm)`

候选顺序：首选位 → `FallbackSlots`（依次）；全部失败进入 `SpawnRetryWindow` 重试窗口，超时取消且**不补发**。
首次解析失败会**逐候选位**打印原因（角色/槽位号/期望世界坐标/地面 Z/与玩家距离），取消时再打一条汇总。

> v1.0 的三个缺陷就是在这里修掉的：探针锚在"输入 Z"上（槽位 Z 低于地面时探针扎进地板 → 必然 blocked）、
> 生成点没吸附地面也没加胶囊半高（半埋）、`FallbackSlots` 里未填的 `(0,0,0)` 被当成合法坐标
> 且它的失败原因盖住了首选位的原因。

## 4. 生成点编辑器（PC-10，Editor 模块）

- **视口**：锚点图标（青色箭头）与槽位手柄（普通绿 / 追兵红 / 备用黄）都是可选中、可用原生操纵器拖拽旋转的子组件；选中任一个即可看到活动圈、锚点→槽位连线与 HUD 标签（标签含 `in sync` / `OUT OF SYNC` 状态）。
- **工具按钮**（选中 `ASpawnPoint` 时的 Details 面板「Spawn Point Editor」分类）：
  `Create Default Slots`（按 DA_RunConfig 的每批数量补 8 普通 + 2 追兵 + 3 备用，环形摆放）、
  `+ Normal / + Aggro / + Fallback`、`Snap To Ground`、`Validate Layout`、
  `Export To Data Asset`、`Import From Data Asset`、`Export All Points In Level`；
  面板顶部显示"关卡手柄 vs 资产定义"的同步状态。
- **删除**：选中某个手柄 → 它的 Details 面板里有 `Remove Slot`（运行时不加任何编辑器函数）。
- **DA 查找**：`Project Settings → Game → Slime War → Run Config` → `USlimeRunConfig::SpawnLayout`。
- **导出语义**：按 `PointId` **更新或插入**，绝不删除其他点位的定义；`ActivityRadius` 原样保留。
- **PIE 前检查**：`FEditorDelegates::PreBeginPIE` 逐个点位比对，不一致就用 `Warning` 指名点位与数量差，**不自动写入**。

## 5. 需要填的数据

`DA_RunConfig`（`Spawn` 分类，C++ 默认全 0，**不填则拒绝开局**）：
`SpawnBatchCount=6`、`SpawnBatchInterval=20`、`SpawnNormalPerBatch=8`、`SpawnAggroPerBatch=2`、
`SpawnPlayerMinDistance=300`、`SpawnRetryWindow=2`、`SpawnRetryInterval=0.25`、`SpawnBatchWarningLead=1`；
确认 `RunDuration=180`、`TargetScore=300`、`bAutoStartRun=true`。

`DA_SpawnLayout`：每个点位一条 `FSlimeSpawnPointDef`（`PointId`、`ActivityRadius`、`NormalSlots`、`AggroSlots`、`FallbackSlots`）。
**不再手填坐标** —— 用生成点编辑器摆位后 `Export To Data Asset` 写回。

> ⚠️ **v1.0 的旧数据已失效**：`Center` 字段被移除、槽位从绝对坐标改为锚点局部坐标，
> 旧的 8/2 个坐标不再有正确含义，需要用工具重摆一次（锚点世界位置可以沿用现在这一处）。

## 6. 与任务清单的偏差（已登记）

1. **PC-08 不做对象池** → 改为 `USlimeEnemyManagerSubsystem` 轻量管理。
2. **PA-12 延后** → Phase C 只做调试 HUD（PA-13）。
3. **终局只做"停生成 + 停计分"** → 冻结与结算演出留 Phase D。
4. **PB-19~21 不并入本 Phase** → 单开补丁窗口。
5. **契约变更 C11（两次）**：`SpawnSlots` → `Points`（v1.0）；`Center` 移除 + `Location` → `RelativeLocation`（v1.1）。
6. **新增 PC-10**：编辑器工具，原计划没有（v1.1 新增）。

## 7. 自动化测试

`Private/Tests/SlimeFlowMathTests.cpp`（v1.0）：
`RunSchedule` / `ScoringRules` / `PointState` / `SpawnSupply`。

`Private/Tests/SlimeSpawnLayoutTests.cpp`（v1.1）：

* `SlimeWar.Spawn.LayoutArrays` —— 乱序 + 缺号的 `(Role, Index)` 正确归入三个数组并按 Index 排序；展平后再构建可往返。
* `SlimeWar.Spawn.DuplicateSlots` —— 同 XY 判定（Z 差异不算区别；堆在锚点的多个槽位会被标记）。
* `SlimeWar.Spawn.AnchorSpace` —— 锚点平移/旋转后槽位世界坐标随之变化；`GetSpawnZ` 的高度计算。
* `SlimeWar.Enemy.ActivityArea` —— 活动圈判定（高度不参与）、把圈外坐标夹回边缘、未设置半径/圆心时不约束。
