# SlimeWar 当前配置说明（UI / 地图 / 玩家 / 史莱姆）

- 适用日期：2026-10-20
- 适用分支：`rebulid-program`
- 读者：UI 蓝图程序、美术、策划、接手 C++ 的程序
- 原则：本文记录**当前运行时实际行为**；策划案目标值与当前实现不一致的地方单独标注，不把目标值写成已实现值。

---

## 0. 先看结论

1. C++ Canvas 调试 HUD 已默认关闭。分数、时间、站点、矿点、史莱姆状态文字不再由 C++ 绘制。
2. 正式 HUD、菜单、准备页、暂停页、结算页全部由 UMG WBP 负责；C++ 只广播事件和数据。
3. `ASlimeHUD` 仍保留，作为可选调试工具；单独把 `Slime.Debug.DrawRun=1` 等 CVar 打开才会出现。
4. `Config/DefaultGame.ini` 已显式写入关闭值，避免打包后默认打开调试 HUD。
5. 史莱姆是单一 C++ 运行时实体 `ASlimeEcologyEnemy`，不再使用 StateTree，也没有 A/B 分队编制。

---

## 1. 资产与配置入口

### 1.1 项目设置

打开 **Project Settings → Game → Slime War**，对应类为 `USlimeGameSettings`。

| 配置 | 当前路径 / 值 | 用途 |
|---|---|---|
| `RunConfig` | `/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint` | 全局回退 RunConfig |
| `DamageEffectClass` | `/Script/SlimeWar.GE_Damage` | 玩家 GAS 伤害 GE |
| `HUDWidgetClass` | `/Game/_SlimeWar/UI/WBP_SlimeHUD.WBP_SlimeHUD_C` | 局内 HUD |
| `PreparationWidgetClass` | `/Game/_SlimeWar/UI/WBP_SlimePreparation.WBP_SlimePreparation_C` | 落点选择页回退 |
| `ResultWidgetClass` | `/Game/_SlimeWar/UI/WBP_SlimeResult.WBP_SlimeResult_C` | 结算页 |
| `PauseWidgetClass` | `/Game/_SlimeWar/UI/WBP_SlimePause.WBP_SlimePause_C` | 暂停页 |
| `MainMenuWidgetClass` | `/Game/_SlimeWar/UI/WBP_SlimeMainMenu.WBP_SlimeMainMenu_C` | 主菜单 |
| `SettingsWidgetClass` | `/Game/_SlimeWar/UI/WBP_SlimeSettings.WBP_SlimeSettings_C` | 设置页 |
| `LevelSelectWidgetClass` | `/Game/_SlimeWar/UI/WBP_SlimeLevelSelect.WBP_SlimeLevelSelect_C` | 选关页 |
| `Levels` | `DA_Level_DoublePoint`、`DA_Level_ThreePoint` | 选关槽位，顺序即槽位顺序 |
| `bSkipMainMenuInPIE` | `false` | 仅编辑器调试用；正式流程从主菜单开始 |

全局文件：`Config/DefaultGame.ini`。

### 1.2 关卡定义

每个关卡由 `USlimeLevelDefinition` 描述，资产在：

```text
/Game/_SlimeWar/Core/Data/Levels/
```

| 关卡 | LevelId | DisplayName | Map | RunConfig |
|---|---|---|---|---|
| `DA_Level_DoublePoint` | `DoublePoint` | `Double Point Warehouse` | `L_Ecology_DoublePoint` | `DA_RunConfig_DoublePoint` |
| `DA_Level_ThreePoint` | `ThreePoint` | `Three Point Arena` | `L_Ecology_ThreePoint` | `DA_RunConfig_ThreePoint` |

两张关卡定义的 `PreparationWidgetClass`、`DeploySequences`、`ResultSequence` 当前为空，
因此使用 `USlimeGameSettings` 的全局回退配置。若某一关要独立美术风格，优先在关卡定义里覆盖，
不要把全局类改成只适合一张图的资产。

### 1.3 数据资产

| 资产 | 路径 | 负责内容 |
|---|---|---|
| 双点 RunConfig | `/Game/_SlimeWar/Core/Data/DA_RunConfig_DoublePoint` | 回合、玩家、相机、数据表引用 |
| 三点 RunConfig | `/Game/_SlimeWar/Core/Data/DA_RunConfig_ThreePoint` | 同上 |
| 双点落点布局 | `/Game/_SlimeWar/Core/Data/DA_SpawnLayout_DoublePoint` | D1/D2 |
| 三点落点布局 | `/Game/_SlimeWar/Core/Data/DA_SpawnLayout_ThreePoint` | D1/D2 |
| 史莱姆体量/生命 | `/Game/_SlimeWar/Core/Data/DT_SlimeStats` | mass 1–8 的生命、半径、占位速度 |
| 旧武器表 | `/Game/_SlimeWar/Core/Data/DT_WeaponStats` | 仅作 `WeaponRules` 缺失时回退 |
| 当前步枪规则 | `/Game/_SlimeWar/Weapon/DA_Weapon_Rifle` | 当前实际伤害、弹匣、蓄力、磁性瞄准 |
| 生态规则 | `/Game/_SlimeWar/Ecology/DA_EcologyRules` | 史莱姆 AI、身体、融合、核心、资源、警戒、手雷、粘液 |

> 当前运行时优先使用 `DA_Weapon_Rifle`，不是 `DT_WeaponStats.csv`。改 CSV 不会改变当前步枪。

---

## 2. C++ 调试 HUD 与 UMG 的边界

### 2.1 已关闭的内容

`ASlimeHUD` 是 C++ Canvas 调试 HUD，默认不再绘制：

- 准星
- 分数 / 倒计时 / 阵营站点 / 矿点 / 资源账本
- 每只史莱姆上方的体量、生命、融合、大脑、眩晕、Cargo 文本
- 瞄准辅助射线、融合连线、旧 Aggro 路径

代码位置：

- `Source/SlimeWar/Private/Core/SlimeWarCVars.cpp`：默认值改为 0
- `Source/SlimeWar/Private/Debug/SlimeHUD.cpp`：资源面板改由 `Slime.Debug.DrawRun` 控制
- `Config/DefaultGame.ini`：`[ConsoleVariables]` 显式关闭

验证截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00122.png`

### 2.2 调试 HUD 开关

控制台输入以下命令可单独打开，用于排查问题；正式画面不要全部打开。

| CVar | 默认 | 内容 |
|---|---:|---|
| `Slime.Debug.Crosshair` | 0 | C++ 占位准星 |
| `Slime.Debug.DrawRun` | 0 | 分数、时间、站点、矿点、账本 |
| `Slime.Debug.DrawEcologyState` | 0 | 史莱姆头顶状态块 |
| `Slime.Debug.DrawAimAssist` | 0 | 相机射线、辅助射线、锁定目标 |
| `Slime.Debug.DrawFusion` | 0 | 融合连线、接触进度、会合点 |
| `Slime.Debug.DrawEnemyState` | 0 | 旧敌人状态标签 |
| `Slime.Debug.DrawAggroPath` | 0 | 旧 Aggro 规划路径 |
| `Slime.Debug.CombatLog` | 0 | 伤害/死亡日志 |
| `Slime.Debug.Paint` | 1 | 粘液 splat 的世界空间调试绘制 |
| `Slime.Run.TimeScale` | 1 | 回合时间倍率；0 冻结倒计时 |

`Slime.Debug.Paint` 不是 UMG，也不是 Canvas HUD；它是在没有正式粘液材质时的世界空间调试显示。
美术材质接好后可把 `DA_EcologyRules` 的 `PaintSplatMaterial` / `PaintSurfaceMaterial` 配上，
再把 `Slime.Debug.Paint` 设为 0。

### 2.3 UMG 页面流程

由 `USlimeUISubsystem` 管理，顺序为：

```text
MainMenu
  -> LevelSelect
  -> Preparation（选落点）
  -> Deploying（投放序列）
  -> Running（HUD）
  -> Result（结算镜头）
  -> Ended（结算页）
```

暂停页由角色的暂停键触发，不改变回合状态机。

### 2.4 WBP 父类

| WBP | 父类 | 关键事件 / 函数 |
|---|---|---|
| `WBP_SlimeHUD` | `USlimeHUDWidget` | 见 2.5 |
| `WBP_SlimePreparation` | `USlimePreparationWidget` | `OnPreparationReady`、`OnDropPointPreviewed` |
| `WBP_SlimeResult` | `USlimeResultWidget` | `OnResultReady` |
| `WBP_SlimePause` | `USlimePauseWidget` | `Resume`、`RequestRetry`、`RequestReselectDropPoint`、`RequestQuit` |
| `WBP_SlimeMainMenu` | `USlimeMainMenuWidget` | `StartGame`、`OpenSettings`、`QuitGame` |
| `WBP_SlimeLevelSelect` | `USlimeLevelSelectWidget` | `OnLevelSelectionChanged` |
| `WBP_SlimeSettings` | `USlimeSettingsWidget` | `OnSettingsValuesChanged` |

WBP 必须在 **File → Reparent Blueprint** 里选对应 C++ 父类，否则找不到事件节点。

### 2.5 局内 HUD 事件

`USlimeHUDWidget` 只在构造和值变化时推送数据，WBP 不应该每帧自己查 GameState。

| 事件 | 何时触发 | 用途 |
|---|---|---|
| `OnScoreUpdated(Score, TargetScore)` | 构造、分数变化 | 分数和目标分 |
| `OnTimeUpdated(RemainingSeconds)` | 构造、每秒 | 倒计时 |
| `OnHealthUpdated(Health, MaxHealth)` | 构造、生命变化 | 血条 |
| `OnAmmoUpdated(CurrentAmmo, MagazineSize)` | 构造、弹药变化 | 弹匣 |
| `OnGrenadeInventoryUpdated(Assimilation, Tornado)` | 构造、投掷/重置 | 两种手雷库存 |
| `OnGrenadeSelectionUpdated(Type, Faction)` | 构造、切换 | 当前手雷与目标阵营 |
| `OnEcologySitesUpdated(Sites)` | 每 0.5 秒 | A/B/C 站点状态 |
| `OnScoreEarned(NewScore, DeltaScore)` | 分数实际上涨 | 击杀飞入图标 |
| `ShowTutorial(Message)` | 首次落地、换弹、目标达成、最后 15 秒、首次融合提示 | Toast |

`OnPointStateUpdated` 和 `OnBatchIncoming` 是旧分队系统兼容空事件，**当前运行时不会调用**。
新 WBP 不要实现它们；旧 WBP 可以删除对应逻辑。

`FSlimeSiteStatus` 字段：

| 字段 | 含义 |
|---|---|
| `SiteId` | 站点 ID |
| `Faction` | 当前阵营 |
| `Location` | 世界坐标 |
| `LiveCount` | 存活个体数 |
| `LiveMass` | 存活体量总和 |
| `DeliveredCount` | 已交付货物数 |
| `CoreCount` | 核心数 |
| `PendingSpawns` | 待生成请求 |

### 2.6 玩家受伤方向

`ASlimeWarCharacter::OnPlayerDamaged(FVector DamageDirection)` 是 BlueprintAssignable。
HUD WBP 可以在 Construct 时获取玩家角色并绑定：

```text
Get Player Pawn -> Cast To SlimeWarCharacter -> Bind Event to OnPlayerDamaged
```

`DamageDirection` 是从玩家指向伤害来源的方向，可用于屏幕边缘红色方向提示。

### 2.7 结算页数据

`OnResultReady(const FSlimeRunResult&)` 在结算页真正显示时触发一次，字段：

```text
Score, TargetScore, BestScore
NormalKills, PlayerKills, NpcKills
PlayerCargoScored, HostileKills
Detonations, ChainKills, DeliveredCargo, Fusions
ClearedPoints, EndReason, bPassed
ElapsedTime, PlayerDamageTaken
```

`EndReason`：`TimeUp` / `PlayerDied` / `Extinction`。
达到目标分不会提前结束回合；时间、玩家死亡、生态灭绝才会结束。

### 2.8 UI 调整建议

- 准星由 WBP 画，C++ 准星默认关闭；准星中心应与屏幕中心一致。
- 分数、倒计时、生命、弹匣、手雷、站点状态分别绑定上表事件，不要轮询。
- 阵营颜色建议统一：

```text
A = (0.33, 0.85, 0.67) 青绿
B = (0.95, 0.72, 0.36) 橙黄
C = (0.71, 0.61, 1.00) 紫色
```

- 站点状态建议显示：阵营、存活数、存活体量、交付数、核心数。不要显示“生成中/已清空”旧文案。
- 手雷 UI 需要显示两种库存、当前选择、目标阵营；同化雷的阵营选择键为 `Z/X/C`。
- 教程文本目前是 C++ 的英文 `NSLOCTEXT`，WBP 可以直接显示，也可以按 Message 内容映射成中文/自定义提示。
- 暂停键是 `Escape`；WBP 不需要自己监听按键，`USlimeUISubsystem` 会打开暂停页。
- 推荐设计分辨率 1920×1080，使用 Safe Zone；右下角手雷/弹药、左上角分数、右上角时间、底部中央生命。

---

## 3. 地图配置

### 3.1 可玩关卡

| 地图 | 尺寸（X×Y） | 回合 | 目标分 | 阵营 | 落点 | 矿点 |
|---|---|---:|---:|---|---|---|
| `L_Ecology_DoublePoint` | 30×44 m | 150 s | 200 | A/B | D1/D2 | 4 |
| `L_Ecology_ThreePoint` | 50×60 m | 180 s | 160 | A/B/C | D1/D2 | 5 |
| `L_Whitebox_CityPlaza` | 旧白盒 | - | - | 回归/调试 | - | - |

### 3.2 双点地图

范围：X `[-1500, 1500]`，Y `[-2200, 2200]`。

| 元素 | 坐标（cm） | 配置 |
|---|---|---|
| A 站点 | `(-100, -1400)` | A 阵营，10 只初始体量 1 |
| B 站点 | `(-100, 1400)` | B 阵营，10 只初始体量 1 |
| D1 | `(-1100, -1800)` | 落点索引 0 |
| D2 | `(1100, 1800)` | 落点索引 1 |
| WestNear | `(500, -700)` | 4/4，不再生 |
| EastNear | `(-500, 700)` | 4/4，不再生 |
| SouthShared | `(600, 0)` | 5/6，5 秒恢复 1 份 |
| NorthShared | `(-600, 0)` | 5/6，5 秒恢复 1 份 |

初始资源：18 份库存；初始体量：20。

### 3.3 三点地图

范围：X `[-2500, 2500]`，Y `[-3000, 3000]`。

| 元素 | 坐标（cm） | 配置 |
|---|---|---|
| A 站点 | `(-1000, -1300)` | A 阵营，10 只初始体量 1 |
| B 站点 | `(-1000, 1300)` | B 阵营，10 只初始体量 1 |
| C 站点 | `(1500, 0)` | C 阵营，10 只初始体量 1 |
| D1 | `(-1800, -2200)` | 落点索引 0 |
| D2 | `(1800, 2200)` | 落点索引 1 |
| A_Near | `(-1400, -1300)` | 4/4，不再生 |
| B_Near | `(-1400, 1300)` | 4/4，不再生 |
| C_Near | `(1900, 0)` | 4/4，不再生 |
| Center_North | `(0, 700)` | 5/6，5 秒恢复 1 份 |
| Center_South | `(0, -700)` | 5/6，5 秒恢复 1 份 |

初始资源：22 份库存；初始体量：30。

### 3.4 站点 Actor

类：`ASlimeFactionSite`。

| 属性 | 默认 | 用途 |
|---|---:|---|
| `SiteId` | A | 站点 ID |
| `Faction` | A | 阵营 |
| `InitialPopulation` | 10 | 开局体量 1 数量 |
| `ActivityRadius` | 1200 cm | 普通个体活动半径 |
| `DeliveryRadius` | 130 cm | 交付结算半径 |
| `SpawnRingMin` | 250 cm | 交付增员生成内圈 |
| `SpawnRingMax` | 400 cm | 交付增员生成外圈 |
| `DisplayColor` | 青绿 | 站点垫片和标签颜色 |

### 3.5 矿点 Actor

类：`ASlimeMineralPatch`。

| 属性 | 默认 | 用途 |
|---|---:|---|
| `PatchId` | Patch | 矿点 ID |
| `InitialStock` | 4 | 初始库存 |
| `MaxStock` | 4 | 最大库存 |
| `bRegenerates` | false | 是否再生 |
| `RegrowSeconds` | 5 s | 再生间隔 |
| `GatherRange` | 120 cm | 采集范围 |

### 3.6 地图制作约束

- 所有可站区域必须有地面碰撞和 NavMesh；当前 Recast 需要烘焙/动态生成。
- 外墙和内部墙体使用 `StaticMeshActor + Static Mobility + BlockAll`，否则冲击分裂可能不触发。
- 地面不要只做视觉网格；必须有 Simple Collision。
- 粘液平面当前运行时光栅为 700×1024（宽×高），世界尺寸 `3000×4400 cm`（`DA_EcologyRules.PaintWorldSize`）。
  双点地图匹配；三点地图如果要做满场粘液，需要按关卡扩大或拆分该参数。
- 站点/矿点的 C++ 占位网格可以在 BP 子类的 Components 面板替换；碰撞和位置由 Actor 负责。
- 新增关卡：创建地图 → 创建 `DA_RunConfig` / `DA_SpawnLayout` / `DA_Level` → 在 `USlimeGameSettings::Levels` 添加一行
  `+Levels=/Game/.../DA_Level_X.DA_Level_X`。不需要改 C++。

### 3.7 后续新增关卡与选关配置

当前选关不是“按地图文件名扫描”，而是按 `USlimeGameSettings::Levels` 的数组顺序固定读取。
新增一张可玩地图，按下面顺序做，任何一步漏掉都会表现为“选关里看不到”或“点了进不去”。

#### 第一步：创建地图

1. 复制一张现有生态地图作为模板（推荐 `L_Ecology_DoublePoint`），命名为
   `L_Ecology_<LevelName>`，放在 `/Game/_SlimeWar/Maps/`。
2. 保留地板、灯光、PlayerStart、RecastNavMesh；删除旧站点/矿点 Actor 后重新摆放。
3. 检查地板范围覆盖所有站点、矿点和落点，外墙完整，NavMesh 覆盖可站区域。
4. 保存地图，运行 `SlimeBakeNav`（编辑器控制台）或在 Build 菜单重建导航。

#### 第二步：创建 RunConfig

复制 `DA_RunConfig_DoublePoint` 为 `DA_RunConfig_<LevelName>`，至少修改：

| 字段 | 说明 |
|---|---|
| `RunDuration` | 本关回合秒数 |
| `TargetScore` | 本关目标分 |
| `SpawnLayout` | 指向本关的 `DA_SpawnLayout_<LevelName>` |
| `EcologyRules` | 通常共用 `DA_EcologyRules` |
| `WeaponRules` | 通常共用 `DA_Weapon_Rifle` |
| `Player...` / `Camera...` | 如果本关需要不同手感再改 |

不要直接改 `DA_RunConfig_DoublePoint` 来服务新关卡；否则双点回归会被破坏。

#### 第三步：创建 SpawnLayout

复制 `DA_SpawnLayout_DoublePoint` 为 `DA_SpawnLayout_<LevelName>`，在 `DropPoints` 数组里按顺序填本关落点。
详见下一节 3.8。

#### 第四步：创建 LevelDefinition

复制 `DA_Level_DoublePoint` 为 `DA_Level_<LevelName>`，字段填写：

| 字段 | 填写 |
|---|---|
| `LevelId` | 稳定 ID，例如 `ThreePoint`；不要用数组下标代替 |
| `DisplayName` | 选关页显示名 |
| `MapAsset` | 指向本关 `.umap` |
| `RunConfig` | 指向本关 RunConfig |
| `PreparationWidgetClass` | 可空，空则用全局 `WBP_SlimePreparation` |
| `DeploySequences` | 可空，空则用 `USlimeGameSettings::DeploySequences` 回退 |
| `ResultSequence` | 可空，空则用全局 `ResultSequence` |

#### 第五步：注册到选关

在 `Config/DefaultGame.ini` 的 `USlimeGameSettings` 段追加一行，顺序就是选关槽位：

```ini
+Levels=/Game/_SlimeWar/Core/Data/Levels/DA_Level_<LevelName>.DA_Level_<LevelName>
```

当前已有两行：

```ini
+Levels=/Game/_SlimeWar/Core/Data/Levels/DA_Level_DoublePoint.DA_Level_DoublePoint
+Levels=/Game/_SlimeWar/Core/Data/Levels/DA_Level_ThreePoint.DA_Level_ThreePoint
```

也可以在 **Project Settings → Game → Slime War → Levels** 里加数组项。
当前选关 UI 预留 4 个槽位，但 `GetConfiguredLevelCount()` 只返回真实配置数；
未配置的槽位应显示为锁定，不要调用 `EnterSelectedLevel()`。

#### 第六步：检查选关页

WBP_SlimeLevelSelect 的节点数量必须覆盖 `GetLevelCount()`（当前 4）。
每个节点至少绑定：

- `GetLevelDisplayName(Index)`：标题
- `GetLevelId(Index)`：缩略图/关卡 ID
- `IsLevelAvailable(Index)`：锁定态
- `SelectLevel(Index)`：点击选中
- `EnterSelectedLevel()`：确认进入

如果新关卡没有配置 `PreparationWidgetClass`，准备页会使用全局 WBP；如果该关落点数量不同，
建议单独做一个准备页 WBP 并在 LevelDefinition 里指定，避免 2 个 marker 的旧页误点出第 3 个落点。

### 3.8 关卡 → 地图 → 落点配置

#### 数据链

```text
选关槽位
  -> USlimeLevelDefinition
       -> MapAsset（本关地图）
       -> RunConfig（本关数值）
            -> SpawnLayout（本关落点数组）
                 -> DropPoints[0] = D1 世界坐标
                 -> DropPoints[1] = D2 世界坐标
```

#### 当前落点

| 关卡 | SpawnLayout | DropPoints[0] | DropPoints[1] |
|---|---|---|---|
| DoublePoint | `DA_SpawnLayout_DoublePoint` | `(-1100, -1800, 0)` | `(1100, 1800, 0)` |
| ThreePoint | `DA_SpawnLayout_ThreePoint` | `(-1800, -2200, 0)` | `(1800, 2200, 0)` |

`DropPoints` 是世界坐标（cm），数组顺序就是 UI marker 下标：

- 下标 0 → `Btn_DropPoint0` / 第一张投放序列
- 下标 1 → `Btn_DropPoint1` / 第二张投放序列
- 下标 N → 第 N+1 个 UI 标记

落点 Z 建议填地面高度（通常 0）；部署时 C++ 会临时把角色放在 `DropPoint + (0,0,150)`，
再由重力落地。不要把 Z 填成角色胶囊中心或高空位置。

#### 修改落点

1. 打开对应 `DA_SpawnLayout_*`，编辑 `DropPoints` 数组。
2. 更新准备页 WBP 的 marker 数量和位置；marker 位置必须和世界坐标对应。
3. 如果新增落点，补一张对应的 `LS_Deploy_*` 序列，或在 LevelDefinition / GameSettings 里留空跳过。
4. 用 `SlimeRunDeploy 0` / `SlimeRunDeploy 1` 验证每个落点；日志应出现
   `URunSubsystem: deploying to drop point N`。
5. 落点越界或数组为空时，`BeginDeployment` 会报错并拒绝进入，不会自动回退到 0。

#### 选关槽位替换地图

只想换某关地图时，改对应的 `DA_Level_*.MapAsset`，不要调整 `+Levels=` 顺序；
调整顺序会让旧存档/最近选择指向另一关。

### 3.9 史莱姆生成点位置设置

当前**没有独立的 SlimeSpawnPoint / SpawnSlot Actor 列表**。史莱姆出生位置由阵营站点生成：

```text
ASlimeFactionSite 的 Actor 位置
  -> 在 SpawnRingMin..SpawnRingMax 的圆环上取点
  -> 投影到 NavMesh / 地面
  -> 避开玩家 SiteSpawnPlayerMinDistance
  -> 避开其他个体和墙体
```

#### 站点就是主要出生锚点

| 站点属性 | 对生成的影响 |
|---|---|
| Actor Location | 出生圆环中心，也是交付点 |
| `Faction` | 生成个体阵营 |
| `InitialPopulation` | 开局体量 1 数量（当前默认 10） |
| `ActivityRadius` | 普通个体活动半径 |
| `SpawnRingMin/Max` | 初始和交付增员的环形半径 |
| `DeliveryRadius` | 返程交付判定 |

当前两张地图的站点位置：

```text
DoublePoint：A (-100, -1400)  B (-100, 1400)
ThreePoint： A (-1000, -1300) B (-1000, 1300) C (1500, 0)
```

要改变“史莱姆从哪里出来”，优先移动对应 `ASlimeFactionSite` Actor，其次调整
`SpawnRingMin/Max`，不要新增一个没有注册的 SpawnPoint Actor。

#### 生成约束

| 参数 | 当前值 | 位置 |
|---|---:|---|
| 生成环内/外 | 250 / 400 cm | `ASlimeFactionSite` 或 `DA_EcologyRules` |
| 玩家最小距离 | 270 cm | `DA_EcologyRules.SiteSpawnPlayerMinDistance` |
| 每帧生成请求 | 8 | `DA_EcologyRules.SpawnRequestsPerFrame` |
| 交付增员 | 1 份货物 → 1 只体量 1 | 生态子系统 |
| 初始人数 | 每站 10 | `ASlimeFactionSite.InitialPopulation` |

如果站点位置被墙/导航阻挡，生成会尝试地面碰撞回退；仍失败时保留排队请求，后续重试。
不要把站点放在墙体内部或地图边界外。

#### 需要固定多簇出生时

当前 C++ 只支持“围绕站点环形生成”。如果策划要求同一阵营有多个固定出生簇，需要新增一个
生成簇配置（例如 `FSlimeSpawnCluster` 数组）并修改 `USlimeEcologySubsystem`，
不能只在地图上多放几个同名 Actor 就生效。文档先记录这个限制，避免美术/策划误以为
场景里的空 Actor 会被自动识别。

### 3.10 模型与材质配置

#### 史莱姆

| 项目 | 当前资产/入口 | 调整方法 |
|---|---|---|
| 身体网格 | `/Game/Characters/Slime/SM_Slime_Base` | 在 `DT_SlimeStats` 每行覆盖 `Mesh`，或改 `ASlimeEnemyBase::DefaultBodyMesh` |
| 身体碰撞 | C++ 胶囊 | 改 `DT_SlimeStats` 的 `BodyRadius`；不要用网格碰撞代替胶囊 |
| 阵营颜色 | `USlimePaintSubsystem::GetFactionColor` | A 青绿、B 橙黄、C 紫；材质实例应暴露颜色参数 |
| 贴地 | `ASlimeEnemyBase::ApplyBodyRadius` | 自动按胶囊半径缩放，改网格后检查底部是否贴地 |
| 弱点 | `USlimeWeakpointComponent` | mass 7/8 才显示；半径 30、长 32 |

美术不要直接把网格放大到和胶囊不匹配；代码会自动把网格水平包围盒适配胶囊。若模型有特殊
枢轴、武器/角/装饰凸出，需在网格资产里修正枢轴或在 BP 子类里加偏移。

#### Cargo / 地面货物

| 项目 | 当前 |
|---|---|
| 类 | `ASlimeCargoVisual`，地面拾取为 `ASlimeCargoPickup` |
| 默认网格 | `/Engine/BasicShapes/Cube.Cube` |
| 默认缩放 | `BaseScale=0.24`，数量越多略微放大 |
| 携带位置 | 胶囊上方 `Radius × 0.65` |

替换方式：改 `ASlimeCargoVisual::CargoMeshAsset`，或在 BP 子类里替换 `CargoMesh` 组件。
当前 `ASlimeEcologyEnemy` 直接 Spawn `ASlimeCargoVisual::StaticClass()`；如果必须使用 BP 子类，
需要把生成类提成可配置项（代码改动）。

#### 站点与矿点

| 项目 | 当前占位 | 替换方式 |
|---|---|---|
| 站点垫片 | `ASlimeFactionSite::PadMesh` | BP 子类 Components 面板替换 StaticMesh / 材质 |
| 站点标签 | `ASlimeFactionSite::Label` | 改 TextRender 字体、大小、颜色；或 BP 里改为 WidgetComponent |
| 站点颜色 | `DisplayColor` | 同时影响垫片材质参数 `Color` 和标签颜色 |
| 矿点晶体 | `ASlimeMineralPatch::CrystalMesh` | BP 子类替换 StaticMesh / 材质 |
| 矿点标签 | `ASlimeMineralPatch::Label` | 改字体、大小；库存文字由 C++ 刷新 |

站点/矿点的碰撞和位置由 Actor 负责，不要只放视觉网格。矿点库存标签由
`ASlimeMineralPatch::RefreshLabel` 更新，不要在 WBP 里重复计算。

#### 粘液地面与 splat

| 项目 | 当前 | 调整 |
|---|---|---|
| 地面平面 | `ASlimePaintField`，运行时自动生成 | 可在关卡里放一个实例覆盖默认 |
| 世界尺寸 | `WorldSize=(3000,4400)` | 双点地图匹配；三点地图需扩大或分块 |
| RenderTarget | 700×1024 | `DA_EcologyRules.PaintTextureWidth/Height` |
| Splat 材质 | `PaintSplatMaterial` | 由美术提供，参数见下 |
| 地面材质 | `PaintSurfaceMaterial` | 由美术提供 |
| 参数名 | `SlimePaint`、`SplatUV`、`SplatColor`、`SplatRadius` | `DA_EcologyRules` 可改 |

正式材质接好后，把 `Slime.Debug.Paint` 设为 0，避免调试 splat 和正式材质叠加。

#### 手雷、玩家、场景模型

| 模型 | 当前入口 | 注意 |
|---|---|---|
| 玩家 | `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter` | 保持父类 `ASlimeWarCharacter`；相机/碰撞由 C++ 配置 |
| 手雷 | `ASlimeEcologyGrenade::Mesh` | 当前 C++ 直接 Spawn 静态类；要换 BP 模型需把生成类提成配置 |
| 墙体/货架 | 关卡 StaticMeshActor | `Static Mobility + BlockAll + Simple Collision` |
| 准备页俯瞰图 | `/Game/_SlimeWar/UI/T_Overhead_CityPlaza` | 新地图要制作自己的俯瞰图 |
| UI 图标/字体 | WBP 内部 | 不在 C++ 里引用 |

#### 模型配置检查清单

- 网格可见、材质正确、没有丢失引用。
- 胶囊/碰撞与视觉尺寸匹配；史莱姆不要用网格碰撞。
- 站点点位在地面/NavMesh 上，标签朝相机可读。
- 矿点库存标签不被遮挡，库存变化时仍可读。
- 墙体是 Static + BlockAll，否则蓄力撞墙分裂不触发。
- 手雷/货物/粘液不参与 Pawn 阻挡，避免把玩家或史莱姆卡住。
- 新地图的 `PaintWorldSize`、俯瞰图比例、落点 marker 三者一致。

---

## 4. 玩家配置

### 4.1 类和流程

| 项 | 当前值 |
|---|---|
| GameMode | `ASlimeWarGameMode` |
| PlayerController | `ASlimeWarPlayerController` |
| DefaultPawn | `/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter`（父类 `ASlimeWarCharacter`） |
| GameState | `ASlimeRunGameState` |
| 玩家 GAS | 仅玩家启用 ASC，生命由 AttributeSet 权威，HealthComponent 镜像 |

### 4.2 RunConfig 玩家参数（两张关卡相同）

| 参数 | 值 |
|---|---:|
| `PlayerMaxHealth` | 100 |
| `PlayerMoveSpeed` | 600 cm/s |
| `PlayerSprintSpeed` | 900 cm/s |
| `PlayerAimSpeed` | 300 cm/s |
| `PlayerJumpSpeed` | 700 cm/s |
| `PlayerGravity` | 2000 cm/s² |
| `PlayerTurnRateDegPerSec` | 500 |
| `PlayerAcceleration` | 2048 cm/s² |
| `PlayerBrakingDeceleration` | 2048 cm/s² |
| `HitProtectionDuration` | 0.6 s |

瞄准速度覆盖冲刺速度：冲刺中按右键会降到 300。

### 4.3 相机

| 参数 | 值 |
|---|---:|
| `CameraBoomLength` | 220 cm |
| `CameraAimBoomLength` | 220 cm |
| `CameraSocketOffset` | (0, 60, 40) cm |
| `CameraFieldOfView` | 0（引擎默认 90°） |
| `CameraAimFieldOfView` | 55° |
| `MuzzleOffsetLocal` | (40, 25, -15) cm |
| `AimAssistMaxAngleDeg` | 6° |

`Q` 换肩把 Socket Y 在 `+60 / -60` 间切换。瞄准主要改变 FOV；当前瞄准臂长与常态相同。

### 4.4 操作

| 输入 | 行为 |
|---|---|
| `WASD` | 移动 |
| `Shift` | 冲刺 |
| `Space` | 跳跃；仅落地可跳 |
| 鼠标左键 | 按住蓄力，松开射击 |
| 鼠标右键 | 瞄准减速 + 磁性瞄准 |
| `R` | 换弹 |
| `Q` | 换肩 |
| `Escape` | 暂停 |
| `1` / `2` | 选择同化雷 / 旋风雷 |
| `Z` / `X` / `C` | 同化雷目标阵营 A/B/C |
| `G` | 投掷当前手雷 |

### 4.5 玩家 GAS 与受伤

- 受击后 0.6 秒无敌，通过 `GA_HitProtection` / `GE_Invulnerable`。
- `OnPlayerDamaged` 广播伤害方向。
- 回合非 Running 时输入被门控，但暂停键仍可用。
- 死亡会取消能力、锁定移动、广播 `OnPlayerDied`，进入结算。

---

## 5. 武器与手雷

### 5.1 当前步枪 `DA_Weapon_Rifle`

| 参数 | 值 |
|---|---:|
| 基础伤害 | 8 |
| 射击间隔 | 0.22 s |
| 弹匣 | 3 |
| 换弹 | 1.0 s |
| 射程 | 1800 cm |
| 蓄力开始 | 0.18 s |
| 满蓄力 | 1.2 s |
| 满蓄力击退半径 | 400 cm |
| 满蓄力速度 | 1400 cm/s |
| 直接推力 | 400 cm/s |
| 磁性捕获 | 屏高 10% |
| 保留半径 | 1.35 倍 |
| 换锁阈值 | 55% |
| 扫描间隔 | 0.05 s |

蓄力公式：

```text
s = clamp((hold - 0.18) / (1.20 - 0.18), 0, 1)
damage = 8 * (1 - s)
```

满蓄力伤害为 0，但击退最大。命中/击退会打开待眩晕、撞墙分裂窗口和融合打断。

### 5.2 手雷

| 参数 | 同化雷 | 旋风雷 |
|---|---:|---:|
| 每局库存 | 3 | 3 |
| 投掷间隔 | 0.7 s | 0.7 s |
| 投掷速度 | 1300 cm/s | 1300 cm/s |
| 上抛速度 | 400 cm/s | 400 cm/s |
| 重力 | 2000 cm/s² | 2000 cm/s² |
| 最长飞行 | 2 s | 2 s |
| 效果半径 | 450 cm | 600 cm |
| 持续时间 | - | 4.5 s |
| 吸引加速度 | - | 1200 cm/s² |
| 速度上限 | - | 450 cm/s |
| 中心停止半径 | - | 20 cm |

同化雷改变阵营、站点归属、颜色，并清理融合/战斗/资源记忆。
旋风雷只提供推力与短暂接触融合资格，不自动添加玩家撮合标记。

---

## 6. 史莱姆配置

### 6.1 运行时结构

唯一运行时类：`ASlimeEcologyEnemy`（继承 `ASlimeEnemyBase`）。

| 组件 | 职责 |
|---|---|
| `USlimeEcologyBrainComponent` | 0.25 s 感知、记忆、行为优先级、采集/返程/探索 |
| `USlimeLocomotionComponent` | 导航路径、局部转向、排斥、避让 |
| `USlimeBodyInteractionComponent` | 推挤、外力、阻尼、停稳眩晕、撞墙分裂 |
| `USlimeEcologyFusionComponent` | 自主/接触/位移融合、冷却、打断、眩晕合并 |
| `USlimeCoreComponent` | 中性/敌对核心、追击、跃击、吞噬、引爆 |
| `USlimeWeakpointComponent` | mass 7/8 弱点 |
| `USlimeAlertComponent` | 警戒、预警、传播、资源争夺 |
| `USlimeEcologyAIController` | 只负责 C++ 控制器与寻路，不使用 StateTree |

敌人不挂 ASC；生命由 `USlimeHealthComponent` 权威管理。

### 6.2 体量 / 生命 / 半径（当前 `DT_SlimeStats`）

| 体量 | 最大生命 | 胶囊半径 cm | 普通身体分 | 敌对半分 | 可推动 |
|---:|---:|---:|---:|---:|---|
| 1 | 20 | 20.0 | 4 | 2 | 是 |
| 2 | 40 | 25.5 | 8 | 4 | 是 |
| 3 | 60 | 31.5 | 13 | 6 | 是 |
| 4 | 80 | 37.0 | 20 | 10 | 否 |
| 5 | 100 | 43.0 | 29 | 14 | 否 |
| 6 | 120 | 48.5 | 40 | 20 | 否 |
| 7 | 140 | 54.5 | 53 | 26 | 否 |
| 8 | 160 | 60.0 | 68 | 34 | 否 |

> 当前实现是 `MaxHealth = 20 × mass`。策划案里的 `30 × mass` 尚未落地，属于已知数值差异；
> 改表会影响击杀节奏和 150/180 秒目标分，先由策划确认再改。

`DT_SlimeStats` 的 `MoveSpeed=300` 不是实际行为速度。实际速度由 `DA_EcologyRules` 按行为决定：
探索/返程 115、玩家避让 110、警戒撤离 154、核心追击 380、跃击 1050。

`KillScore` 列当前不参与生态计分；生态计分使用 6.8 的身体分公式。

### 6.3 视觉资源

| 项 | 当前 |
|---|---|
| 默认身体网格 | `/Game/Characters/Slime/SM_Slime_Base` |
| 网格类型 | StaticMesh，挂在 `BodyMesh`，无碰撞 |
| 碰撞 | 胶囊，`BodyRadius` 见上表 |
| 贴身网格 | 按胶囊半径自动缩放并贴地，底部与胶囊底对齐 |
| Cargo 视觉 | `ASlimeCargoVisual`，默认立方体，缩放随数量增加 |
| 阵营颜色 | A 青绿 / B 橙黄 / C 紫 |
| 弱点 | mass 7/8，半径 30 cm、高度 32 cm |

美术替换建议：

- 在 `DT_SlimeStats` 每行覆盖 `Mesh`，或改 `ASlimeEnemyBase::DefaultBodyMesh`。
- 保持网格原点/包围盒合理；代码按水平包围盒自动缩放，不依赖固定网格尺寸。
- 需要阵营材质时，优先在网格材质实例里暴露颜色参数，由表现层按 `Faction` 切换。
- 站点垫片、矿点晶体、标签可以在 `ASlimeFactionSite` / `ASlimeMineralPatch` 的 BP 子类里替换占位组件。
- Cargo 目前由 C++ 生成 `ASlimeCargoVisual`；若要换模型，改 `ASlimeCargoVisual` 的 `CargoMeshAsset` 或提供 BP 子类配置。

### 6.4 感知、移动与探索

| 参数 | 当前值 |
|---|---:|
| 感知间隔 | 0.25 s |
| 同族感知半径 | 700 cm |
| 视线高度容差 | 200 cm |
| 探索速度 | 115 cm/s |
| 聚群速度 | 115 cm/s |
| 玩家避让进入/退出 | 240 / 320 cm |
| 玩家避让速度 | 110 cm/s |
| 群锚点退出/进入 | 280 / 180 cm |
| 排斥额外间距 | 25 cm |
| 排斥权重 | 3.0 |
| 转向平滑 | 8.0 |
| 移动加速度 | 1200 cm/s² |
| 刹车减速度 | 1800 cm/s² |
| 转身速度 | 360°/s |
| 探索半径 | 300–600 cm |
| 探索目标最小位移 | 150 cm |
| 探索到达半径 | 70 cm |
| 探索选点间隔 | 12 s |
| 探索候选数 | 12 |
| 路径刷新 | 0.5 s |
| 航点接受半径 | 40 cm |
| 卡住判定 | 2 s / 15 cm |
| 空间哈希网格 | 400 cm |
| 探索历史网格 | 400 cm |
| 重复访问惩罚 | 5 m 等效 |

行为优先级：

1. 回合冻结/死亡：暂停。
2. 核心攻击前摇、跃击、收势。
3. 受冲击恢复/瞄准锁定/眩晕：停止自主移动。
4. 携货：避玩家 → 避敌对核心 → 返程交付。
5. 空载核心攻击行为。
6. 首次警戒等待。
7. 避敌对核心。
8. 避玩家（240 进入 / 320 退出，滞回）。
9. 已建立融合连接（优先于普通资源行为）。
10. 采集/拾取/返程。
11. 粘液趋近（仅采集休息期间）。
12. 警戒避敌/聚群。
13. 探索。

### 6.5 身体、推挤、眩晕与分裂

| 参数 | 当前值 |
|---|---:|
| 可推动最大体量 | 3 |
| 身体推动基础速度 | 260 cm/s |
| 玩家推动速度比例 | 0.4 |
| 推动体量指数 | 0.35 |
| 接触额外间距 | 20 cm |
| 外力阻尼 | 500 cm/s² |
| 停稳速度 | 10 cm/s |
| 停稳时间 | 0.1 s |
| 命中待眩晕 | 1.5 s |
| 分裂眩晕 | 2 s |
| 玩家撮合窗口 | 3 s |
| 撞墙分裂最低速度 | 600 cm/s |
| 撞墙分裂窗口 | 0.9 s |

命中眩晕只在停稳后开始计时；待眩晕/眩晕期间保留重力、碰撞与外力滑动。
体量 1–3 可被玩家身体推动，体量 4–8 阻挡玩家。

### 6.6 融合

| 参数 | 当前值 |
|---|---:|
| 自主准备 | 14–22 s |
| 自主寻伴半径 | 400 cm |
| 自主靠近速度 | 75 cm/s |
| 接触时间 | 2.4 s |
| 接触容差 | 18 cm |
| 位移撮合容差 | 8 cm |
| 成功冷却 | 1.8 s |
| 打断禁用 | 5 s |
| 自主融合体量上限 | 2 |
| 总融合体量上限 | 8 |
| 寻伴超时 | 10 s |
| 射击附加融合冷却 | 0.8 s |

三条路径：

1. 自主寻找：双方准备完成、同阵营、无 cargo、体量和 ≤2，靠近后接触 2.4 秒。
2. 接触连接：物理接触后开始 2.4 秒计时。
3. 位移融合：上一位置到当前位置的扫掠线段命中同阵营个体，立即提交；允待眩晕，受分裂平复、成功冷却和射击冷却快照限制。

融合保留剩余生命比例、Cargo、BodyScoreCredit，眩晕取双方较长值。

### 6.7 核心、跃击、吞噬与引爆

| 参数 | 当前值 |
|---|---:|
| 苏醒 | 1.2 s |
| 感知半径 | 900 cm |
| 追击速度 | 380 cm/s |
| 战斗记忆 | 6 s |
| 追离距离 | 1900 cm |
| 近身玩家候选 | 240 cm |
| 玩家优先切换 | 180 cm |
| 玩家受激记忆 | 6 s |
| 低血撤退 | <30%，2.5 s |
| 攻击间隔 | 1.6 s |
| 前摇 | 0.65 s |
| 收势 | 0.4 s |
| 基础跃击距离 | 340 cm |
| 跃击速度/时长 | 1050 cm/s / 0.4 s |
| 跃击上抛 | 300 cm/s |
| 跃击允许高度差 | 240 cm |
| 对玩家/敌族伤害 | 20 / 16 |
| 吞噬回血 | 5 / 体量 |
| 爆裂半径 | 300 + 25×mass cm |
| 爆裂伤害 | 中心 42，边缘 65% |
| 爆裂推力 | 800 / mass^0.45 |
| 核心连锁 | 只有敌对核心递归连锁；中性核心只造成普通爆裂伤害 |

### 6.8 资源、警戒、粘液和计分

资源：

| 参数 | 当前值 |
|---|---:|
| 资源感知 | 800 cm |
| 远距离回退 | 2000 cm |
| 资源记忆 | 28 s |
| 目标保留 | 2 s |
| 采集距离/时间 | 120 cm / 2 s |
| 交付距离 | 130 cm |
| 拾取距离/动作距离 | 110 / 100 cm |
| 携货速度 | 115 cm/s |
| 有资源记忆探索速度 | 90 cm/s |
| 初始采集休息 | 0.1–1.2 s |
| 采集后休息 | 2–4 s |
| 矿点拥挤半径/代价 | 140 cm / 1.8 m |
| 交付增员 | 1 份 → 1 只体量 1 |
| 每帧生成请求 | 8 |
| 生成圈 | 250–400 cm |
| 玩家安全距离 | 270 cm |

警戒/争夺：

| 参数 | 当前值 |
|---|---:|
| 预警 | 1 s |
| 警戒持续 | 10 s |
| 传播半径 | 500 cm |
| 警戒撤离速度 | 154 cm/s |
| 敌族感知 | 600 cm |
| 携货/空载避核心 | 220 / 180 cm |
| 警戒后空载采集休息 | 4 s |
| 争夺距离/累计/冷却 | 340 cm / 1.5 s / 5 s |

粘液：

| 参数 | 当前值 |
|---|---:|
| 粘液感知 | 600 cm |
| 接近速度 | 65 cm/s |
| 停止半径 | 50 cm |
| 粘液 RT | 700×1024 |
| 世界尺寸 | 3000×4400 cm |
| 最大 splat 记录 | 2048 |

计分（只对玩家造成的击杀给分）：

```text
Body = max(mass × 4 + max(mass - 2, 0)^2, BodyScoreCredit)
Full = Body + Cargo × 6
Award = HostileCore ? floor(Full × 0.5) : Full
NPC 击杀 = 0 玩家分
```

终局：时间到、玩家死亡、生态灭绝（无存活个体、无待生成、无地面货物）。
达到目标分不会提前结束。

---

## 7. 已知差异与调整边界

以下差异已在 `Docs/EcologyRebuild/DesignConsistencyReport.md` 中登记。本文只记录，不代表本轮要改：

- 当前史莱姆生命 `20×mass`，策划案目标 `30×mass`。
- 当前相机常态 FOV 使用引擎默认 90°，策划案目标 75°。
- 当前相机臂长 220 cm，策划案目标 400 cm。
- 当前玩家水平加速度 2048 cm/s²，策划案目标 20 m/s²。
- 当前身体推动使用 500 cm/s² 阻尼，没有独立的 18 m/s² 推动加速度。
- 当前吞噬回血 5/体量，策划案目标 15。
- 当前跃击高度容差 240 cm，策划案目标 140 cm。
- 当前携货速度 115 cm/s，策划案目标 135 cm/s。
- 当前位移撮合 8 cm，未采用策划案的 0.65 m 捕获间距和 0.45 m/s 连接拉近。
- 当前核心追离 1900 cm，策划案单点/双点目标为 1500/1900 cm。

调整原则：

- UI/美术不要为了视觉方便改这些数值；需要改时由策划确认，并同步更新本文件和一致性报告。
- 调整节奏优先改 `DA_RunConfig_*` 与 `DA_EcologyRules`，不要把数值写死到 WBP。
- 新增关卡只加资产和 `+Levels=`，不改 C++ 流程。
- 新增 UI 只换 WBP 类和事件实现，不要重新打开 C++ Canvas HUD。

---

## 8. 调试与验证命令

局内控制台常用：

```text
SlimeRunStart
SlimeRunTimeScale 0
SlimeEcologyStatus
SlimeEcologyGroundProbe
SlimeEcologyResourceStatus
SlimeEcologyResourceLedgerCheck
SlimeEcologySoakTest 60
SlimeEcologyPriorityTest
SlimeEcologyR5Test
SlimeEcologyExternalConnectionTest
SlimeEcologyShotCooldownTest
SlimeEcologyCorePriorityTest
SlimePlayerControlStatus
SlimePaintTest
SlimeGrenadeStatus
```

调试 HUD：

```text
Slime.Debug.DrawRun 1
Slime.Debug.DrawEcologyState 1
Slime.Debug.Crosshair 1
Slime.Debug.DrawAimAssist 1
Slime.Debug.DrawFusion 1
```

导航烘焙（编辑器控制台）：

```text
SlimeBakeNav
```

本轮关闭 C++ HUD 的验证：

- 编译：`SlimeWarEditor Win64 Development` 0 error。
- 实机日志：`Saved/Logs/UIOff-final.log`。
- 截图：`Saved/Screenshots/WindowsEditor/HighresScreenshot00122.png`。
- 截图中无 C++ 分数、站点、矿点、史莱姆状态文字。

