# UI 菜单 C++ 接口文档

本文档说明新增菜单 UI C++ 基类的职责、蓝图可调用接口和 WBP 绑定方式。

## 1. 总体边界

本项目采用：

```text
C++：提供功能接口、流程控制和 UE 系统调用
WBP：负责布局、图片、文本、按钮、动画和视觉状态
```

新增菜单类不替代已有的局内 UI。

已有局内 UI 仍由以下系统负责：

```text
USlimeUISubsystem
USlimeHUDWidget
USlimePreparationWidget
USlimeResultWidget
USlimePauseWidget
```

`USlimeUISubsystem` 继续只管理：

```text
落点准备页
局内 HUD
结算页
暂停页
```

主菜单、设置页和关卡选择页是独立菜单流程。

## 2. 新增文件

### 主菜单

```text
Source/SlimeWar/Public/UI/SlimeMainMenuWidget.h
Source/SlimeWar/Private/UI/SlimeMainMenuWidget.cpp
```

对应 WBP：

```text
Content/_SlimeWar/UI/WBP_SlimeMainMenu.uasset
```

### 设置页

```text
Source/SlimeWar/Public/UI/SlimeSettingsWidget.h
Source/SlimeWar/Private/UI/SlimeSettingsWidget.cpp
```

对应 WBP：

```text
Content/_SlimeWar/UI/WBP_SlimeSettings.uasset
```

### 关卡选择页

```text
Source/SlimeWar/Public/UI/SlimeLevelSelectWidget.h
Source/SlimeWar/Private/UI/SlimeLevelSelectWidget.cpp
```

对应 WBP：

```text
Content/_SlimeWar/UI/WBP_SlimeLevelSelect.uasset
```

## 3. 主菜单接口

C++ 类：

```cpp
USlimeMainMenuWidget
```

WBP 创建后需要将父类设置为：

```text
SlimeMainMenuWidget
```

### `StartGame()`

蓝图节点：

```text
Start Game
```

行为：

1. 如果 `LevelSelectWidgetClass` 已配置，创建并显示关卡选择页。
2. 如果关卡选择页没有配置或加载失败，直接打开：

```text
/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza
```

因此主菜单的“开始游戏”按钮不直接控制局内 HUD 或落点选择。

WBP 绑定：

```text
开始游戏按钮 OnClicked
    -> StartGame
```

### `OpenSettings()`

蓝图节点：

```text
Open Settings
```

行为：

1. 读取 `USlimeGameSettings::SettingsWidgetClass`。
2. 创建设置页 WBP。
3. 隐藏当前主菜单。
4. 将当前主菜单作为设置页的返回页面。

WBP 绑定：

```text
游戏设置按钮 OnClicked
    -> OpenSettings
```

### `QuitGame()`

蓝图节点：

```text
Quit Game
```

行为：

```text
UKismetSystemLibrary::QuitGame
```

WBP 绑定：

```text
退出游戏按钮 OnClicked
    -> QuitGame
```

## 4. 设置页接口

C++ 类：

```cpp
USlimeSettingsWidget
```

WBP 创建后需要将父类设置为：

```text
SlimeSettingsWidget
```

## 4.1 设置值接口

### `SetMasterVolume(float Volume)`

设置待应用的主音量。

参数范围：

```text
0.0 到 1.0
```

推荐 WBP 绑定：

```text
音量 Slider OnValueChanged
    -> SetMasterVolume
```

该函数会自动限制输入范围。

音量不会立即写入用户配置，必须点击“应用”并调用 `ApplySettings()`。

### `SetResolution(int32 Width, int32 Height)`

设置待应用的分辨率。

推荐 WBP 逻辑：

```text
分辨率下拉框选中
    -> 拆分为 Width 和 Height
    -> SetResolution(Width, Height)
```

例如：

```text
1920 x 1080
    Width = 1920
    Height = 1080
```

### `SetFullscreenMode(int32 Mode)`

设置待应用的窗口模式。

蓝图接口使用以下编号：

```text
0 = 窗口化
1 = 全屏
2 = 无边框窗口
```

推荐 WBP 使用 ComboBox：

```text
窗口化       -> SetFullscreenMode(0)
全屏         -> SetFullscreenMode(1)
无边框窗口   -> SetFullscreenMode(2)
```

## 4.2 设置操作接口

### `ApplySettings()`

应用并保存：

```text
分辨率
窗口模式
主音量
```

分辨率和窗口模式使用：

```cpp
UGameUserSettings
```

主音量当前行为：

```cpp
暂存并保存到用户配置
暂不修改实际音频输出
```

Wwise 2024.1.8.8898 接入后，主音量会改为设置 Wwise RTPC/Game Parameter。

WBP 绑定：

```text
应用按钮 OnClicked
    -> ApplySettings
```

### `ResetSettings()`

恢复默认的显示设置和主音量值。

WBP 绑定：

```text
恢复默认按钮 OnClicked
    -> ResetSettings
```

注意：恢复默认只修改当前设置页中暂存的值。用户需要再次点击“应用”，才会将显示设置写入引擎配置。

### `ReturnToPreviousMenu()`

隐藏设置页，显示进入设置页之前的页面。

当前流程下通常是：

```text
设置页 -> 主菜单
```

WBP 绑定：

```text
返回按钮 OnClicked
    -> ReturnToPreviousMenu
```

### `SetReturnWidget(UUserWidget* Widget)`

该函数主要由 C++ 内部调用，用于记录返回页面。

一般不需要在 WBP 中手动调用。

## 4.3 设置读取接口

### `GetMasterVolume()`

返回当前设置页暂存的主音量。

可用于：

```text
初始化 Slider
刷新音量文本
```

### `GetResolution()`

返回当前暂存的 `FIntPoint` 分辨率。

```text
X = Width
Y = Height
```

### `GetFullscreenMode()`

返回蓝图接口定义的窗口模式编号：

```text
0 = 窗口化
1 = 全屏
2 = 无边框窗口
```

### `GetSupportedResolutions(TArray<FIntPoint>& OutResolutions)`

取得当前平台支持的分辨率列表。

WBP 可在构造时调用该函数，然后把每个 `FIntPoint` 转成显示文本，例如：

```text
1280 x 720
1600 x 900
1920 x 1080
```

### `OnSettingsValuesChanged()`

这是 `BlueprintImplementableEvent`，不是按钮函数。

每当设置值变化时，C++ 会调用该事件。WBP 可以在这里统一刷新：

```text
音量 Slider
当前分辨率文本
窗口模式 ComboBox
设置预览文本
```

推荐在 WBP 中实现这个事件，避免每个控件各自维护一套刷新逻辑。

## 4.4 设置页音频状态

当前项目尚未接入 Wwise，且不再依赖 UE 原生的 `SC_Master` / `SM_Master`。

当前主音量流程为：

```text
Slider
    -> SetMasterVolume
    -> 暂存 0.0 到 1.0 的值
    -> ApplySettings
    -> 保存到用户配置
```

当前阶段实际音频音量不会改变，这是有意保留的占位行为。

Wwise 2024.1.8.8898 接入后，程序会根据音效老师提供的 Game Parameter/RTPC：

```text
Slider
    -> SetMasterVolume
    -> ApplySettings
    -> 设置 Wwise Master Volume RTPC
```

因此当前 `WBP_SlimeSettings` 不需要配置 `MasterSoundClass` 或 `MasterSoundMix`。

## 5. 关卡选择接口

C++ 类：

```cpp
USlimeLevelSelectWidget
```

WBP 创建后需要将父类设置为：

```text
SlimeLevelSelectWidget
```

## 5.1 `SelectLevel(int32 LevelIndex)`

选择关卡。

当前编号：

```text
0 = 01 CityPlaza
1 = 02，未开放
2 = 03，未开放
```

WBP 绑定示例：

```text
CityPlaza 按钮 OnClicked
    -> SelectLevel(0)

02 按钮 OnClicked
    -> SelectLevel(1)

03 按钮 OnClicked
    -> SelectLevel(2)
```

该函数会触发：

```cpp
OnLevelSelectionChanged(LevelIndex, bAvailable)
```

## 5.2 `EnterSelectedLevel()`

进入当前选中的关卡。

当前只有 `LevelIndex == 0` 可以进入：

```text
/Game/_SlimeWar/Maps/L_Whitebox_CityPlaza
```

如果当前选择的是 02 或 03，函数不会切换地图。

WBP 绑定：

```text
进入关卡按钮 OnClicked
    -> EnterSelectedLevel
```

建议 WBP 根据当前可用状态控制进入按钮：

```text
CityPlaza 被选中 -> 进入按钮可用
02/03 被选中     -> 进入按钮禁用或显示未开放
```

## 5.3 `ReturnToMainMenu()`

隐藏关卡选择页，重新显示主菜单。

WBP 绑定：

```text
返回主菜单按钮 OnClicked
    -> ReturnToMainMenu
```

### `SetReturnWidget(UUserWidget* Widget)`

由主菜单 C++ 设置返回对象。

通常不需要在 WBP 中手动调用。

### `GetSelectedLevel()`

返回当前选择的关卡编号：

```text
0、1 或 2
```

### `IsLevelAvailable(int32 LevelIndex)`

判断关卡当前是否开放。

当前结果：

```text
IsLevelAvailable(0) = true
IsLevelAvailable(1) = false
IsLevelAvailable(2) = false
```

### `OnLevelSelectionChanged(int32 LevelIndex, bool bAvailable)`

这是 `BlueprintImplementableEvent`。

推荐在 WBP 中实现：

```text
更新当前选中边框
更新关卡预览图
更新关卡名称
更新“未开放”文字
更新进入按钮是否可用
```

## 6. 配置入口

代码使用 `USlimeGameSettings` 保存三个菜单 WBP 类引用：

```cpp
MainMenuWidgetClass
SettingsWidgetClass
LevelSelectWidgetClass
```

当前配置位置：

```text
Config/DefaultGame.ini
```

配置内容：

```ini
MainMenuWidgetClass=/Game/_SlimeWar/UI/WBP_SlimeMainMenu.WBP_SlimeMainMenu_C
SettingsWidgetClass=/Game/_SlimeWar/UI/WBP_SlimeSettings.WBP_SlimeSettings_C
LevelSelectWidgetClass=/Game/_SlimeWar/UI/WBP_SlimeLevelSelect.WBP_SlimeLevelSelect_C
```

这些路径要求对应 WBP 资产真实存在。

## 7. UE 编辑器中的创建顺序

### 创建主菜单 WBP

1. 在 `Content/_SlimeWar/UI` 创建 Widget Blueprint。
2. 命名为 `WBP_SlimeMainMenu`。
3. 将父类设置为 `SlimeMainMenuWidget`。
4. 添加开始游戏、游戏设置和退出游戏按钮。
5. 分别绑定：

```text
StartGame
OpenSettings
QuitGame
```

### 创建设置页 WBP

1. 创建 `WBP_SlimeSettings`。
2. 将父类设置为 `SlimeSettingsWidget`。
3. 添加音量 Slider。
4. 添加分辨率 ComboBox。
5. 添加窗口模式 ComboBox。
6. 添加应用、恢复默认和返回按钮。
7. 绑定对应的 C++ 函数。
8. 当前不需要设置 UE 原生 Sound Class 或 Sound Mix；Wwise 接入后再配置对应的 Wwise Game Parameter。

### 创建关卡选择 WBP

1. 创建 `WBP_SlimeLevelSelect`。
2. 将父类设置为 `SlimeLevelSelectWidget`。
3. 添加 CityPlaza、02、03 三个关卡按钮。
4. 添加进入关卡和返回主菜单按钮。
5. 绑定对应的 C++ 函数。
6. 实现 `OnLevelSelectionChanged`，更新选中状态和未开放显示。

## 8. 当前没有实现的内容

本次接口不包含：

```text
菜单地图 .umap
自动从游戏启动进入主菜单
关卡解锁存档
通关进度保存
下一关逻辑
多关卡数据表
动态关卡预览
```

当前 `DefaultEngine.ini` 仍然使用原来的默认地图：

```ini
GameDefaultMap=/Game/ThirdPerson/Maps/ThirdPersonMap.ThirdPersonMap
```

如果需要正式从主菜单启动，还需要在 UE 编辑器中创建菜单地图，并将 `GameDefaultMap` 改为菜单地图。

## 9. 与现有落点选择页的关系

落点选择继续使用：

```text
WBP_SlimePreparation
USlimePreparationWidget
```

已有接口：

```cpp
PreviewDropPoint(int32 DropPointIndex)
ConfirmDropPoint(int32 DropPointIndex)
ConfirmPreviewedDropPoint()
```

菜单关卡选择页只负责打开 `CityPlaza`。

进入 `CityPlaza` 后，运行中的 `USlimeUISubsystem` 再负责显示：

```text
WBP_SlimePreparation
```

完整流程为：

```text
主菜单
    -> 关卡选择
    -> CityPlaza
    -> WBP_SlimePreparation
    -> 投放演出
    -> WBP_SlimeHUD
    -> WBP_SlimeResult
```
