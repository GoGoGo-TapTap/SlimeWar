# 设置页实施文档（WBP_SlimeSettings）

> **给谁看**：负责 `WBP_SlimeSettings` 的 UI，以及改 `USlimeSettingsWidget` 的 C++。
> **怎么用**：先看 §0 的两张记录表；按 §3 的顺序做；每节先看「C++ 要改什么」，再照「WBP 怎么连」；最后按 §10 验收。
> **维护约定**：C++ 接口或语义一变，先更新 `Docs/UI/UI_Blueprint_Interface_Guide.md` §1 的 `SlimeSettingsWidget` 表，再回来改本文对应小节。两张记录表随做随改，状态变了就地更新。
> **范围**：本期只做**屏幕分辨率 / 窗口模式 / 应用 / 重置 / 返回**，以及入口和页面骨架的核对。
> **不做什么**：不改 Wwise 工程、不实现音量、不把设置页塞进 `ESlimeUIFlowState`、不重构 `USlimeUISubsystem`。
> **读图约定**：白线（exec）/ 数据线 / `Sequence` 的规则见 `UI_Blueprint_Interface_Guide.md` §0（11-40 行），本文不重复。

---

## 0. 记录表（开工前先填）

> 状态用项目统一四态：`[ ]` 未开始 / `[~]` 进行中 / `[x]` 完成 / `[!]` 阻塞或暂缓（`Docs/ProgramTaskList.md:630-631`）。
> `Owner` 开工时认领；**WBP 是二进制资产，同一时间只能一个人改**。

### 0.1 功能缺口表

| ID | 模块 | 期望行为 | 现状 / 证据 | 本期动作 | 依赖 | Owner | 状态 | 关联 |
|---|---|---|---|---|---|---|---|---|
| S-01 | 入口 | 主菜单能打开设置，返回后主菜单恢复 | C++ 已通（`SlimeMainMenuWidget.cpp:24-43`、`:55-89`）；配置在 `Config/DefaultGame.ini:16` | 只验证，不改代码 | — | TBD | `[~]` | R-10 |
| S-02 | 页面骨架 | 页面布局完整，控件都在、都能点 | 资产已有完整控件和 4 条绑定（§1.3） | 核对，缺的补 | S-01 | TBD | `[~]` | R-12 |
| S-03 | 分辨率 | 首次打开不显示 `0 x 0` | `NativeConstruct` 直接读 `GetScreenResolution()`（`SlimeSettingsWidget.cpp:15-19`） | C++ 加兜底（§5.1） | — | TBD | `[ ]` | R-01 |
| S-04 | 分辨率 | 下拉包含平台支持项 + 当前分辨率 + 桌面分辨率 | `GetSupportedResolutions` 只追加 Pending（`:149-163`） | 追加桌面、过滤非正、稳定排序 | S-03 | TBD | `[ ]` | R-01 |
| S-05 | 分辨率 | 选择只暂存，点应用才生效并持久化 | 两个 ComboBox 都没有 `OnSelectionChanged` | 填充 / 回填 / 防递归（§5.3-5.5） | S-04 | TBD | `[ ]` | R-05、R-06 |
| S-06 | 窗口模式 | 三项顺序固定，选择只暂存 | C++ 映射就绪（`:50-67`、`:135-147`）；WBP 未接 | 三项 + 连线（§6） | — | TBD | `[ ]` | R-07 |
| S-07 | 联动 | 无边框时分辨率下拉禁用，切回恢复 | 无 | 只改 WBP（§6.3） | S-06 | TBD | `[ ]` | R-03 |
| S-08 | 应用 | 应用分辨率 + 模式并写盘，不写 `0×0` | `ApplySettings` 未校验（`:69-91`） | 下发前兜底 + 日志（§7.1） | S-03、S-04 | TBD | `[ ]` | R-01、R-09 |
| S-09 | 重置 | 重置后控件回默认且不出现 `0×0`；只暂存 | `ResetSettings` 在 `SetToDefaults` 后读到 0×0（`:93-105`） | 兜底 + 文案（§8） | S-03 | TBD | `[ ]` | R-02、R-13 |
| S-10 | 返回 | 返回必定回主菜单，不黑屏 | `ReturnWidget` 失效时只 `RemoveFromParent`（`:107-128`） | `ShowMainMenu()` 兜底（§9.1） | — | TBD | `[ ]` | R-04 |
| S-11 | 声音 | 音量滑块可操作，拖动即生效并写盘 | `SetMasterVolume` 只 clamp + 暂存，`ApplySettings` 写 ini；`ApplyMasterVolumeToAudio()` **已于 2026-10-20 删除** | 滑块恢复 `Is Enabled` + 绑 `SetMasterVolume`（只改暂存值）；**发声改在 WBP 的 `OnSettingsValuesChanged` 里推 `Set RTPC Value`**（《UI音频接入方案》§4.1） | 蓝图层 | TBD | `[~]` | R-11 |
| S-12 | 资产核对 | 文档假设与资产一致 | 资产有控件，与指南「整页空白」描述不符 | 开工前 30 秒核对（§1.3） | — | TBD | `[~]` | R-12 |
| S-13 | 验收 | 按 §10 全部通过 | 未验 | 执行 §10 | 全部 | TBD | `[ ]` | — |
| S-14 | 首次进入 | 三行都有默认值：声音有位置、分辨率有选中项、模式有选中项 | 当前进入时三行都没有默认值（§2.4） | WBP 填选项并设选中（§2.4、§5.3、§6.2） | S-02、S-05、S-06 | TBD | `[ ]` | R-15 |

### 0.2 问题 / 风险表

| ID | 等级 | 问题 / 风险 | 触发条件 | 影响 | 处理策略 | 关联 | 证据 | 状态 |
|---|---|---|---|---|---|---|---|---|
| R-01 | 高 | 首次启动分辨率 `0×0` | 从未保存过 `GameUserSettings.ini` | 下拉显示 `0 x 0`；应用时可能下发 0×0 | 本期修：私有 helper 逐级兜底 | S-03、S-04、S-08 | `SlimeSettingsWidget.cpp:15-19` | `[ ]` |
| R-02 | 高 | 重置后 `0×0` | `SetToDefaults()` 把分辨率清 0，随后直接读回 | 重置后下拉为空、应用写 0×0 | 本期修：重置后走同一 helper | S-09 | `:93-105` | `[ ]` |
| R-03 | 中 | 无边框窗口下改分辨率无效 | 选「无边框窗口」后调分辨率 | 玩家以为设置坏了 | UI 禁用下拉；C++ 不做特殊判断 | S-07 | UE 窗口行为 | `[ ]` |
| R-04 | 高 | 返回后黑屏 | `ReturnWidget` 为空 / 已销毁 / 不在视口 | 设置页移除后没有 UI | 本期修：回退 `ShowMainMenu()` | S-10 | `:107-119` | `[ ]` |
| R-05 | 高 | 首次刷新早于下拉填充 | `NativeConstruct` 末尾就触发 `OnSettingsValuesChanged()` | 回填空下拉、选中 -1 | WBP 加「Option 数为 0 直接 return」守卫 + Construct 自己补一次 | S-02、S-05 | `:31` | `[ ]` |
| R-06 | 高 | `Set Selected Index` 触发递归 | 程序化回填下拉 | 死循环、卡死、值来回弹 | `bUpdatingUI` 守卫 | S-05 | UMG 行为 | `[ ]` |
| R-07 | 中 | 模式索引顺序被改 | 重排选项或加「默认」项 | 选「全屏」变成「无边框」 | 固定 0/1/2，只改文案 | S-06 | `:50-67` | `[ ]` |
| R-08 | 中 | 按钮形态判断错 | 按裸 `Button` 的习惯去连 `WBP_GenericButton` | 找不到 `On Clicked`，或一次点击跑两遍 | 见 §7.2 的形态判断表 | S-02、S-08、S-10 | 资产里的 `OnButtonClicked` 绑定 | `[ ]` |
| R-09 | 中 | PIE 下分辨率不生效 | 在编辑器 PIE 里改分辨率 | 误判功能坏了 | 验收用 Standalone / 打包 | S-08 | UE 行为 | `[ ]` |
| R-10 | 低 | 双击设置创建多个实例 | 主菜单连点「设置」 | 多层设置页、返回异常 | 本期接受；需要时再加弱指针守卫 | S-01 | `SlimeMainMenuWidget.cpp:70-83` | `[ ]` |
| R-11 | 低 | 音量滑块是死控件 | 滑块可拖但音频未接 | 玩家拖了没声音 | **重新打开**：`SetMasterVolume` 已随音频 C++ 一起删除，需在 WBP 里推 `Set RTPC Value`（`all`，插值 100ms） | S-11 | `:165-170`（已删） | `[~]` |
| R-12 | 高 | 资产现状与「空白页」描述不符 | 直接按空白页重建 | 覆盖现有布局、产生重复控件 | 先核对；已有控件保留原名，只补连线 | S-02、S-12 | `UI实现/WBP_Blueprint_Wiring_Guide.md:136` | `[~]` |
| R-13 | 低 | 重置是全局默认 | 以后加画质等设置 | 未展示的选项也被重置并落盘 | 本期接受；以后重定义 Reset 范围 | S-09 | `:98` | `[ ]` |
| R-14 | 低 | `Docs/UI` 的接口指南 §6 仍写「设置不用改」 | 读者只看 §6 | 被误导 | 本次只改 §1，在 §1 表下加指向本文的注 | — | `UI_Blueprint_Interface_Guide.md:683-687` | `[ ]` |
| R-15 | 高 | 进入设置页三行都没有默认值 | WBP 从未 `Add Option` / `Set Selected Index`；声音没回填 | 页面像坏了，玩家不知道当前设置，也无法判断改没改 | 本期修：按 §2.4 的初始化顺序补 | S-14 | 资产里没有 `AddOption` / `SetSelectedIndex` 节点 | `[ ]` |

---

## 1. 范围、入口与现状核对

### 1.1 本期做 / 不做

| 做 | 不做 |
|---|---|
| 分辨率下拉的填充、回填、选择 | 设备音量、总线结构改动 |
| 窗口模式下拉的填充、回填、选择 | 画质、语言、按键、灵敏度 |
| 无边框时禁用分辨率下拉 | 暂停页的设置入口 |
| 应用 / 重置 / 返回三个按钮 | 应用后自动关页、返回时自动保存 |
| **主音量滑块（映射 Wwise `all` RTPC，0~1）** | Sfx / Bgm 分离滑块（本期只做总音量） |
| C++ 的 4 处边界修复（§5.1、§8.1、§9.1） | 新增公开 C++ 接口、重构 UI 子系统 |
| 资产核对与连线补齐 | 改任何 `.uasset` 之外的东西（本文只写文档，不动资产） |

### 1.2 入口与生命周期

```text
主菜单 Btn_Settings
  -> USlimeMainMenuWidget::OpenSettings()        SlimeMainMenuWidget.cpp:24-43
     -> 读 USlimeGameSettings::SettingsWidgetClass（DefaultGame.ini:16）
     -> OpenMenuWidget(SettingsClass)             :55-89
        -> CreateWidget（每次新建，不缓存）
        -> SetReturnWidget(this)                  :76-79
        -> AddToViewport(10)                      盖在 UI 子系统页面（Z=0）之上
        -> 主菜单 SetVisibility(Collapsed)
```

要点：

- 设置页**不在** `ESlimeUIFlowState` 状态机里；打开期间 UI 子系统仍认为自己在 `MainMenu`。
- 关闭即 `RemoveFromParent()`，**不缓存**。没点「应用」就返回，改动直接丢弃；下次打开由 `NativeConstruct` 重新读盘。
- 返回目标是「打开它的那个 WBP」，由 `SetReturnWidget` 记录；当前唯一入口是主菜单。
- `OpenSettings()` 读不到类或加载失败时只打 Warning，不会崩（`:27-42`）。

### 1.3 WBP 资产现状核对（开工第一步，约 30 秒）

按仓库当前资产（`Content/_SlimeWar/UI/WBP_SlimeSettings.uasset`，91,570 B，2026-10-18 21:55）核对，**它不是空白页**：

| 检查项 | 期望 | 不符时怎么办 |
|---|---|---|
| Class Settings | 父类是 `SlimeSettingsWidget` | 重新 reparent（`UCLASS(Abstract)`，不 reparent 时 `Cast` 失败） |
| Designer 控件 | 见 §2.1 的清单 | 缺的补，**已有的不要改名**（会带动引用） |
| 四个按钮 | `Btn_Apply` / `Btn_Reset` / `Btn_Back` 各有一条 `On Button Clicked` 绑定 | 缺的按 §7-§9 补 |
| 音量滑块 | 绑定节点名是 `Slider_332`，资产里另有 `Slider_Volume` | **先确认是不是同一个控件、绑定有没有悬空**；悬空就删掉那条绑定，只保留 `Slider_Volume` 并置灰 |
| 分辨率下拉 | `ComboBox_Resolution` 没有 `Add Option`、没有 `Set Selected Index`、没有 `OnSelectionChanged`（所以现在是空的） | 按 §5.3 / §5.5 补 |
| 模式下拉 | `ComboBox_Fullscreen` 没有 `Add Option`、没有 `Set Selected Index`、没有 `OnSelectionChanged`（所以现在是空的） | 按 §6.2 补 |
| 根节点 | 叫 `Cavans_Root`（拼写笔误） | **不要为了拼写改名**，改名会拖动一堆引用 |

> 归档的 `UI实现/WBP_Blueprint_Wiring_Guide.md:136` 写「整个页面空白：控件、初始化、六个函数全缺」，与资产实况不符，属于描述滞后。**本文以 Designer 里看到的为准**，核对结果记到 R-12。

### 1.4 三个状态

| 状态 | 什么时候 | 存在哪 | 返回时 |
|---|---|---|---|
| 待应用 | 玩家改了任一控件 | `PendingResolution` / `PendingFullscreenMode`（`SlimeSettingsWidget.h:72-73`） | 丢弃 |
| 已应用 | 点了「应用」 | `UGameUserSettings` + `SaveSettings()`（`GameUserSettings.ini`） | 保留 |
| 默认 | 点了「重置」 | 只回填到 Pending，**不落盘** | 丢弃 |

---

## 2. 页面形态与数据流

### 2.1 控件清单

| 控件 | 类型 | 本期动作 |
|---|---|---|
| `Cavans_Root` | Canvas Panel | 不动（拼写不改） |
| `Img_SettingsBG` / `Img_TitleBG` | Image | 不动 |
| `Overlay_Title` / `Text_Title` / `Text_TitleEn` / `HBox_Title` | Overlay / Text / HBox | 不动 |
| `HBox_Volume` | HorizontalBox | 保留布局 |
| `VBox_VolumeLabel` / `Btn_VolumeLabel` / `Text_VolumeEn` | 标签 | 不动 |
| `Slider_Volume` | Slider | **置 `Is Enabled = false`，断开 `OnValueChanged`**（§4.2） |
| `HBox_Resolution` | HorizontalBox | 不动 |
| `VBox_ResolutionLabel` / `Btn_ResolutionLabel` / `Text_ResolutionEn` | 标签 | 不动（标签不绑事件） |
| `ComboBox_Resolution` | ComboBox (String) | 补填充 / 回填 / 选择（§5） |
| `HBox_Fullscreen` | HorizontalBox | 不动 |
| `VBox_FullscreenLabel` / `Btn_FullscreenLabel` / `Text_FullscreenEn` | 标签 | 不动 |
| `ComboBox_Fullscreen` | ComboBox (String) | 补填充 / 回填 / 选择（§6） |
| `HBox_Buttons` | HorizontalBox | 不动 |
| `Btn_Apply` / `Btn_Reset` / `Btn_Back` | `WBP_GenericButton` 实例 | 确认各只有一条绑定（§7.2） |

### 2.2 层级树

```text
Cavans_Root (Canvas Panel)
├─ Img_SettingsBG
├─ Img_TitleBG
├─ Overlay_Title / HBox_Title
│  ├─ Text_Title
│  └─ Text_TitleEn
├─ HBox_Volume                  ← 本期保留、不可操作
│  ├─ VBox_VolumeLabel -> Btn_VolumeLabel
│  ├─ Text_VolumeEn
│  └─ Slider_Volume
├─ HBox_Resolution
│  ├─ VBox_ResolutionLabel -> Btn_ResolutionLabel
│  ├─ Text_ResolutionEn
│  └─ ComboBox_Resolution
├─ HBox_Fullscreen
│  ├─ VBox_FullscreenLabel -> Btn_FullscreenLabel
│  ├─ Text_FullscreenEn
│  └─ ComboBox_Fullscreen
└─ HBox_Buttons
   ├─ Btn_Apply
   ├─ Btn_Reset
   └─ Btn_Back
```

### 2.3 一次操作的调用链

```text
玩家改分辨率下拉
  -> ComboBox_Resolution.OnSelectionChanged
  -> SetResolution(X, Y)                    // 只写 Pending
  -> OnSettingsValuesChanged()              // C++ 回调，WBP 回填（bUpdatingUI 挡）
  -> 实际窗口不变

玩家点「应用」
  -> Btn_Apply.On Button Clicked
  -> ApplySettings()
  -> UGameUserSettings::SetScreenResolution / SetFullscreenMode
  -> ApplySettings(false) + SaveSettings()  // 立即生效并写盘
  -> OnSettingsValuesChanged()

玩家点「返回」
  -> Btn_Back.On Button Clicked
  -> ReturnToPreviousMenu()
  -> RemoveFromParent + 恢复主菜单 + UIOnly
```

### 2.4 首次进入的默认状态（三行都必须有值）

> **现状**：进入设置页时，**声音没有默认值，分辨率和模式下拉是空的**。
> 原因不在 C++：C++ 已经给了 `GetResolution()` / `GetFullscreenMode()` / `GetMasterVolume()`，但 WBP **从来没有 `Add Option`，也没有 `Set Selected Index`**，所以下拉里没有项、也没有选中项；声音滑块也没有回填。
> 这一节定义「进入页面时应该看到什么」，§5 / §6 负责怎么做到。

| 行 | 进入时应该显示 | 数据来源 | 现在为什么是空的 | 补在哪 |
|---|---|---|---|---|
| 声音 | 滑块停在存档音量位置；本期置灰、不可拖 | `GetMasterVolume()`（存档缺失时 C++ 默认 `1.0`） | 没有任何回填，且 Designer 里 Slider 的 `Value` 可能是 `0` | §4.2：Designer 里把 `Value` 设为 `1.0` + 置灰；本期不做回填 |
| 屏幕分辨率 | 当前分辨率，例如 `1920 x 1080` | `GetSupportedResolutions()` + `GetResolution()` | `ComboBox_Resolution` 没有 `Add Option`，也没有 `Set Selected Index` | §5.3 |
| 模式 | `窗口化 / 全屏 / 无边框窗口` 三项，选中当前模式 | 固定三项（WBP 手写）+ `GetFullscreenMode()` | `ComboBox_Fullscreen` 一项都没加 | §6.2 |

**空态对照（进页面看到的样子 → 说明 → 怎么处理）**

| 现象 | 说明 | 处理 |
|---|---|---|
| 下拉框空白，点开也没有任何项 | 从来没有 `Add Option` | 按 §5.3 / §6.2 在 `Construct` 里填 |
| 有选项，但框里显示空白 | 加完选项没 `Set Selected Index`，或索引是 `-1` | 显式 `Set Selected Index`；分辨率匹配不到就选 0 |
| 分辨率只有一项 | 平台列表为空，C++ 只追加了当前 / 桌面 | C++ 修复后这是正常兜底；可以接受 |
| 声音滑块停在最左（0） | Designer 默认 `Value = 0`，且没回填 | Designer 里设 `1.0`；本期不接回填 |
| 三行全都没有值 | `Construct` 里填充 / 选中一步都没做 | 按下面的顺序补齐 |

**初始化顺序（照这个顺序做，缺一步就是空）**

```text
1. C++ NativeConstruct
     -> 读 Pending：分辨率走 ResolveValidResolution（§5.1），模式默认 Windowed，音量默认 1.0
     -> 末尾触发 OnSettingsValuesChanged（此时 WBP 下拉可能还没有选项）
2. WBP Event Construct
     -> bUpdatingUI = true
     -> 填分辨率选项（GetSupportedResolutions -> ForEach Add Option + 存 ResolutionOptions）
     -> 填模式三项（Add Option x3）
     -> 选中：分辨率按 GetResolution 反查，模式按 GetFullscreenMode
     -> 刷新无边框禁用分辨率状态
     -> bUpdatingUI = false
3. 之后玩家改任何控件
     -> SetResolution / SetFullscreenMode -> OnSettingsValuesChanged -> 回填（带空守卫 + bUpdatingUI）
```

**现在缺的就是第 2 步。** 只要 WBP 不做这一步，C++ 补多少兜底都看不到；这也是为什么"进去三行都是空的"。

**怎么补（按顺序，三件事，全部在 WBP，不用改 C++）**

```text
1. 模式下拉
   Construct -> Clear Options -> Add Option "窗口化" / "全屏" / "无边框窗口"
             -> Set Selected Index (Get Fullscreen Mode)

2. 分辨率下拉
   Construct -> Clear Options -> ResolutionOptions.Clear
             -> Get Supported Resolutions -> ForEach: Add Option "{X} x {Y}" + Array Add
             -> 用 Get Resolution 在 ResolutionOptions 里反查 -> Set Selected Index（找不到选 0）

3. 声音滑块
   Designer -> Slider 的 Value 设为 1.0 -> 取消勾选 Is Enabled
   本期不接回填；等音量方案定了再补 OnSettingsValuesChanged 里的 Set Value
```

> **顺序不能反**：必须先 `Add Option` 再 `Set Selected Index`；反了就是"先选一个不存在的项"，索引会被夹成 -1 或 0，之后回填也对不上。

---

## 3. 执行顺序总览

| 步骤 | 章节 | 内容 | 前置 |
|---|---|---|---|
| 0 | §1.3 | 资产核对（控件、绑定、`Slider_332`） | — |
| 0.5 | §2.4 | 首次进入的默认状态（三行都要有值） | 0 |
| 1 | §4 | 声音（接 Wwise `all` RTPC，滑块可操作） | 0 |
| 2 | §5 | 屏幕分辨率 | 0 |
| 3 | §6 | 窗口模式 | 2 |
| 4 | §7 | 应用 | 2、3 |
| 5 | §8 | 重置 | 2 |
| 6 | §9 | 返回 | 0 |
| 7 | §10 | 验收 | 全部 |

**修复流程不单独成章**：首次进入的空态写在 §2.4，0×0 的兜底写在 §5.1 和 §8.1，返回黑屏写在 §9.1，防递归写在 §5.4-5.5。

---

## 4. 第 1 步：声音（改为蓝图推 RTPC，C++ 路线已作废）

> **2026-10-20 二次补丁**：Wwise 参数默认值已修为 100 并重生成 Bank，RTPC `all/sfx/bgm` 都在，
> 所以"滑块置灰 + 断线"的结论**作废**。
>
> **但 C++ 路线也不能用了**：音频 C++ 层（含 `USlimeAudioSubsystem` 与 `ApplyMasterVolumeToAudio()`）
> 当天整体删除。现在只有一条路——
>
> 在 `WBP_SlimeSettings` 的 `On Settings Values Changed` 事件里：
> `Get Master Volume`（0~1）→ `×100` → `Set RTPC Value`（RTPC = `all`，插值 `100`，`Actor` 留空）。
> 只接这一处；滑块上只绑 `SetMasterVolume` 改暂存值，**不要**在滑块上发声。
>
> 细节见 `Docs/音效/UI音频接入方案.md` §4.1，执行位置见 `Docs/音效/音频接入执行清单.md` 批次 4。
>
> 以下 §4.1~§4.5 保留原文，仅作历史记录，**不要照做**。

### 4.1 现状（历史）

| 项 | 现状 | 证据 |
|---|---|---|
| C++ 暂存 | `SetMasterVolume` 只 clamp 0~1 + 暂存 | `SlimeSettingsWidget.cpp:34-38` |
| 读盘 / 写盘 | `NativeConstruct` 读、`ApplySettings` 写 `GameUserSettings.ini` 的 `[/Script/SlimeWar.SlimeSettingsWidget] MasterVolume` | `:21-30`、`:80-88` |
| 音频 | `ApplyMasterVolumeToAudio()` 是刻意空实现，**全项目无调用** | `:165-170`（声明 `.h:67-68`） |
| WBP | 滑块绑在 `Slider_332` 上 → `SetMasterVolume` | 资产字符串 |

### 4.2 本期动作（只改 WBP）

0. Designer 里把 `Slider_Volume` 的 `Value` 设为 `1.0`（C++ 的默认音量就是 1.0）。不改的话禁用后会停在 0 位，玩家会以为当前是静音。
1. `Slider_Volume`（或确认后的实际控件）→ Details → `Interaction` → 取消勾选 `Is Enabled`。
2. 删掉 `OnValueChanged` 那条绑定，避免「能拖、能写盘、但听不见」的死控件。
3. `HBox_Volume` **保留**，不要 `Collapsed`：设计稿有这一行，折叠会让版式缺一块。只是让滑块不可操作。
4. C++ 一行不改，ini key 和函数签名都保留。

### 4.3 为什么暂缓

- 音频链路没接完，现在接线只能得到一个「拖了没反应」的滑块，验收无法判定对错。
- 音量方案涉及 Wwise 侧的参数绑定范围，属于独立问题，不塞进本次显示设置的交付里。

### 4.4 恢复音量时要补什么（记录，不执行）

```text
1. C++：给 ApplyMasterVolumeToAudio() 填实现，并在 ApplySettings()、启动时各调一次
2. C++：需要一个"启动时应用一次"的挂点（GameInstanceSubsystem 的 Initialize）
3. WBP：滑块恢复 Is Enabled，重新绑 OnValueChanged -> SetMasterVolume
4. 验收：重启后保持音量；拖到 0 能静音；拖回 1.0 恢复
```

### 4.5 自检

```text
[ ] 设置页看不到可操作的音量控件（滑块灰掉）
[ ] 滑块停在 1.0 位置，不是 0
[ ] WBP 里没有 SetMasterVolume 的绑定
[ ] C++ 的 MasterVolume 读写和函数签名都还在
[ ] HBox_Volume 没有被 Collapsed，版式没有缺行
```

> 坑：把 `HBox_Volume` 设成 `Collapsed` 会连标题和英文一起消失，设计稿对不上；要「不可操作」就禁用滑块，不要藏整行。

---

## 5. 第 2 步：屏幕分辨率

### 5.1 C++：不改 / 要补 / 不做

| 项 | 结论 | 说明 |
|---|---|---|
| `SetResolution(int32, int32)` | **不改** | 非正值已忽略；仍触发 `OnSettingsValuesChanged`（`:40-48`） |
| `GetResolution()` | **不改** | 返回 `PendingResolution`（`.h:50-51`） |
| `PendingResolution` | **不改** | `SlimeSettingsWidget.h:72` |
| `ResolveValidResolution()` | **新增（私有）** | 见下，不标 `UFUNCTION`，不进 §1 表 |
| `NativeConstruct` | **行为变更** | `PendingResolution` 改走 helper（`:15-19`） |
| `ResetSettings` | **行为变更** | `SetToDefaults()` 后走 helper（`:98-100`，详见 §8.1） |
| `ApplySettings` | **行为变更** | 下发前校验，绝不写 0×0（`:74`，详见 §7.1） |
| `GetSupportedResolutions` | **行为变更** | 追加当前 / 桌面、过滤非正、稳定排序（`:149-163`） |
| 新增公开接口 | **不做** | WBP 自己维护 `FIntPoint` 数组，不再加 `GetResolutionIndex` 之类 |
| `IsResolutionSupported` | **不做** | 下拉只放支持项，C++ 不需要二次判断 |

**新增私有 helper 的行为（逐级兜底，每级打日志）：**

```cpp
FIntPoint USlimeSettingsWidget::ResolveValidResolution(const UGameUserSettings* UserSettings) const
// 1) GetScreenResolution() 两维都 > 0        -> 直接用
// 2) GetDesktopResolution() 两维都 > 0       -> 用桌面，打 Verbose
// 3) GetSupportedFullscreenResolutions() 非空 -> 取面积最大的一项（不要假设有序），打 Verbose
// 4) 以上都失败                              -> FIntPoint(1280, 720)，打 Warning
```

为什么必须抽出来：`NativeConstruct`、`ResetSettings`、`ApplySettings` 三处要用同一套策略；写三遍一定漏一处。

`GetSupportedResolutions` 的具体规则：

```text
1. 取平台列表
2. 过滤 X <= 0 或 Y <= 0 的项
3. 追加 PendingResolution（有效时）、桌面分辨率（不同时）
4. 排序：面积升序 -> X 升序 -> Y 升序（保证同一台机器顺序稳定）
```

> 坑：`GetSupportedFullscreenResolutions` 在窗口模式下返回的仍是显示器支持的全屏分辨率列表，不是「当前窗口能开多大」。这是平台接口的语义，不要试图在 C++ 里改成窗口尺寸枚举。

### 5.2 WBP：需要新增的两个变量

| 名称 | 类型 | 默认 | 用途 |
|---|---|---|---|
| `ResolutionOptions` | Array of IntPoint | 空 | 下拉索引 ↔ `FIntPoint` 的唯一映射 |
| `bUpdatingUI` | Boolean | false | 程序化回填期间挡掉 `OnSelectionChanged` |

> 只加这两个。不要在 WBP 里再存一份「当前分辨率字符串」，字符串只用于显示。

### 5.3 `Event Construct`：填充与首次回填

```text
Event Construct
  ├─ Set bUpdatingUI = true
  ├─ ComboBox_Resolution.Clear Options
  ├─ ResolutionOptions.Clear
  ├─ Get Supported Resolutions (Out Resolutions)
  ├─ ForEachLoop (Resolutions)
  │    ├─ Break IntPoint -> X, Y
  │    ├─ Format Text "{X} x {Y}" -> ComboBox_Resolution.Add Option
  │    └─ Array Add (ResolutionOptions, Item)
  ├─ ComboBox_Fullscreen.Clear Options
  ├─ Add Option "窗口化" / "全屏" / "无边框窗口"      ← 顺序固定，见 §6
  ├─ ForEachLoopWithBreak (ResolutionOptions)
  │    ├─ Equal (IntPoint 与 Get Resolution)
  │    │    True -> ComboBox_Resolution.Set Selected Index (Index) -> Break
  │    └─ Completed -> Set Selected Index 0 + Print String "当前分辨率不在列表"
  ├─ ComboBox_Fullscreen.Set Selected Index (Get Fullscreen Mode)
  ├─ ComboBox_Resolution.Set Is Enabled (Get Fullscreen Mode != 2)
  └─ Set bUpdatingUI = false
```

要点：

- **首次回填必须 WBP 自己做一次**。C++ 在 `NativeConstruct` 末尾（`:31`）就调了 `OnSettingsValuesChanged()`，那一刻下拉可能还是空的。
- `ResolutionOptions` 只在 `Construct` 里建一次，之后只改选中索引，不要 `Clear + 重建`。
- `Equal (IntPoint)` 如果节点列表里没有，就 `Break IntPoint` 后分别比较 X、Y 再 `AND`。
- 反查失败不要留 `-1`，选 0 并打字符串，方便排错。

### 5.4 `Event OnSettingsValuesChanged`：回填

```text
Event On Settings Values Changed
  ├─ Branch: ComboBox_Resolution.Get Option Count == 0 -> return     ← 守卫放最前
  ├─ Set bUpdatingUI = true
  ├─ 反查 Get Resolution 在 ResolutionOptions 的下标
  │    找到   -> ComboBox_Resolution.Set Selected Index (Index)
  │    未找到 -> ComboBox_Resolution.Set Selected Index (0) + Print String
  ├─ ComboBox_Fullscreen.Set Selected Index (Get Fullscreen Mode)
  ├─ ComboBox_Resolution.Set Is Enabled (Get Fullscreen Mode != 2)
  └─ Set bUpdatingUI = false
```

要点：

- 守卫必须在最前面，而且 `return` 分支不要动 `bUpdatingUI`（它此时还是 false）。
- 任何**中途** `return` 的分支，都要先把 `bUpdatingUI = false`，否则后续所有用户操作都会被吞掉。
- 不要在这个事件里 `Clear Options`，会丢选中项并触发额外回调。

### 5.5 `OnSelectionChanged`：用户改选

```text
Event ComboBox_Resolution.OnSelectionChanged (Selected Item, Selection Type)
  ├─ Branch: bUpdatingUI == true -> return
  ├─ Index = ComboBox_Resolution.Get Selected Index
  ├─ Branch: Index >= 0 AND Index < ResolutionOptions.Length()
  │    True -> ResolutionOptions[Index] -> Break IntPoint -> Set Resolution (X, Y)
  └─ False -> 什么都不做（不要拿 Selected Item 去解析字符串）
```

要点：

- `OnSelectionChanged` 只给**字符串**，不给索引；索引必须自己 `Get Selected Index`。
- `Set Selected Index` 会再次触发本事件，所以回填期间必须 `bUpdatingUI == true`，否则会形成
  `SetResolution -> OnSettingsValuesChanged -> SetSelectedIndex -> OnSelectionChanged -> SetResolution` 的循环。
- 不要用 `Find Option Index` 反查分辨率，格式一变就找不到；只用 `ResolutionOptions[Index]`。

### 5.6 边界表

| 场景 | 状态 | 期望 |
|---|---|---|
| 首次启动无 ini | `GetScreenResolution() == (0,0)` | 显示桌面分辨率；下拉有值 |
| 重置后 | `SetToDefaults()` 把分辨率置 0 | 显示桌面分辨率，不出现 `0 x 0` |
| 应用时仍无效 | `PendingResolution` 非正 | helper 兜底后再下发，绝不写 0×0 |
| 平台列表为空 | C++ 返回空数组 | 正常情况下 C++ 已追加当前 / 桌面，不会是空数组；WBP 若仍拿到空数组，加一个禁用的占位项「无可用分辨率」并禁用下拉，不要留空 |
| 当前分辨率不在列表 | 平台列表没有它 | 追加进列表，保证能选中 |
| 重复项 | 平台列表已有当前 / 桌面 | 去重，不出现两个相同项 |
| 无边框模式 | `GetFullscreenMode() == 2` | 下拉禁用，但保留已选值 |
| PIE | 编辑器视口 | 分辨率可能不生效，不作为验收依据 |

### 5.7 自检

```text
[ ] 首次打开不是 0 x 0
[ ] 进入页面时下拉至少有一项，且已选中（不是空白框）
[ ] 下拉项数量 == ResolutionOptions 长度
[ ] 改选一次只触发一次 SetResolution
[ ] 回填不递归（bUpdatingUI 生效）
[ ] 反查失败时选中 0，不是 -1
[ ] 无边框时不出现"改了分辨率却没变化"的假象（下拉已禁用）
```

> 坑：`ComboBoxString` 在 `Add Option` 时会自动选中第一项并触发一次 `OnSelectionChanged`。所以填充阶段必须包在 `bUpdatingUI = true` 里，且 `Clear Options` 也要在里面。

---

## 6. 第 3 步：窗口模式

### 6.1 C++：不用改

| 项 | 结论 | 说明 |
|---|---|---|
| `SetFullscreenMode(int32)` | **不改** | 0/1/2 映射，越界 `Clamp` 到 2（`:50-67`） |
| `GetFullscreenMode()` | **不改** | 反向映射回 0/1/2（`:135-147`） |
| `PendingFullscreenMode` | **不改** | `SlimeSettingsWidget.h:73` |
| 新增接口 | **不做** | 不需要 `ApplyFullscreenMode()`，`ApplySettings()` 已一起应用 |

### 6.2 WBP：三项与索引约束

`ComboBox_Fullscreen` 的选项**必须**按这个顺序加：

| Index | 显示文案 | C++ 枚举 |
|---|---|---|
| 0 | 窗口化 | `EWindowMode::Windowed` |
| 1 | 全屏 | `EWindowMode::Fullscreen` |
| 2 | 无边框窗口 | `EWindowMode::WindowedFullscreen` |

```text
Event Construct 里：
  -> ComboBox_Fullscreen.Clear Options
  -> Add Option "窗口化"
  -> Add Option "全屏"
  -> Add Option "无边框窗口"
  -> ComboBox_Fullscreen.Set Selected Index (Get Fullscreen Mode)
```

```text
Event ComboBox_Fullscreen.OnSelectionChanged (Selected Item, Selection Type)
  ├─ Branch: bUpdatingUI == true -> return
  ├─ Index = ComboBox_Fullscreen.Get Selected Index
  ├─ Branch: Index >= 0 AND Index <= 2
  │    True -> Set Fullscreen Mode (Index)
  │            -> ComboBox_Resolution.Set Is Enabled (Index != 2)
  └─ False -> 什么都不做
```

要点：

- **顺序不能改**，索引直接当模式号用；文案随便改。
- 不要加第 4 项「默认」，会让索引整体错位。
- `Get Selected Index` 可能是 `-1`（清空选项那一瞬间），必须先判断再传；`Clamp(-1, 0, 2)` 会静默变成 0，看起来像「自动选了窗口化」。
- **现在页面是空的，就是因为 `Construct` 里还没有这三行 `Add Option`。** 加完必须再 `Set Selected Index`：只加选项不设索引时，UMG 可能显示空白，也可能自动停在第一项，两种都不等于"当前模式"。

### 6.3 无边框时禁用分辨率

| 模式 | 分辨率是否影响显示 | 下拉状态 |
|---|---|---|
| 窗口化 (0) | 是 | 可用 |
| 全屏 (1) | 是 | 可用 |
| 无边框窗口 (2) | 否（引擎用桌面分辨率） | 禁用 |

- 三个地方都要刷新：`Construct` 末尾、`OnSettingsValuesChanged`、模式 `OnSelectionChanged`。
- 禁用时**保留**已选分辨率，切回窗口化 / 全屏后恢复可用，不要清空选择。
- 这是 UI 约束，不是安全边界：C++ 仍然接受 `SetResolution`，只是无边框下引擎不会按它改显示。

### 6.4 自检

```text
[ ] 三项顺序是 窗口化 / 全屏 / 无边框窗口
[ ] 页面打开时下拉有三项，不是空的
[ ] 首次打开选中项等于 Get Fullscreen Mode
[ ] 选无边框后分辨率下拉变灰
[ ] 切回窗口化 / 全屏后下拉恢复可用且保留原选择
[ ] 没有把 -1 传给 Set Fullscreen Mode
```

> 坑：模式下拉的 `OnSelectionChanged` 里不要再调 `SetResolution`。分辨率由分辨率下拉负责，两个下拉互相写会形成跨控件循环。

---

## 7. 第 4 步：应用

### 7.1 C++：要补

| 项 | 结论 | 说明 |
|---|---|---|
| 应用顺序 | **不改** | `SetScreenResolution` → `SetFullscreenMode` → `ApplySettings(false)` → `SaveSettings()`（`:69-91`） |
| `false` 参数 | **不改** | 不检查命令行覆盖，游戏内应用就该用它 |
| 分辨率校验 | **要补** | 下发前用 `ResolveValidResolution` 兜底；`PendingResolution` 回写成实际下发的值 |
| `UserSettings` 为空 | **要补** | 打 Error 并跳过，不要静默「成功」 |
| 成功日志 | **要补** | 打一条 `LogSlimeWar`：请求的分辨率 + 模式，方便验收时看 Output Log |
| 自动关页 | **不做** | 应用后留在本页，让玩家继续调 |
| `OnSettingsApplied` | **不做** | `OnSettingsValuesChanged` 已经会在末尾触发，够用 |

> 坑：`SetScreenResolution(0,0)` 在部分平台会被引擎用桌面分辨率兜底，看起来「没坏」，但 `GameUserSettings.ini` 里会留下 0×0，下次启动又回到 R-01。必须在下发前拦住。

### 7.2 WBP：按钮形态与连线

先判断按钮是哪种形态，接错入口就是「点了没反应」：

| 控件 | 有没有 `On Clicked` | 正确接法 | 能否禁用 |
|---|---|---|---|
| `WBP_GenericButton` 实例 | **没有**，只有自定义事件 `On Button Clicked` | Details → Events 里点 `+` 建组件事件（现有资产就是 `BndEvt__...OnButtonClicked`）；列表里没有才用 `Bind Event to On Button Clicked` | 没有暴露禁用接口 |
| 裸 `UButton` | **有** `On Clicked` | 直接连 `On Clicked` | 可以 `Set Is Enabled` |
| `WBP_SelectableButton` | 没有 `On Clicked`，只有 `On Button Clicked` | 必须 `Bind Event to`；它带选中态，不适合本页动作按钮 | — |

本页三个按钮当前都是 `WBP_GenericButton`，保持现状：

```text
[Btn_Apply] On Button Clicked -> Apply Settings
[Btn_Reset] On Button Clicked -> Reset Settings
[Btn_Back]  On Button Clicked -> Return To Previous Menu
```

要点：

- **不要同时**在 Details 里建组件事件、又在 `Construct` 里 `Bind Event to`；一次点击会跑两遍（应用两次、重置两次）。
- 按钮文案在实例的 Details → `Button Text` 里填：`应用` / `恢复默认` / `返回`。
- 现有资产已经有三条绑定，先确认没有重复，再补缺失的。

### 7.3 可选：应用反馈

可以加一个 `Text_Status`（TextBlock，默认 `Collapsed`）：点应用后显示「已应用」，1.5 秒后收起。不要为此新增 C++ 事件，用 WBP 的 Delay 即可。不做也不影响验收。

### 7.4 自检

```text
[ ] 每次点击只触发一次 ApplySettings
[ ] 点应用后 Output Log 有分辨率 + 模式的日志
[ ] 无边框下点应用不崩、不写 0×0
[ ] PIE 里不生效时先用 Standalone 复验再判断
[ ] 应用后页面不自动关闭
```

---

## 8. 第 5 步：重置

### 8.1 C++：0×0 修复

| 项 | 结论 | 说明 |
|---|---|---|
| `SetToDefaults()` | **保留** | 恢复引擎默认（`:98`） |
| 之后的 `GetScreenResolution()` | **要改** | `SetToDefaults()` 的默认实现会把分辨率清 0（平台实现可能不同），直接读多半是 `(0,0)` |
| 改为 | `ResolveValidResolution(UserSettings)` | 与 §5.1 同一个 helper |
| `PendingFullscreenMode` | **保留** | 继续读 `GetFullscreenMode()`，默认 `Windowed` |
| `PendingMasterVolume = 1.0f` | **保留** | 音量本期不显示，但复位逻辑别删 |
| 自动应用 | **不做** | 重置只回填控件，必须再点「应用」 |

### 8.2 语义：只暂存，不生效

```text
点「重置」 -> 控件回到默认（分辨率 = 桌面，模式 = 窗口化）
           -> 实际窗口不变
           -> 不写盘
点「应用」 -> 才真正改窗口并保存
```

按钮文案建议：`恢复默认`，旁边一行小字（或 tooltip）「点应用后生效」。否则玩家会以为按了没反应。

### 8.3 WBP

- `Btn_Reset` → `On Button Clicked` → `Reset Settings`（已存在，确认即可）。
- 重置后由 `OnSettingsValuesChanged` 回填两个下拉；不要再在前端手工清空或设索引。
- 如果默认模式是窗口化，分辨率下拉应恢复可用（`Get Fullscreen Mode != 2`）。
- 分辨率反查应该能找到桌面项（`GetSupportedResolutions` 已追加）；找不到就选 0 并打字符串。

### 8.4 自检

```text
[ ] 点重置后分辨率是桌面值，不是 0 x 0
[ ] 点重置后实际窗口没有立刻变化
[ ] 点重置后再点应用能落盘，重启保持
[ ] 重置后分辨率下拉可用
```

> 坑：`SetToDefaults()` 是**全局默认**，不只分辨率和窗口模式。以后加了画质选项，Reset 会一起重置并落盘（R-13）。本期只有显示设置可见，接受。

---

## 9. 第 6 步：返回

### 9.1 C++：兜底

| 项 | 结论 | 说明 |
|---|---|---|
| 正常路径 | **不改** | `ReturnWidget` 有效：`RemoveFromParent()` + 恢复调用页（`:111-115`） |
| 兜底 | **要补** | `ReturnWidget` 为空 / 失效 / 不在视口时，调 `USlimeUISubsystem::Get(this)->ShowMainMenu()` |
| 顺序 | **要补** | 先恢复目标页（或回退），再 `Collapsed` + `RemoveFromParent`，避免一帧黑屏 |
| 日志 | **要补** | 走兜底时打 Warning，便于确认 `SetReturnWidget` 没调到 |
| `ReturnToMainMenu()` | **不要用** | 它会 `OpenLevel` 重载当前地图（`SlimeUISubsystem.cpp:572-600`），回菜单会黑屏几秒 |
| 进 `ESlimeUIFlowState` | **不做** | 设置页仍是临时覆盖层 |

`ShowMainMenu()`（`SlimeUISubsystem.cpp:564-570`）是纯切屏，不重载世界，正好符合「设置页返回主菜单」。

### 9.2 WBP

```text
[Btn_Back] On Button Clicked -> Return To Previous Menu
```

- **不要**在 WBP 里再手动 `Set Visibility` / `Remove From Parent`；C++ 已经负责生命周期，重复做会二次移除。
- 返回不自动应用：没点应用就返回，改动丢弃。这是现有行为，也是验收项。

### 9.3 自检

```text
[ ] 点返回后主菜单可见、设置页消失、鼠标可点
[ ] 再次打开设置，显示的是已保存值（未应用的改动已丢弃）
[ ] ReturnWidget 失效时不黑屏，Output Log 有 Warning
[ ] WBP 里没有重复的 RemoveFromParent
```

> 坑：`ReturnToPreviousMenu()` 里已经有 `SetVisibility(Collapsed)`，再 `RemoveFromParent()` 是冗余但无害；补兜底时不要顺手删掉 `Collapsed`，它保证移除那一帧不闪现。

---

## 10. 验收清单

> **PIE 只验 UI 流程**；分辨率和窗口模式必须用 **Standalone 或打包**验证。
> 测设置页前确认 `bSkipMainMenuInPIE=False`（`Config/DefaultGame.ini:21`），否则启动直接进关卡，测不到入口。

| # | 场景 | 怎么造 | 期望 |
|---|---|---|---|
| 1 | 首次启动无存档 | 删 `Saved/Config/Windows/GameUserSettings.ini`（PIE 是 `WindowsEditor`）→ Standalone 启动 → 主菜单 → 设置 | 分辨率有值且不是 `0 x 0`；模式等于当前模式；无 Error |
| 2 | 打开 / 关闭 | 主菜单点设置，点返回 | 设置页出现、主菜单隐藏；返回后主菜单可见、设置页消失、鼠标可点 |
| 3 | 分辨率暂存 | 记下当前分辨率，选另一项，不点应用 | 下拉立即变；实际窗口不变；返回再进恢复旧值 |
| 4 | 分辨率应用 | 选另一项 → 应用 | 窗口分辨率变化；`GameUserSettings.ini` 的 `ResolutionSizeX/Y` 更新；重启保持 |
| 5 | 窗口模式 | 依次选窗口化 / 全屏 / 无边框 → 应用 | 模式按选择变化 |
| 6 | 无边框联动 | 选无边框 → 应用 | 分辨率下拉禁用；改分辨率不生效；UI 不报错 |
| 7 | 切回非无边框 | 无边框 → 窗口化 | 分辨率下拉恢复可用，且保留之前的选择 |
| 8 | 重置暂存 | 改分辨率 / 模式 → 重置 | 控件回默认（桌面分辨率 + 窗口化）；实际窗口未变；不是 0×0 |
| 9 | 重置应用 | 接第 8 步 → 应用 | 实际窗口回默认；写盘；重启保持 |
| 10 | 重置后 0×0 | 点重置后看下拉 | 不出现 `0 x 0`；至少有一项 |
| 11 | 返回兜底 | 临时把 `SetReturnWidget(this)` 注释掉（`SlimeMainMenuWidget.cpp:78`）→ 返回 | 不黑屏；回主菜单；Output Log 有 Warning |
| 12 | 快速操作 | 连续快速切换分辨率 / 模式、连点应用 | 不卡死、不递归；最终值与下拉一致 |
| 13 | 打包验证 | 打包 Windows 或 Standalone，重复 3~9 | 全部生效；PIE 结果不作验收 |
| 14 | 配置缺失 | 临时把 `SettingsWidgetClass` 置空或改错 | 点设置无页面；Output Log 有 Warning；不崩 |
| 15 | 音量（原：无可操作控件） | 打开设置，拖动音量滑块 | **可拖**；拖动时 UI 音随之变化；拖到 0 静音 |
| 16 | 入口确认 | 确认 `bSkipMainMenuInPIE=False` | 能从主菜单进设置 |
| 17 | 首次进入默认值 | 全新存档进入设置 | 音量滑块停在 1.0 且可拖；分辨率有选中项；模式有三项且选中当前模式 |

> 第 11 条要临时改代码造场景，验完必须还原。

---

## 11. 排错速查

| 现象 | 先看这里 |
|---|---|
| 分辨率显示 `0 x 0` | C++ 的 `ResolveValidResolution` 有没有接进 `NativeConstruct`（§5.1）；是否删过 `GameUserSettings.ini` |
| 分辨率下拉是空的 | `GetSupportedResolutions` 返回空？WBP 是否在 `Construct` 里调了 `Add Option`？Option 数和数组长度是否一致 |
| 模式下拉是空的 | `Construct` 里没 `Add Option` 三项（§6.2）；当前资产就是这个状态 |
| 有选项但框里是空白 | 只 `Add Option` 没 `Set Selected Index`，或索引是 -1（§2.4） |
| 三行都没有默认值 | WBP 的 `Construct` 没做填充 + 选中；按 §2.4 的初始化顺序补 |
| 声音滑块停在最左 | Designer 的 Slider `Value` 是 0，且本期没回填（§4.2） |
| 改下拉没反应 | `OnSelectionChanged` 是否真的绑上（两个 ComboBox 当前都没有）；`bUpdatingUI` 是否卡在 true |
| 一拖/一选就卡死 | 回填没挡 `bUpdatingUI`，形成递归；检查 `OnSettingsValuesChanged` |
| 选了全屏变成无边框 | 模式下拉的顺序被改了，或加了第 4 项（§6.2） |
| 点应用没变化 | 是否在 PIE 里测（R-09）；是否只改了暂存没点应用；`ApplySettings` 是否真的被调用 |
| 设置页整页空白 | WBP 是否 reparent 到 `SlimeSettingsWidget`；`SettingsWidgetClass` 是否指向本资产（`DefaultGame.ini:16`） |
| 返回后黑屏 | `SetReturnWidget` 有没有被调用；`ReturnWidget` 是否失效（§9.1 兜底） |
| 点按钮没反应 | 按钮是 `WBP_GenericButton` 还是裸 `Button`；接错入口（§7.2） |
| 一次点击执行两次 | Details 事件和 `Bind Event to` 同时存在（§7.2） |
| 出现了两层设置页 | 主菜单连点（R-10）；确认只创建一次 |
| 改完分辨率重启又回去 | 只点了下拉没点应用；或 `SaveSettings()` 没执行 |

---

## 12. 维护与变更记录

### 12.1 接口变化怎么记

1. 改 `USlimeSettingsWidget` 的公开函数或语义 → 先更新 `Docs/UI/UI_Blueprint_Interface_Guide.md` §1 的 Settings 表。
2. 再回来改本文对应小节，并在下表追加一行。
3. WBP 侧只改连线、不改签名时，不需要动 §1 表，但要更新 §0 的状态。

### 12.2 本文变更记录

| 日期 | 版本 | 摘要 |
|---|---|---|
| 2026-10-19 | 初版 | 拆分 6 个步骤的 C++ / WBP 分工；音量暂缓；登记 13 项缺口与 14 项风险 |
| 2026-10-19 | 修订 1 | 新增 §2.4「首次进入的默认状态」：三行默认值、空态对照、初始化顺序、三步补齐；补 S-14、R-15、验收 17 与排错条目 |
| 2026-10-20 | 修订 2 | 音量方案由「置灰断线」改为「接入 Wwise `all` RTPC」：S-11 重开、R-11 关闭、§1.1 范围表与 §4 加补丁说明、执行顺序与验收 15/17 同步；细节见 `Docs/音效/UI音频接入方案.md` §4.6 与 `Docs/UI/UI_改动补丁_框架对齐.md` §B |

### 12.3 注意事项

- `WBP_SlimeSettings.uasset` 是二进制资产，**改之前先在群里认领**（GitHub 不支持文件锁）。
- 命名统一用 ASCII：`ComboBox_Resolution`、`ComboBox_Fullscreen`、`Btn_Apply`、`Btn_Reset`、`Btn_Back`。
- 现有 `Cavans_Root` 拼写笔误**不要改**，改名会带动引用；新建时写 `Canvas_Root`。
- `Docs/UI/UI_Blueprint_Interface_Guide.md` §6（`683-687` 行）仍写「`WBP_SlimeSettings` 不用改」，本文不改那一节；以本文为准。
