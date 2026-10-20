# UI 执行清单（代码 + WBP）

- 适用日期：2026-10-20
- 用途：把 `UI_改动补丁_框架对齐.md` 的差异落成**逐文件、逐页面、可勾选**的执行单。音频部分不在这里，见 `Docs/音效/音频接入执行清单.md`。
- 文档链：`ResultPage_Implementation_Plan.md` / `UI_Settings_Implementation.md`（原方案）→ `UI_改动补丁_框架对齐.md`（差异说明）→ **本文（执行单）**
- 状态图例：`[ ]` 未开始 / `[~]` 进行中 / `[x]` 完成 / `[!]` 阻塞

---

## 0. 开工前

- [ ] `git commit` 或 `git stash`（第 1 批会同时动 UI 与音频相关文件，留回滚点）
- [ ] 编译目标 `SlimeWarEditor Win64 Development`（UE 5.5）
- [x] `BP_ThirdPersonCharacter`：Mesh = `SK_Player_Body`，Anim Class = `ABP_Player_C`
- [x] `DefaultGame.ini` 的 `Levels` 已删旧白盒槽 → 现在只有 `DoublePoint`(0) / `ThreePoint`(1)
- [x] 两张 `DA_RunConfig_*` 的 `bAutoStartRun = false`
- [x] 音量只做总音量（`all`）
- [x] AimPreview 走 C++ 基类，WBP 只做 Reparent

> 已确认的连带结论：白盒槽删掉后，三点关 `HasNextLevel()` 返回 false，“下一关”按钮在该局自动变灰，不需要额外分支。

---

## 1. 批次总览

> **跨域执行顺序看 `Docs/接入执行总计划_UI与音频.md` 的 M0~M9**：UI 第 1 批与音频第 1 批合并为 M0，本文的 U 编号不变，只是被 M 编号引用。

| 批次 | 执行人 | 内容 | 前置 |
|---|---|---|---|
| U1 | 程序（我） | U-C1 ~ U-C7（新建 3 类，改 5 文件） | 无 |
| U1' | 你 | 编译，报错回贴 | U1 |
| U2 | 你 | WBP：U-W1 ~ U-W8 | U1' |
| U3 | 你 | 编辑器/关卡：U-A1 ~ U-A3 | U1' |
| U4 | 你 | 验收（本文 §5） | U2、U3 |

U1 与音频第 1 批**共用同一批文件**（`SlimeUISubsystem`、`SlimeGameSettings`），所以会一次性交付，不分成两轮。

---

## 2. 程序侧（我）

> **2026-10-20 M0 已交付**：下表 `U-C1 ~ U-C7` 的代码全部写完（含 `USlimeAimPreviewWidget` 与红屏），
> 但**尚未编译验证**——状态以 M0'（编译通过）为准。下面的 `[ ]` 保留给“编译后人工确认”用。

| 编号 | 文件 | 内容 | 状态 |
|---|---|---|---|
| U-C1 | `GameplayFramework/SlimeGameSettings.h/.cpp` | `IndexOfLevel(FName)`；`AudioConfig` 软引用**已移除**（见下） | `[ ]` |
| U-C2 | 新建 `UI/SlimeResultCamera.h/.cpp` | SceneCapture + 运行时 RT + 阶段驱动拍照 | `[ ]` |
| U-C3 | `UI/SlimeResultWidget.h/.cpp` | `GetResultPhoto` / `OnResultShown` / `HasNextLevel` / `RequestNextLevel` / `RequestLevelSelect` | `[ ]` |
| U-C4 | `UI/SlimeUISubsystem.h/.cpp` | 四个转发 + AimPreview 创建与显隐 + BGM 三挂点 | `[ ]` |
| U-C5 | `UI/SlimeSettingsWidget.h/.cpp` | 0×0 兜底 / 返回兜底 / 分辨率列表 / 音量真实现 | `[ ]` |
| U-C6 | 新建 `UI/SlimeAimPreviewWidget.h/.cpp` | 四控件绑定 + Tick 驱动环与弱点准星 | `[ ]` |
| U-C7 | `Player/SlimeWarCharacter.h/.cpp` | `UPostProcessComponent` + 受伤闪红 / 低血常亮 | `[ ]` |

### U-C1 细节

- `IndexOfLevel`：遍历 `Levels`，`LoadSynchronous()` 后比较 `LevelId`，找不到返回 `INDEX_NONE`。
- ~~`AudioConfig`：`TSoftObjectPtr<USlimeAudioConfig>`，前向声明，`.cpp` 里 include 音频头，ini 键名 `AudioConfig=`~~
  **已作废（2026-10-20）**：音频 C++ 层整体删除，`USlimeAudioConfig` 类与 `AudioConfig` 字段都已在代码中移除，
  `Config/DefaultGame.ini` 里的 `AudioConfig=` 也删了。**不要再往 `USlimeGameSettings` 里加这个字段**，
  蓝图直接选事件资产即可（见 `Docs/音效/UI音频接入方案.md` §1.1）。
- 注意：`IndexOfLevel` 是“下一关”的唯一依据，**不要**改用关卡名或数组下标写死。

### U-C2 细节

- 头文件依赖：`Camera/CameraActor.h`、`Components/SceneCaptureComponent2D.h`、`Engine/TextureRenderTarget2D.h` —— 都在 `Engine` 模块，**`SlimeWar.Build.cs` 不加新依赖**。
- 参数：`CaptureWidth=1024`、`CaptureHeight=640`、`PhotoDelay=0.3`、`RenderTargetOverride` 留空。
- 时机：`Result` 阶段延迟 `PhotoDelay` 拍一次；`Ended` 时若未拍过则补拍。
- 格式：`RTF_RGBA8_SRGB`（否则 UI 里发灰）。
- 不做：不读 `LS_Result`、不参与运镜、不碰 view target。

### U-C3 细节

- `OnResultShown()` 仅在阶段 == `Ended` 时触发；`RefreshResult()` 改为先查阶段再决定调不调。
- `GetResultPhoto()`：`ASlimeResultCamera::Get(World)` → `GetPhoto()`；相机不存在返回 null（WBP 回退 `T_Result_BG`）。
- `RequestLevelSelect()`：转发 `USlimeUISubsystem::RequestLevelSelect()`（只切屏）。
- `HasNextLevel()` / `RequestNextLevel()`：转发子系统同名函数。

### U-C4 细节

- `RequestLevelSelect()`：`ApplyMenuState(LevelSelect)`；`LevelSelectWidget` 缺失时退回 `ReturnToMainMenu()` 并打 Warning。
- `RequestNextLevel()`：`SetSelectedLevelId` → `ClearSelectedDropPoint` → `RequestEnterGameplay` → `OpenLevel`；任一前置失败直接 `return false`，**不允许留下“进游戏”标志**。
- `HasNextLevel()`：`FindLevelByIndex(Current+1)` 且 `MapAsset` 非空。
- AimPreview：`EnsureWidgets()` 里创建（类走 `USlimeGameSettings::AimPreviewWidgetClass`，新增一个软引用字段），`ApplyPhaseToScreens()` 里 `Running` 显示 / 其余折叠。
- BGM：`ApplyMenuState()` → `Start`；`ApplyPhaseToScreens()` → `Idle:Start`、`Deploying:Game`、`Ended:End`；`SetPauseState()` → 只 `DuckBgm`。

### U-C5 细节

- `ResolveValidResolution()`：① `GetScreenResolution` 两维>0 ② `GetDesktopResolution` ③ 支持列表取面积最大 ④ 兜底 `1280×720`，每级打日志。
- 三处接入：`NativeConstruct`、`ResetSettings`（`SetToDefaults()` 之后）、`ApplySettings`（下发前）。
- `GetSupportedResolutions()`：过滤非正 → 追加当前与桌面并去重 → 按面积/X/Y 稳定排序。
- `ReturnToPreviousMenu()`：`ReturnWidget` 失效时 `USlimeUISubsystem::ShowMainMenu()`（**不是** `ReturnToMainMenu`，后者会重载关卡）。
- ~~`ApplyMasterVolumeToAudio()`：转调 `USlimeAudioSubsystem::SetMasterVolume`~~
  **已删除（2026-10-20）**：该函数连同 4 处调用一起从 `SlimeSettingsWidget` 移除。
  音量改为在 `WBP_SlimeSettings` 的 `On Settings Values Changed` 里推 `Set RTPC Value`（`all`），
  见 `Docs/音效/UI音频接入方案.md` §4.1。**`PendingMasterVolume` 的存盘逻辑没变**，不用动。

### U-C6 细节

- `BindWidgetOptional`：`Img_ChargeRing_Fill`（`UImage`）、`Text_ChargePercent`（`UTextBlock`）、`Img_Crosshair_Default`、`Img_Crosshair_Weakpoint`。
- Tick：取 `GetOwningPlayerPawn()` → Cast `ASlimeWarCharacter`：
  - `IsCharging()` → `GetChargeAlpha()` 写材质参数 `ChargePercent`，文本 `0~100%`；
  - 不充能 → 参数归 0、文本 `0%`、环 `Collapsed`；
  - `GetAimAssistComponent()->IsWeakpointLocked()` → 切两个准星的显示。
- 材质：`GetDynamicMaterial()` 首次调用自动建 MID，不手动 Create。
- 性能：每帧两个纯函数 + 一次 `SetVectorParameterValue`，可接受；不要在里面查 GameState。

### U-C7 细节

- 构造里 `CreateDefaultSubobject<UPostProcessComponent>`，`bUnbound = true`。
- `BeginPlay` 从 `Settings.Blendables` 里找材质为 `M_HurtIndicator_PP` 的项建 MID；找不到只打一次 Warning（不刷屏）。
- `OnPlayerDamaged` → `Intensity = 0.8`，`0.12s` 内线性回 0（Tick 或 Timer）。
- 低血：`GetHealthComponent()->GetHealth()/GetMaxHealth() < 0.3` 时压到 `0.15` 常亮，回血/结束归零。
- 暂停（`GlobalTimeDilation=0`）时世界 Tick 停住，衰减自动暂停，不需要特判。

---

## 3. WBP 侧（你）

> 前置：U1' 编译通过，蓝图里能搜到本文 §2 列出的节点。
> **与音频清单共用同一批 WBP，请按 §6 的顺序一次改完，不要跑两遍。**

### U-W1 `WBP_SlimeResult`（最重，按七步走，每步可单独验）

- [x] **1 数字**：`Event On Result Ready(Result)` → Break → 填 5 个 Text：

| 控件 | 字段 | 备注 |
|---|---|---|
| `Text_FinalScore` | `Result.Score` | — |
| `Text_BestScore` | `Result.BestScore` | 未达标时加小字“本局不计入最高分” |
| `Text_TargetScore` | `Result.TargetScore` | **删掉硬编码** |
| `Text_CoreKills`（原清点栏） | `Result.HostileKills` | 标签改“核心击杀” |
| `Text_Kills` | `Result.NormalKills` | 标签“击杀数” |

- [x] 已被弃用，无需做：**1b 结束原因**：`Branch(EndReason)` 三分支文案——`TimeUp`“时间到”、`PlayerDied`“阵亡”、`Extinction`“生态被清空”。
- [x] **2 进度条**：蓝 = `Clamp(Score/Target,0,1)`，黄 = `Clamp((Score-Target)/150,0,1)`；**全部走浮点**；`Target<=0` 两段设 0。
  采样自检：双点 `0/100/200/275/350` → 蓝 `0/50/100/100/100`、黄 `0/0/0/50/100`。

```
Event On Result Ready (Result)
  ├─ Set CachedScore      = Result.Score
  ├─ Set CachedTargetScore = Result.TargetScore
  └─ （其余数字、进度条也在这里写）

Event On Result Shown
  └─ Score Updated (Score = CachedScore, Target Score = CachedTargetScore)
```

```
RefreshResult()
  ├─ OnResultReady(Result)   ← 先，带真值
  └─ OnResultShown()         ← 后，读缓存一定是真值
```



- [x] **3 印章**：`Branch(Result.bPassed)` → `T_Stamp_Passed` / `T_Stamp_NotPassed`，默认 `Collapsed`，在 `On Result Ready` 里由 WBP 自己打开。

  > **答“印章是哪段程序调的”**：**C++ 完全不碰印章**。程序只负责算出 `FSlimeRunResult::bPassed` 的值（`SlimeFlowMath::ComputeRunPassed`：只有 `EndReason == TimeUp` 且 `Score >= TargetScore` 才为 true），切换贴图和显隐全部由 WBP 在这条事件里完成。所以“印章不出来”永远是 WBP 这条分支的问题，不是 C++。

- [x] **4 照片**：**`On Result Ready`**

  > **前置**：`M_ResultPhoto` 必须先存在（§4 的 `U-A0`），并在关卡里摆了 `ASlimeResultCamera`（`U-A1`）。
  > 两样缺任意一个，`Get Result Photo` 都是 null，页面只会显示兜底图——看起来像“照片逻辑写错了”。

  `Get Result Photo` → `IsValid?`

  - 有效：`Img_Photo` 的 `Get Dynamic Material` → `Set Texture Parameter Value("Photo", 图)`，显示 `Img_Photo`、折叠 `Img_ResultBG`；
  - 无效：反之（回退 `T_Result_BG`）。

```
Canvas (或设计里的照片槽)
└─ SizeBox_Photo            ← 固定尺寸，Width/Height Override 必须勾选并填值
   └─ Overlay               ← 兜底图和照片同一块区域叠放
      ├─ Img_ResultBG       ← T_Result_BG，铺满，常驻
      ├─ Img_Photo          ← M_ResultPhoto，接 RT
      └─ Img_Frame (可选)    ← 相框装饰，放最上层
```



- [x] **5 按钮**：
  - `Btn_Restart` → `Request Retry`
  - `Btn_NextLevel` → `Request Next Level`，并在 `On Result Ready` 末尾 `Set Is Enabled(Has Next Level)`
  - `Btn_SelectLevel` → `Request Level Select`（**不要**接 `Request Quit`）
- [ ] **6 音与动画**：新建 `Event On Result Shown`，淡入/数字滚动/印章弹出全部挂这里；**不要挂 `OnResultReady`**（后者关卡一开始就会带全 0 触发）。结算音见音频清单 B3。
- [ ] **7 自检**：`SlimeRunSetTime 1`（空局）/ `SlimeRunAddScore 200`（达标）/ `SlimeKillPlayer`（死亡）各跑一次。

### U-W2 `WBP_SlimeLevelSelect`

- [ ] 卡片 0/1 分别 `SelectLevel(0/1)`，在 `OnLevelSelectionChanged(Index, bAvailable)` 里做选中态
- [ ] 卡片用 `IsLevelAvailable(Index)` 判锁定：不可用时置灰；**锁定卡不发声**（不可交互，官方事件表没有"锁"音）
- [ ] 卡片 `OnHovered` → `Play UI Sound(Choose1)`；`OnPressed` → `Play UI Sound(Choose2)`（两个不同事件，别合成一个）
- [ ] 进入按钮 → `EnterSelectedLevel()`
- [ ] 返回按钮 → `ReturnToMainMenu()`（页面内的，只切屏）
- [ ] 槽位 2/3 留空节点可保留，但必须走 `IsLevelAvailable` 分支（否则点进去没反应）

### U-W3 `WBP_SlimePreparation`

> **先分清两个“点”**（每加一张图都会再问一次）：
>
> | 概念 | 是什么 | 在哪配 | 双点关 | 三点关 |
> |---|---|---|---|---|
> | **站点 / Site** | 史莱姆的阵营据点 | 关卡里的 `ASlimeFactionSite` Actor | 2 个（A/B） | **3 个（A/B/C）** |
> | **落点 / DropPoint** | **玩家**开局的投放位置 | `DA_SpawnLayout_*` 的 `DropPoints` 数组 | 2 个（D1/D2） | **2 个（D1/D2）** |
>
> 名字里的“三点”指**阵营站点**，不是落点。所以两关的准备页都只有 **2 个 marker**，`GetDropPointCount()` 都返回 2。

- [ ] 核对 marker 数量与 `GetDropPointCount()`（双点 2 个、三点 2 个）
- [ ] 每个 marker `Bind Event to On Button Clicked` → `Preview Drop Point(N)`
- [ ] `Event On DropPointPreviewed` → 互斥高亮（选中一个、另一个 `SetNormal`）→ `Btn_Go.Set Is Enabled(true)`
- [ ] `Btn_Go` → `Confirm Previewed Drop Point`
- [ ] `Btn_Back` → `Request Back To Main Menu`
- [ ] `Btn_Go` 默认 disabled

### U-W4 `WBP_SlimeHUD`

- [ ] 特殊子弹：`On Grenade Inventory Updated(A, V)` → `Text_Special_Assimilate_Ammo = ×A`、`Text_Special_Vortex_Ammo = ×V`；为 0 时对应 icon 置灰
- [ ] `On Grenade Selection Updated(Type, Faction)` → 高亮当前 icon、压暗另一个（**第二个参数是阵营，不是数量**）
- [ ] 核对已接：分数/时间/生命/弹匣/站点/`ShowTutorial`
- [ ] 删除对 `OnPointStateUpdated` / `OnBatchIncoming` 的依赖（当前运行时不会调用）

> **数量不要写死**：初始 3 枚来自角色（`ASlimeWarCharacter::AssimilationGrenades / TornadoGrenades`，默认各 3），WBP 只显示事件给的数字。容器是 `HB_Special_Weapon`（复用弹药面板布局），程序不引用这个容器名，改名不影响代码。

### U-W5 `WBP_SlimePause`

- [ ] 四个按钮 → `Resume` / `Request Retry` / `Request Reselect Drop Point` / `Request Quit`
- [ ] 不需要自己监听 Esc（`USlimeUISubsystem` 负责）

### U-W6 `WBP_SlimeSettings`

- [ ] `Event Construct` 初始化顺序：`bUpdatingUI=true` → 填分辨率选项（`GetSupportedResolutions` + 存 `ResolutionOptions`）→ 填模式三项（窗口化/全屏/无边框）→ 反查选中 → 刷新无边框禁用 → `bUpdatingUI=false`
- [ ] 两个下拉的 `OnSelectionChanged`：先判 `bUpdatingUI`，再判索引有效
- [ ] `Slider_Volume`：`Value = GetMasterVolume()` 回填，`Is Enabled` 打开，`OnValueChanged → Set Master Volume`
- [ ] 三键：应用 → `Apply Settings`、重置 → `Reset Settings`、返回 → `Return To Previous Menu`
- [ ] 自检：不是 `0 x 0`；无边框时分辨率下拉变灰；重置后不是 `0 x 0`

### U-W7 `WBP_AimPreview`

- [ ] **Reparent** 到 `SlimeAimPreviewWidget`（File → Reparent Blueprint）
- [ ] 核对四个控件名（大小写敏感，不匹配不报错但不生效）：`Img_ChargeRing_Fill`、`Text_ChargePercent`、`Img_Crosshair_Default`、`Img_Crosshair_Weakpoint`
- [ ] `Img_ChargeRing_Fill` 的 Brush 用 `MI_ChargeRing`，`Draw As = Image`
> **270° 环的边界**：环的“四分之三、从左下顺时针填充、底部缺口放文字”全部由 `M_ChargeRing` 的 Custom 节点决定，程序只推 `ChargePercent`（0~1）。
> 所以 `Img_ChargeRing_Fill` **必须是 Image + MI_ChargeRing**，不要换成 `ProgressBar`（那会变成整圈/直线填充，且不吃这个参数）；
> 也不要在 WBP 里写 Tick 去改它，C++ 已经在驱动（两处同时改会互相覆盖）。
> `ChargePercent` 的初始值由 C++ 在 `NativeConstruct` 里强制设 0，MI 里的默认值配错不影响。
- [ ] `Img_Crosshair_Weakpoint` 默认 `Collapsed`
- [ ] 不要自己在 Graph 里写 Tick（C++ 已接管），否则两处同时改会打架

### U-W8 `WBP_GenericButton`

- [ ] 与音频清单 B2.1 共用：`On Button Clicked` 首位置插 `Play UI Sound(Click)`
- [ ] **只做一次**，不要在四个页面里给同一实例重复绑

---

## 4. 编辑器 / 关卡（你）

> 这一节全是 `.uasset` 操作，**只能你手动做**。顺序：先建材质（4.1）→ 再摆相机（4.2）→ PP（4.3）→ 设置（4.4）。

### 4.1 资产前提：`M_ResultPhoto` 必须先建（U-A0）

照片走的是“RT → 材质 → Image”这条路，**没有这个材质，照片永远不显示**，而且不会报错，只会走兜底分支。

- [ ] **U-A0 新建 `M_ResultPhoto`（约 2 分钟）**

  1. Content Browser → `/Game/_SlimeWar/UI/` → 右键 → **Material** → 命名 `M_ResultPhoto`
     （老计划写的是 `UI/Materials/` 子目录，放哪个目录都行，只要 WBP 里 `Img_Photo` 的 Brush 指向同一个资产）
  2. 双击打开 → `Material Domain = User Interface`、`Blend Mode = Opaque`
  3. 空白处右键 → 搜 `TextureSampleParameter2D` 拖进来 → 参数名改成 **`Photo`**（大小写敏感）
  4. 采样节点的 `RGB` → 输出节点的 `Emissive Color`
  5. 保存

其余资产应该都已存在，缺了会“静默失效”：

| 资产 | 路径 | 缺了会怎样 |
|---|---|---|
| `M_HurtIndicator_PP` | `/Game/_SlimeWar/UI/` | 受伤不闪红（日志一条 Warning） |
| `T_Result_BG` | `.../UI/Textures/` | 无照片时兜底图空白 |
| `MI_ChargeRing` + `M_ChargeRing` | `.../UI/` | 蓄力环不动 |
| `WBP_AimPreview` | `.../UI/` | 没有准星（还要先 reparent，见 `U-W7`） |

### 4.2 摆放结算相机（U-A1）

`ASlimeResultCamera` 是关卡里的一个 Actor，内部挂了一个 `SceneCaptureComponent2D`：`Result` 阶段 +0.3s 时用它渲染一张图到运行时创建的 RenderTarget。**它不是截屏**，所以不含 UMG，只有 3D 世界；也不需要任何资产，RT 是代码现造的。

**每张图各放一台**，没放就是“没有照片 + 走兜底”，不会崩。

- [ ] **U-A1 双点关**
  1. 打开 `L_Ecology_DoublePoint`
  2. Place Actors → 搜 `SlimeResultCamera` → 拖进视口（游戏里看不到它，它没有可见网格）
  3. 选中后按 `F` 聚焦，用移动/旋转把它摆成你想让玩家看到的结算角度（当成第二台摄影机）
  4. Details 确认：`Capture Width = 1024`、`Capture Height = 640`、`Photo Delay = 0.3`、`Render Target Override = 空`
  5. `Camera Component` 的 `Field of View` 调取景范围；**改完 FOV 后拖一下 Actor**，让 `OnConstruction` 把 FOV 同步给 SceneCapture
  6. 保存关卡

- [ ] **U-A1 三点关**：对 `L_Ecology_ThreePoint` 重复 1~6，**构图重新做，不要抄双点坐标**

> **不要对齐 `LS_Result` 末帧**（旧文档 §5.1 第 2 步已废弃）：那条序列是白盒图时代做的、两关共用一条，机位对不上。现在相机自己就是构图，序列只负责运镜。
>
> **一个关卡只放一台**。放两台时 `Get()` 取第一台并打 Warning：`2 cameras in this level; the first one is used`。

**拍不到 / 拍错了怎么查**

| 现象 | 原因 | 处理 |
|---|---|---|
| 结算页只有 `T_Result_BG`，日志无 `captured` | 没摆相机 / 摆了但没绑上 | 看日志有没有 `no ASlimeRunGameState`、`capture skipped` |
| 有 `settlement photo captured` 但全黑 | 相机在墙里/朝外/朝地下 | 换位置，朝场地中央 |
| 只拍到天空 | 俯仰角不对 | 往下压 Pitch |
| 照片被拉伸或裁切 | 框的比例不是 1.6:1 | `SizeBox` 做成 1024:640 的倍数，或改 `CaptureWidth/Height` 去匹配框 |
| 照片偏暗发灰 | RT 格式不对 | 代码已固定 `RTF_RGBA8_SRGB`，不用管 |

> 快速验证：PIE 后控制台 `SlimeRunDeploy 0` → `SlimeRunSetTime 1`，10 秒就能到结算看照片，不用打完整局。

### 4.3 受伤红屏（U-A2）

- [ ] `BP_ThirdPersonCharacter` → Components 里选中 C++ 已创建的 **PostProcess** 组件（名字 `HurtPostProcess`）
- [ ] `Rendering Features → Post Process Materials` 加一条 → 选 `M_HurtIndicator_PP`，Weight `1.0`
- [ ] 编译保存 BP

> ⚠️ **关卡里不要再摆带同一个材质的 `PostProcessVolume`**。红屏已经由角色的 `UPostProcessComponent` 提供（这样每张图都不用摆），两处都挂会让 `Intensity` 混合两遍，受击时明显比设计更红更闷。
> 已确认：`L_Ecology_DoublePoint` / `L_Ecology_ThreePoint` 目前都**没有** PostProcessVolume，所以现状无冲突。

### 4.4 Project Settings（U-A3）

- [ ] Project Settings → Game → Slime War：
  - ~~`AudioConfig` → `DA_SlimeAudioConfig`~~ **已移除（2026-10-20）**：该字段与 DA 资产都已删除，面板上不会再有这一项。音效改为蓝图直连 `Post Event`，不需要任何全局配置指向
  - `AimPreviewWidgetClass` → `WBP_SlimeAimPreview` / `WBP_AimPreview`（**必须先 reparent 成 `USlimeAimPreviewWidget`**，否则子系统会移除它并打一条 Warning）

### 4.5 美术资产的既定约定（不要改，会破坏程序对接）

| 资产 | 约定 | 谁在依赖 |
|---|---|---|
| `M_ChargeRing` | 四分之三（270°）环，从左下顺时针填充，`ChargePercent` 0~1，底部缺口放百分比文字 | `USlimeAimPreviewWidget` 只推参数，不改几何 |
| `MI_ChargeRing` | 默认 `ChargePercent = 0`（C++ 也会强制设 0，配错不影响） | 同上 |
| `T_HurtIndicator_Radial` | 2048×2048，中心透明、边缘红渐变 | `M_HurtIndicator_PP` 采样 |
| `M_HurtIndicator_PP` | 后期处理域，`SceneTexture + Lerp`，唯一参数 `Intensity` | 角色 MID 驱动 |
| `HB_Special_Weapon` | 复用弹药面板布局的容器（程序不引用名字） | `WBP_SlimeHUD` |

> 相机与 PP 组件都做了“缺失即降级”：没有相机 → 照片回退 `T_Result_BG`；没有 Blendable → 不闪红，只打一条 Warning；没有 `M_ResultPhoto` → `Get Dynamic Material` 返回 null，照片分支自然走兜底。**都不会崩。**

---

## 5. 验收（按场景）

| # | 场景 | 操作 | 期望 |
|---|---|---|---|
| 1 | 双点空局 | `SlimeRunSetTime 1` | 0 分、蓝 0%、黄 0%、未达标印章、最高分不变 |
| 2 | 双点达标 | `SlimeRunAddScore 200` → 时间到 | 蓝 100%、黄 0%、达标印章 |
| 3 | 双点超 75 | `SlimeRunAddScore 275` | 蓝 100%、黄 50% |
| 4 | 三点达标 | 进三点关，`SlimeRunAddScore 160` | 蓝 100%、黄 0% |
| 5 | 超分阵亡 | `SlimeRunAddScore 400` → `SlimeKillPlayer` | 未达标；文案“阵亡”；最高分不变 |
| 6 | 生态灭绝 | 清空史莱姆至 `Extinction` | 未达标；文案“生态被清空” |
| 7 | 核心击杀 | 击杀带敌对核心的史莱姆 | “核心击杀”+1（不是清点数） |
| 8 | 照片 | 两关各结一局 | 两张构图分别对应各自关卡 |
| 9 | 无相机 | 删掉其中一台 | 该关回退 `T_Result_BG`，不崩、不黑框 |
| 10 | 下一关 | 双点结算 | 可点 → 进三点**准备页**，落点回到未选 |
| 11 | 下一关（三点） | 三点结算 | 按钮灰（`HasNextLevel=false`） |
| 12 | 选择关卡 | 结算点“选择关卡” | 切选关页，无黑屏重载 |
| 13 | 重试残留 | 连点两次重开 | 照片/数字/音效不残留 |
| 14 | 入场动画 | 进关卡后立刻结算 | 动画只在结算页出现时播一次 |
| 15 | AimPreview | 主菜单/准备页/Running | 前两者看不到准星；Running 才有；蓄力环与百分比同步 |
| 16 | 弱点准星 | 瞄准 mass 7/8 史莱姆弱点 | 变黄；移开恢复默认 |
| 17 | 受伤红屏 | 被史莱姆击中 | 闪一下淡出；低血常亮 |
| 18 | 设置 0×0 | 删 `GameUserSettings.ini` 后启动 | 不出现 `0 x 0` |
| 19 | 设置音量 | 拖到 0.3 → 重启 | 仍是 0.3 |
| 20 | 设置返回 | 主菜单→设置→返回 | 回主菜单，不黑屏 |

---

## 6. 与音频清单的交叉（同一 WBP 一次改完）

| WBP | 本文（UI） | 音频清单（发声） | 顺序 |
|---|---|---|---|
| `WBP_GenericButton` | — | B2.1 加 `Click` | 只做一次 |
| `WBP_SlimeResult` | U-W1 全七步 | B3 结算音（挂 `OnResultShown`） | **先 U-W1，再插音**（节点要先存在） |
| `WBP_SlimeSettings` | U-W6 | 三键音 + 音量 | 先 U-W6，再插音 |
| `WBP_SlimeHUD` | U-W4 特殊子弹 | HUD 四个事件音 | 一次打开同时做 |
| `WBP_AimPreview` | U-W7 Reparent | 满蓄力/锁弱点音 | 先 Reparent，再插音 |
| 选关 / 落点 / 暂停 | U-W2 / U-W3 / U-W5 | 对应按钮音 | 一次打开同时做 |
| `WBP_SlimeMainMenu` | — | B2.2 | 只在音频单里做 |

原则：**UI 负责“显示与交互”，音频负责“发声”，同一个 `.uasset` 一次打开把两件事都做完**，不要按文档顺序跑两遍（`.uasset` 是二进制，反复打开容易互相覆盖）。

---

## 7. 风险与回滚

- U1 只改代码与新建类，**不碰任何 `.uasset`**；编译不过只需回滚代码。
- `OnResultShown` 不出现 → 说明 U1' 没编译或 WBP 没 Reparent 到 `USlimeResultWidget`。
- AimPreview 不显示 → 三处查：`AimPreviewWidgetClass` 是否配、WBP 是否 Reparent、四个控件名是否一致。
- 照片永远是兜底图 → 查关卡里有没有相机、日志有没有 `settlement photo captured`。
- 结算页数字全是 0 → 说明读到的是关卡开始那次（`OnResultReady` 第一次），确认 `RefreshResult` 在 `Ended` 被调用。
- 一次点击响两声 → `WBP_GenericButton` 与页面各播了一次，删页面那条。
- 进关卡响了两声"进入"音 → `EnterLevel` 已由 C++ 在 `ApplyPhaseToScreens` 播，WBP 里不要再接。

## 8. 变更记录

| 日期 | 摘要 |
|---|---|
| 2026-10-20 | 初版：程序侧 U-C1~U-C7、WBP 侧 U-W1~U-W8、编辑器 U-A1~U-A3、20 条验收、与音频清单的交叉表 |
| 2026-10-20 | U-W2 锁定卡改为不发声；卡片补 `OnHovered→Choose1` / `OnPressed→Choose2` 两个触发点（对应官方事件表修正） |

