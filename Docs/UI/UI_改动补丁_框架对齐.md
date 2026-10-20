# UI 改动补丁：框架对齐

- 适用日期：2026-10-20
- 用途：把 `ResultPage_Implementation_Plan.md` 与 `UI_Settings_Implementation.md` 里**已经过时**的部分，按当前代码改成可执行版本
- 用法：这是**补丁**，不是重写。老文档保留原编号，本文按“替换 §X.Y / 新增 §Z”的方式给出，执行时两份对照看
- 执行入口：勾选与步骤看 **`Docs/UI/UI执行清单_代码与WBP.md`**（程序侧 U-C1~U-C7、WBP 侧 U-W1~U-W8、验收 20 条）；本文只回答“为什么改、改成什么”
- 依据代码：`ScoreSubsystem`、`RunSubsystem`、`SlimeWarCoreTypes.h`、`SlimeUISubsystem.cpp`、`DefaultGame.ini`、`SlimeWeaponRules`

---

## 0. 为什么需要这份补丁

生态重做（单一 `ASlimeEcologyEnemy`、无 StateTree、无 A/B 分队）与关卡数据化之后，两份老计划里有四类内容已经对不上：

| 类型 | 例子 | 后果 |
|---|---|---|
| 数据字段失效 | `Result.ClearedPoints` 恒为 0 | WBP 上永久显示 0，被当成 bug |
| 枚举不全 | `EndReason` 只有 TimeUp/PlayerDied | 生态灭绝时无文案分支 |
| 关卡假设过时 | “现在只配了一关 DA_Level_CityPlaza”、目标分 300 | 验收表全部对不上 |
| 缺新需求 | `WBP_AimPreview`、`M_HurtIndicator_PP`、特殊子弹 UI | 这些资产已做但没有任何程序挂点 |

---

## A. ResultPage 计划补丁

### A.1 替换 §2.1 的数据表（清点 → `HostileKills`）

老 §2.1 那一行 `清点数 | Result.ClearedPoints | 出生点清空数，0~3` **作废**。原因是硬事实：`UScoreSubsystem::AddClearedPoint()` 全工程**没有任何调用者**（只有定义），生态重做后站点不会“清空”，这个计数器失去数据源。

替换为：

| 界面元素 | 字段 | 说明 |
|---|---|---|
| 最终得分 | `Result.Score` | 已锁分，最终值 |
| 最高分 | `Result.BestScore` | 见 A.2 的 Q-03 |
| 目标分 | `Result.TargetScore` | **不要写死**；双点 200、三点 160 |
| **核心击杀** | `Result.HostileKills` | **替换掉“清点数”**，敌对核心击杀数 |
| 击杀数 | `Result.NormalKills` | 玩家击杀总数（含敌对核心），与 `PlayerKills` 同值 |
| 非玩家死亡（可选） | `Result.NpcKills` | 内斗/爆裂/生态自灭，策划要才显示 |
| 达标 / 未达标 | `Result.bPassed` | 只有 `TimeUp && Score >= Target` 为 true，死亡一律 false |
| 结束原因 | `Result.EndReason` | `TimeUp` / `PlayerDied` / **`Extinction`（新增）** |
| 存活时长（可选） | `Result.ElapsedTime` | 秒 |
| 承受伤害（可选） | `Result.PlayerDamageTaken` | 已扣除无敌窗口 |

> `EndReason` 的定义在 `SlimeWarCoreTypes.h:107-113`，`bPassed` 的判定在 `SlimeFlowMath.h:64-67`。**生态灭绝必然是未达标**，因为 `ComputeRunPassed` 只认 `TimeUp`；文案上建议单独写“生态被清空”，不要和“时间到未达标”共用一句。

### A.2 收掉 §0.4 的三个待确认（Q-01 / Q-02 / Q-03）

| 编号 | 老问题 | 结论 | 依据 |
|---|---|---|---|
| Q-01 | “清点数”是出生点还是史莱姆数 | **两者都不是：该字段当前无数据源，恒为 0** | `AddClearedPoint()` 无调用者 |
| Q-02 | “击杀总数”是否含追兵 Aggressive | **问题失效**。追兵概念已随旧分队系统删除；`NormalKills` = 玩家击杀总数，`NpcKills` = 非玩家致死 | `ScoreSubsystem.cpp:46-78` |
| Q-03 | 最高分是否显示 `Max(Score, BestScore)` | 仍按老结论：只在 `TimeUp` 达标时刷新。**建议**在未达标时加一行小字“本局不计入最高分”，比改数值诚实 | `SlimeFlowMath.h:80-83` |

### A.3 替换 §5.1 / A-01 / A-03：关卡与资产现状

`Config/DefaultGame.ini` 现在是 **3 个槽位**（用户在 Project Settings 里已重排）：

| 槽位 | LevelId | 地图 | 性质 |
|---|---|---|---|
| 0 | `DoublePoint` | `L_Ecology_DoublePoint` | 可玩（150s / 目标 200） |
| 1 | `ThreePoint` | `L_Ecology_ThreePoint` | 可玩（180s / 目标 160） |
| 2 | `CityPlaza` | `L_Whitebox_CityPlaza` | **旧白盒，回归/调试用，不是正式关** |

因此：

- A-01“在 `L_Whitebox_CityPlaza` 摆拍照相机”改为：**双点、三点各摆一台** `ASlimeResultCamera`。白盒图如果要保留回归，也摆一台。
- A-03“第二关的 `DA_Level_*` 与 `Levels` 配置”已**完成**，可勾掉；但见 A.6 的坑。
- 老文档里“现在只配了一关，所以下一关永远是灰的”**作废**：现在有两关，`Btn_NextLevel` 在双点局是可点的。

### A.4 修改 §1.1 的“照片”决策

老写法：“机位对齐 `LS_Result` 末帧”。这条现在不成立，因为 `LS_Result` 只有**一条全局序列**，而两张生态图的机位完全不同（双点 30×44m、三点 50×60m），一条序列不可能同时对上。

替换为：

| 决策 | 新结论 |
|---|---|
| 照片来源 | 关卡里的静态 `ASlimeResultCamera`，**机位就是照片本身** |
| 与 `LS_Result` 的关系 | 解耦。`LS_Result` 只负责结算运镜，不参与构图；相机不再“对齐末帧” |
| 每关的 `ResultSequence` | 建议每关各做一条（`LS_Result_DoublePoint` / `LS_Result_ThreePoint`），填在 `DA_Level_*::ResultSequence`；留空则回退全局，双点勉强能用、三点会穿帮 |
| `ResultOrbitDuration` | 必须与所属序列长度一致（`SlimePresentationDirector::WarnIfLengthDiffers` 会打 Warning） |

### A.5 替换 §6.2 的进度条采样表

`Target` 不再是 300，按关卡取：

| Score | Target（双点 200） | Blue | Yellow |
|---|---|---|---|
| 0 | 200 | 0% | 0% |
| 100 | 200 | 50% | 0% |
| 200 | 200 | 100% | 0% |
| 275 | 200 | 100% | 50% |
| 350 | 200 | 100% | 100% |
| 500 | 200 | 100% | 100%（封顶） |

| Score | Target（三点 160） | Blue | Yellow |
|---|---|---|---|
| 0 | 160 | 0% | 0% |
| 80 | 160 | 50% | 0% |
| 160 | 160 | 100% | 0% |
| 235 | 160 | 100% | 50% |
| 310 | 160 | 100% | 100% |

公式不变：

```text
Blue   = (Target > 0) ? Clamp(Score / Target, 0, 1) : 0
Yellow = Clamp((Score - Target) / 150.0, 0, 1)
```

> 黄段的 150 仍是硬编码（老计划的已知项）。现在两关的目标分不同，这个 150 的语义更模糊了，建议排期把它挪进 `USlimeRunConfig::ScoreBarOvershoot`（老文档 §10 的优化 1，现在优先级提高了）。

### A.6 新增：`HasNextLevel()` 必须跳过不可用槽

因为 slot2 是旧白盒（`DA_Level_CityPlaza` 有 `MapAsset`，所以按老实现 `HasNextLevel()` 会返回 **true**），从三点局点“下一关”会进白盒图。

三种处理，选一种：

| 方案 | 做法 | 评价 |
|---|---|---|
| A（推荐） | 从 `DefaultGame.ini` 删掉 slot2 的 `+Levels=` 行 | 最干净；白盒回归改用 PIE 直接开图 + `bSkipMainMenuInPIE` |
| B | `HasNextLevel()` 里跳过 `LevelId == "CityPlaza"` | 引入了关卡名硬编码，不推荐但可救急 |
| C | 在 `USlimeLevelDefinition` 加 `bPlayable` 字段，`HasNextLevel` 与选关页都读它 | 最正规，代价是加字段 + 两个数据资产勾选 |

无论选哪个，`RequestNextLevel()` 都要沿用老计划的两条：**清空落点**（`ClearSelectedDropPoint()`）、**进准备页而不是直接开局**。

### A.7 C++ 清单更新（老 §0.1）

| 编号 | 事项 | 状态变化 |
|---|---|---|
| C-01 | 新增 `ASlimeResultCamera` | 仍要做；**每关一台** |
| C-02 | `GetResultPhoto()` | 仍要做 |
| C-03 | `OnResultShown`（阶段=Ended 才触发） | 仍要做，且是结算音的前置依赖 |
| C-04 | 结算页 `HasNextLevel/RequestNextLevel/RequestLevelSelect` | 仍要做，注意 A.6 |
| C-05 | UI 子系统同名转发 | 仍要做 |
| C-06 | `USlimeGameSettings::IndexOfLevel()` | 仍要做 |
| **C-07（原“新”）** | ~~`USlimeGameSettings` 增 `AudioConfig` 软引用（指向 `DA_SlimeAudioConfig`）~~ **已取消（2026-10-20）** | 音频 C++ 层整体删除，DA 与字段都不存在了；音效改为蓝图直连，不需要全局配置项 |
| **C-08（新）** | `ASlimeResultCamera` 支持“每关多个候选机位”时的 `PhotoId` | 可选，只有一关要多机位才做 |

### A.8 替换 §7 的验收表

| # | 场景 | 操作 | 期望 |
|---|---|---|---|
| 1 | 双点 0 分 | `SlimeRunSetTime 1` | 0 分、蓝 0%、黄 0%、未达标印章、最高分不变 |
| 2 | 双点刚好达标 | `SlimeRunAddScore 200` → `SlimeRunSetTime 1` | 蓝 100%、黄 0%、达标印章 |
| 3 | 双点超出 75 | `SlimeRunAddScore 275` | 蓝 100%、黄 50% |
| 4 | 三点刚好达标 | 进三点关，`SlimeRunAddScore 160` → 时间到 | 蓝 100%、黄 0% |
| 5 | 超分但死亡 | `SlimeRunAddScore 400` → `SlimeKillPlayer` | 未达标印章；最高分不变 |
| 6 | 生态灭绝 | 清空所有史莱姆直到 `Extinction` | 未达标；文案走“生态被清空”分支，不是“时间到” |
| 7 | 核心击杀 | 击杀带敌对核心的史莱姆后结算 | “核心击杀”计数 +1，不是“清点数” |
| 8 | 照片 | 双点/三点各结一局 | 两张照片构图分别对应各自关卡，不是同一张 |
| 9 | 没放相机 | 临时删掉其中一台 | 该关回退 `T_Result_BG`，另一关不受影响 |
| 10 | 下一关（双点→三点） | 双点结算点“下一关” | 进三点**准备页**，落点回到未选择 |
| 11 | 下一关（三点） | 三点结算看按钮 | 按 A.6 的方案：灰掉（A）或进白盒（未处理时） |
| 12 | 选择关卡 | 结算页点“选择关卡” | 切到选关页，无黑屏重载 |
| 13 | 重试残留 | 连点两次“重新开始” | 照片、数字、音效都不残留 |
| 14 | 入场动画/音 | 进关卡后立刻结算 | 只在结算页出现时播一次 |

---

## B. Settings 计划补丁

### B.1 两条状态记录（§0.1 的 S-11 / §0.2 的 R-11）

| 编号 | 老状态 | 新状态 | 理由 |
|---|---|---|---|
| S-11 声音 | `[!]` 页面上没有可操作的音量控件，滑块置灰 + 断线 | `[~]` 滑块已恢复可操作、数值能存盘，但**还没推到 Wwise** | `SetMasterVolume` 随音频 C++ 一起删了，需要在蓝图 `On Settings Values Changed` 里推 `Set RTPC Value`（《UI音频接入方案》§4.1） |
| R-11 音量滑块是死控件 | `[!]` 本期不接 | `[~]` **重新打开** | 同上；存盘部分（`GConfig->SetFloat`）没被删，仍然有效 |

### B.2 修改 §1.1 的范围表

老表里“不做”列有 **“音量（含 Wwise RTPC、总线、设备音量）”**，改为：

| 做 | 不做 |
|---|---|
| 主音量滑块（映射到 Wwise `all` RTPC，0~100 线性） | Sfx / Bgm 分离滑块（本期只做总音量） |
| 进页面回填当前音量、应用后写盘 | 设备选择、总线结构改动 |
| 0×0 兜底、返回兜底、下拉填充与防递归 | 画质、语言、按键、灵敏度 |

### B.3 替换 §4（原“声音：占位，本期不做”）

**2026-10-20 二次修正：下面第 1、2 步的 C++ 做法已作废**（音频 C++ 层整体删除，`ApplyMasterVolumeToAudio()` 函数本身也已移除）。现在的做法只有一条：

1. ~~C++：`ApplyMasterVolumeToAudio()` 实现为~~ → **改为蓝图**：`WBP_SlimeSettings` 的 `On Settings Values Changed` 里
   `Get Master Volume`（0~1）→ `×100` → `Set RTPC Value`（RTPC = `all`，插值 100ms，`Actor` 留空）。
   只在这一处接，滑块上不要再接一遍。
2. ~~C++：`NativeConstruct()` 末尾补一次调用~~ → **不需要**：`OnSettingsValuesChanged` 在构造时就会触发一次，已经覆盖。
3. WBP：`Slider_Volume` 勾回 `Is Enabled`，`Value` 由 `GetMasterVolume()` 回填，`On Value Changed → Set Master Volume`（**只改暂存值**，别在这里发声）。
4. 存档不动：继续用 `[/Script/SlimeWar.SlimeSettingsWidget] MasterVolume`（`GGameUserSettingsIni`）。
5. 老的 §4.4“恢复音量时要补什么”整节可删（已执行）。

### B.4 验收表两行更新

| # | 老期望 | 新期望 |
|---|---|---|
| 15 | 打开设置没有可操作的音量控件 | 音量滑块**可拖**，拖动时能听到 UI 音变化 |
| 17 | 声音滑块不在 0 位（置灰） | 音量滑块停在存档值（首次 1.0），拖动后重启保持 |

---

## C. 新增章节：三项从未进过任何计划的内容

### C.1 `WBP_AimPreview`（准星与蓄力环）

资产已存在：`/Game/_SlimeWar/UI/WBP_AimPreview`、`/Game/_SlimeWar/UI/MI_ChargeRing`。当前**没有任何程序挂点**。

**挂点选择**：不要放在“玩家 BeginPlay 里 Create Widget”，而是由 `USlimeUISubsystem` 创建、按阶段显示。理由：UI 的显示权归 `USlimeUISubsystem`（项目既有约定），放角色里会导致主菜单/准备页上也挂着准星。

| 项 | 做法 |
|---|---|
| 创建 | `USlimeUISubsystem::EnsureWidgets()` 里新增 `AimPreviewWidget`，`AddToViewport(0)`，默认 `Collapsed` |
| 显示 | `ApplyPhaseToScreens()`：`Running` 显示，其余阶段折叠 |
| 数据来源 | `ASlimeWarCharacter::GetChargeAlpha()`（`SlimeWarCharacter.h:117`）、`IsCharging()`（`:118`） |
| 弱点锁定 | `USlimeAimAssistComponent::IsWeakpointLocked()`（`SlimeAimAssistComponent.h:43`） |
| 更新方式 | **允许 WBP Tick 轮询**：这是每帧连续的动画数据（0~1 环），不是 GameState 快照；轮询两个纯函数，开销可忽略。这与“HUD 不轮询 GameState”的规则不冲突 |
| 控件接线 | `Img_ChargeRing_Fill` 的动态材质设 `ChargePercent`；`Text_ChargePercent` 显示 `0%~100%`；`Img_Crosshair_Weakpoint` 按锁定状态切 `Visible/Collapsed`，同时折叠 `Img_Crosshair_Default` |
| 音频 | 满蓄力播一次 `Collect`、首次锁弱点播 `Choose3`（见《UI音频接入方案》§3.8），两者都要“进入状态时播一次”，不要每帧判 |

**边界**：`IsCharging()==false` 时环必须归零并隐藏，否则松手瞬间会留一个满环残影。

### C.2 受伤红屏 `M_HurtIndicator_PP`

资产：`/Game/_SlimeWar/UI/M_HurtIndicator_PP`，参数 `Intensity`（0~1）。

**推荐做法**：不要把材质挂到关卡里的 PostProcessVolume，而是在玩家角色上加一个 `UPostProcessComponent`：

| 方案 | 优点 | 缺点 |
|---|---|---|
| A（推荐）角色挂 `UPostProcessComponent` | 一次配置，**所有地图自动生效**；不受关卡是否摆 Volume 影响 | 多占一个组件；需要在角色 BP 里设 Blendable |
| B 每关摆 PostProcessVolume | 符合“后期处理放场景”的习惯 | 每张新地图都要记得摆，漏了就没红屏 |

具体：

1. `BP_ThirdPersonCharacter` 加 `PostProcessComponent`，`Unbound = true`，Blendables 加 `M_HurtIndicator_PP`（Weight 1.0）。
2. 角色 C++ 取到该组件 → `CreateDynamicMaterialInstance` → 记下来。
3. 绑定已有的 `OnPlayerDamaged(FVector)`（BlueprintAssignable，`GameConfig` §2.6）：
   ```text
   Intensity = 0.8
   0.12s 线性插值回 0（用 Tick 或 Timer 都行，建议 Timer 每帧更新更省）
   ```
4. 低血量常驻：`Health/MaxHealth < 30%` 时把 Intensity 压到 0.15 常亮，作为“濒死提示”；回血/过关就归零。
5. **暂停时不要继续衰减**：暂停是 `GlobalTimeDilation=0`，如果用的是世界 Timer 会自动停，符合预期，不需要特判。

### C.3 HUD 特殊子弹 UI

资产里控件已存在（`UI改动对接文档.md` §三）：`Img_Assimilate_Icon`、`Text_Special_Assimilate_Ammo`、`Img_Vortex_Icon`、`Text_Special_Vortex_Ammo`。

这些**不需要改 C++**，只接线：

```text
Event On Grenade Inventory Updated (Assimilation, Tornado)
  ├─ Text_Special_Assimilate_Ammo → "×{Assimilation}"
  ├─ Text_Special_Vortex_Ammo     → "×{Tornado}"
  ├─ Branch (Assimilation == 0) → Img_Assimilate_Icon.Set Color and Opacity(灰)
  └─ Branch (Tornado == 0)      → Img_Vortex_Icon.Set Color and Opacity(灰)

Event On Grenade Selection Updated (Type, Faction)
  ├─ Branch (Type == Assimilation) → 同化 icon 高亮（白）、旋风 icon 压暗
  └─ Branch (Type == Tornado)      → 反之
```

注意：`OnGrenadeSelectionUpdated` 的第二个参数是**目标阵营**（A/B/C），不是数量；同化雷切换阵营时这个事件也会再触发一次，所以“高亮”是幂等的，不要在里面做计数。

---

## D. 落地顺序与依赖

```text
第一层（可并行）
  D1  C++：C-06 IndexOfLevel → C-01 ResultCamera → C-02/C-03/C-04/C-05
  D2  ~~C++：AkAudio 依赖 → USlimeAudioSubsystem → DA_SlimeAudioConfig~~ **已作废（2026-10-20）**：
      音频 C++ 层整体删除，改为蓝图层直接 `Post Event`，不占 D 序列，见《音频接入执行清单》批次 1~7
  D3  C++：玩家 PostProcessComponent + 受伤驱动（本补丁 §C.2）

第二层（依赖第一层编译通过）
  D4  WBP_SlimeResult：数字（HostileKills）→ 进度条 → 印章 → 按钮 → 照片 → 动画
  D5  WBP_AimPreview：由 UISubsystem 创建 + Tick 取 ChargeAlpha
  D6  WBP_SlimeSettings：音量滑块恢复 + 三键 + 两个下拉
  D7  HUD：特殊子弹 4 控件

第三层（依赖第二层）
  D8  结算音（必须等 C-03 的 OnResultShown）
  D9  逐页验收：本补丁 A.8 + 老 §10
```

## E. 与老文档的冲突对照总表

| 老文档位置 | 老写法 | 本文处理 |
|---|---|---|
| `ResultPage_Implementation_Plan.md` §0.4 Q-01/Q-02 | 待确认 | A.2 结案 |
| 同上 §2.1 数据表 | 清点数 = ClearedPoints | A.1 替换为 HostileKills |
| 同上 §5.1 / A-01 / A-03 | 只针对 `L_Whitebox_CityPlaza`、单关卡 | A.3 改为两张生态图 |
| 同上 §1.1 照片决策 | 机位对齐 `LS_Result` 末帧 | A.4 解耦 |
| 同上 §6.2 采样表 | Target=300 | A.5 换成 200/160 |
| 同上 §7 验收表 | 300 分、单关 | A.8 全表替换 |
| 同上 §2.2 时序图 | `EndReason` 两种 | A.1 补 `Extinction` |
| `UI_Settings_Implementation.md` §0.1 S-11 | 音量置灰、本期不做 | B.1 改为可操作 |
| 同上 §0.2 R-11 | 音量是死控件 | B.1 关闭风险 |
| 同上 §1.1 / §4 | 不做音量 | B.2 / B.3 替换 |
| 同上 §10 验收 15/17 | 无可操作音量控件 | B.4 替换 |
| 两份老文档都没有 | — | §C 新增 AimPreview / HurtIndicator / 特殊子弹 |

## F. 变更记录

| 日期 | 摘要 |
|---|---|
| 2026-10-20 | 初版：结算字段换 HostileKills、EndReason 补 Extinction、关卡/采样/验收表按双点三点重写、Settings 音量放开、新增 AimPreview 与受伤红屏与特殊子弹三节 |
