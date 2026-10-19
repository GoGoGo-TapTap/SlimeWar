# Wwise 音频初始化方案（最小改动版）

> **给谁看**：接 Wwise 的程序 + 做 `WBP_SlimeSettings` 的 UI。
> **怎么用**：先看 §1 的根因（已实测），再按 §2 → §3 的顺序做。§4 是框架稳定后才做的正式方案。
> **前提**：程序老师正在改整体框架，本方案要求**不新增 C++ 类、不改动框架层**。
> **工程环境**：UE 5.5 ｜ Wwise 2024.1.8 Build 8898（Integration 2024.1）｜ `AudioRouting=EnableWwiseOnly`

---

## 1. 根因（已实测，不是猜测）

### 1.1 现象

- 蓝图 `Space Bar Pressed → Post Event (player_footstep, Actor=Self)` 无声音
- 编辑器里播引擎自带 `Click_on_Button` SoundCue 也无声音
- Wwise Browser 显示 `Not Connected to Wwise`

### 1.2 三个原因，分清主次

| # | 现象 | 真实原因 | 严重度 |
|---|---|---|---|
| 1 | **全部音效静音** | `sfx` / `bgm` / `all` 三个 Game Parameter 的 **Default Value = 0**，而 RTPC 曲线是 `0 → -200 dB`、`100 → 0 dB` | 🔴 主因 |
| 2 | 引擎原生 SoundCue 没声音 | `AudioRouting=EnableWwiseOnly`，Wwise 主动关闭 UE 的 AudioDeviceManager（日志 `Audio Device Manager Initialization Failed!`） | 🟡 预期行为，不是 bug |
| 3 | Wwise Browser 连不上 | `WwiseProjectPath` 曾指向不存在的 `SlimeWar_WwiseProject/...` | 🟢 已修复 |

### 1.3 证据

`2026taptap/Game Parameters/Default Work Unit.wwu`

```xml
<GameParameter Name="sfx"> <Property Name="InitialValue" Value="0"/>
<GameParameter Name="bgm"> <Property Name="InitialValue" Value="0"/>
<GameParameter Name="all"> <Property Name="InitialValue" Value="0"/>
```

`2026taptap/Actor-Mixer Hierarchy/Default Work Unit.wwu`（容器/对象上绑定了 `all` + `sfx` 两条 Volume RTPC，全工程共 52 条 RTPC；40 个 Sound 中 22 个直接绑定，其余叶子变体靠父容器继承）

```xml
<Point><XPos>0</XPos>   <YPos>-200</YPos></Point>   <!-- 参数 0 → 完全静音 -->
<Point><XPos>100</XPos> <YPos>0</YPos></Point>      <!-- 参数 100 → 原音量 -->
```

`Saved/Logs/SlimeWar.log`（引擎已就绪，但默认参数把声音压死）

```
LogAkAudio: Wwise SoundEngine successfully initialized.
LogAkAudio: Audiokinetic Audio Device initialized.
LogAudio: Warning: Audio Device Manager Initialization Failed!   ← EnableWwiseOnly 的预期结果
LogWwiseMonitor: Voice Starvation [Debugger]                     ← 次要，先不管
```

---

## 2. 阶段 0：零代码修复（必做，5 分钟）

> 这一步不需要改任何代码。Bank 里已经烘进了「0 = -200 dB」，只改 C++ 等于在运行时打补丁。

1. 打开 Wwise Authoring，加载 `2026taptap/2026taptap.wproj`
2. `Game Syncs` → `Game Parameters` → 依次选中 `sfx` / `bgm` / `all`
3. 右侧编辑器顶部 **`Default Value` 改为 `100`**
4. `Ctrl+S` 保存
5. `Generate SoundBanks`（F7）→ 平台 `Windows` → Generate
6. 重启 UE 编辑器 → 重新 PIE

**验收**：按空格能听到 `player_footstep`。

> 注意：UE 里的 `Generate SoundBanks...` 按钮依赖 WAAPI 连接，Wwise Authoring 没打开时会失败。
> 这一步直接在 Wwise 里按 F7 最稳。

### 2.1 顺手建议（非必须）

当前曲线是**线性 dB**：`0→-200`、`50→-100`、`90→-20`，滑条只有最后 5 格听得见。
后续要做音量菜单时建议改成对数式：

| RTPC | dB |
|---|---|
| 0 | -96 |
| 50 | -12 |
| 80 | -4 |
| 100 | 0 |

另：`move01-05` / `player_hit01-04` / `player_footstep01-05` / `weapon_hit01-04` 这 18 个叶子 Sound 自己没绑 RTPC，靠父容器继承。
如果以后要单独控某一路（比如让 `weapon_hit` 不受 `sfx` 影响），需要给那个对象单独 Add RTPC。

---

## 3. 阶段 1：最小代码初始化（4 处改动，不新增类）

目标：让"启动即应用保存的音量"这一行为落在已有代码里，**不改框架、不加 Subsystem**。

### 3.1 改动清单

| # | 文件 | 改动 |
|---|---|---|
| 1 | `Source/SlimeWar/SlimeWar.Build.cs` | 依赖加 `"AkAudio"` |
| 2 | `Public/UI/SlimeSettingsWidget.h` | 加 `UAkRtpc` 成员与实现声明 |
| 3 | `Private/UI/SlimeSettingsWidget.cpp` | 把已有的空实现 `ApplyMasterVolumeToAudio()` 改成真正推 RTPC |
| 4 | `Private/Player/SlimeWarPlayerController.cpp` | `BeginPlay` 里读 ini 并推一次（启动初始化） |

> 为什么改 `ApplyMasterVolumeToAudio()`：`SlimeSettingsWidget.h:67` 的注释写着
> *"Reserved for Wwise 2024.1.8.8898 RTPC integration. Currently a deliberate no-op."*
> —— 这个桩就是为这一刻留的，改动面最小、语义也最准确。

### 3.2 代码

**① `SlimeWar.Build.cs`**（`PublicDependencyModuleNames` 里追加）

```csharp
"AkAudio",   // Wwise：UAkGameplayStatics / UAkRtpc
```

**② `SlimeSettingsWidget.h`**

```cpp
class UAkRtpc;

// public:
UFUNCTION(BlueprintPure, Category = "Slime|UI|Settings")
float GetMasterVolume() const { return PendingMasterVolume; }   // 已有

// protected:
UPROPERTY(EditDefaultsOnly, Category = "Slime|UI|Settings|Audio")
TObjectPtr<UAkRtpc> MasterVolumeRTPC;                            // 新增

void ApplyMasterVolumeToAudio();                                 // 已有，改实现
```

**③ `SlimeSettingsWidget.cpp`**

```cpp
#include "AkGameplayStatics.h"
#include "AkRtpc.h"

void USlimeSettingsWidget::ApplyMasterVolumeToAudio()
{
    if (MasterVolumeRTPC)
    {
        // 0~1 → 0~100；Interpolation 100ms 防止拖动滑条时爆音
        UAkGameplayStatics::SetRTPCValue(MasterVolumeRTPC, PendingMasterVolume * 100.f, 100, nullptr);
    }
}
```

在 `NativeConstruct()` 末尾（`OnSettingsValuesChanged()` 之后）加一行，让进设置页时也同步一次：

```cpp
ApplyMasterVolumeToAudio();
```

**④ `SlimeWarPlayerController.cpp`** —— 启动时应用保存值

```cpp
#include "AkGameplayStatics.h"
#include "AkRtpc.h"
#include "Misc/ConfigCacheIni.h"

void ASlimeWarPlayerController::BeginPlay()
{
    Super::BeginPlay();

    float SavedVolume = 1.0f;
    if (GConfig)
    {
        GConfig->GetFloat(
            TEXT("/Script/SlimeWar.SlimeSettingsWidget"),
            TEXT("MasterVolume"),
            SavedVolume,
            GGameUserSettingsIni);
    }
    SavedVolume = FMath::Clamp(SavedVolume, 0.0f, 1.0f);

    static ConstructorHelpers::FObjectFinder<UAkRtpc> RtpcAll(
        TEXT("/Game/WwiseAudio/Game_Parameters/Default_Work_Unit/all.all"));
    if (RtpcAll.Succeeded())
    {
        UAkGameplayStatics::SetRTPCValue(RtpcAll.Object, SavedVolume * 100.f, 0, nullptr);
    }
}
```

> `Actor` 传 `nullptr` = 全局 Game Parameter，等价于蓝图里 `Actor` 引脚留空。

### 3.3 资产路径（已核对）

```
/Game/WwiseAudio/Game_Parameters/Default_Work_Unit/all.all
/Game/WwiseAudio/Game_Parameters/Default_Work_Unit/sfx.sfx
/Game/WwiseAudio/Game_Parameters/Default_Work_Unit/bgm.bgm
```

必须用 `ConstructorHelpers` / `UPROPERTY` 硬引用；用 `LoadObject` 拼字符串打包后会静音。

### 3.4 API 签名（本工程插件源码实测）

`Plugins/Wwise/Source/AkAudio/Classes/AkGameplayStatics.h`

```cpp
static void SetRTPCValue(class UAkRtpc const* RTPCValue, float Value, int32 InterpolationTimeMs, class AActor* Actor);
```

---

## 4. 阶段 2：正式方案（等框架稳定后再做）

把「值 + 应用 + 存档」收进 `UAudioSettingsSubsystem : UGameInstanceSubsystem`，理由：

| 优势 | 说明 |
|---|---|
| UI 直接拿 | `Get Game Instance → Get Subsystem (AudioSettings)`，不需要 Cast 到 PC |
| 跨关卡不丢 | 与 GameInstance 同生命周期；PC / GameMode 切关卡会重建 |
| 一份数据源 | 滑条初始值、实时调节、存档走同一对象 |
| 不污染播放逻辑 | 武器 / 角色 C++ 只负责 `PostAkEvent` |

### 4.1 接口设计

```cpp
// 初始化 / 读档后一次性应用
UFUNCTION(BlueprintCallable) void ApplyAll(float Master01, float Sfx01, float Bgm01);

// UI 单项调节（互不覆盖）
UFUNCTION(BlueprintCallable) void SetMaster(float V01);
UFUNCTION(BlueprintCallable) void SetSfx(float V01);
UFUNCTION(BlueprintCallable) void SetBgm(float V01);

// UI 打开时初始化滑条位置
UFUNCTION(BlueprintPure) float GetMaster() const;
UFUNCTION(BlueprintPure) float GetSfx() const;
UFUNCTION(BlueprintPure) float GetBgm() const;
```

### 4.2 职责边界（硬约束）

| 职责 | 位置 |
|---|---|
| 音量（`sfx` / `bgm` / `all` 三个全局参数） | 只在 `UAudioSettingsSubsystem` |
| 播放（`PostAkEvent`） | 各自业务 C++（Weapon / Character / UI） |
| ❌ 禁止 | 业务代码里出现 `SetRTPCValue` |

### 4.3 UI 接入

```text
WBP_SlimeSettings :: Construct
   Sub = Get Game Instance → Get Subsystem (AudioSettings)
   Slider_Master.SetValue(Sub.GetMaster())        ← 解决"滑条不知道当前值"

Slider_Master.OnValueChanged (0~1)
   Sub.SetMaster(Value)                           ← 只改一路，不覆盖另外两路

返回 / Destruct
   Sub.Save()
```

### 4.4 迁移路径

阶段 1 的 `ApplyMasterVolumeToAudio()` 保留为空壳转调 `Subsystem->SetMaster()`，UI 蓝图不用改。

---

## 5. 与现有设置页文档的冲突（必须同步）

`Docs/UI/UI_Settings_Implementation.md` 里两条记录与本方案冲突：

| ID | 现状态 | 本方案后 |
|---|---|---|
| S-11 声音 | `[!]` 「页面上没有可操作的音量控件」→ 滑块置灰 + 断线 | 音量可操作，需重新打开滑块并接线 |
| R-11 音量滑块是死控件 | `[!]` 本期不接 | 已接 Wwise，风险关闭 |

落地后请把这两条状态一起更新，并在 `Docs/UI/UI_Blueprint_Interface_Guide.md` §1 的 `SlimeSettingsWidget` 表里补 `MasterVolumeRTPC` 属性。

> 现有实现已经有持久化：`[/Script/SlimeWar.SlimeSettingsWidget] MasterVolume`（`GGameUserSettingsIni`），
> 阶段 1 直接复用它，不新增存档格式。

---

## 6. 验收清单

- [ ] Wwise 三个参数 `Default Value = 100`
- [ ] `.wwu` 中确认 `InitialValue=100`（不是只看界面）
- [ ] 重新 Generate SoundBanks 后 `.bnk` 时间戳更新
- [ ] PIE 按空格能听到 `player_footstep`
- [ ] 设置页把主音量拖到 0.3 → 重启 PIE → 音量仍是 0.3（读 ini 生效）
- [ ] 开枪 / 空枪 / 换弹事件在蓝图 `Post Event` 后正常发声
- [ ] 打包后同样有声（验证 Bank 被 cook）
- [ ] 武器 / 角色 C++ 中**没有** `SetRTPCValue`

---

## 7. 常见坑

| 现象 | 原因 | 处理 |
|---|---|---|
| 改了 Default Value 还是没声 | 没重新 Generate SoundBanks | 初始值烘在 `.bnk` 里，必须重生成 |
| 编辑器里 FF 预览事件没声 | 预览不跑蓝图，用的是 Wwise 默认值 | 依赖阶段 0 |
| 打包后静音 | 资产靠字符串 `LoadObject`，没被 cook | 改硬引用 |
| 滑条拖了没反应 | 只在 `SetMasterVolume` 里存值，没调 `ApplyMasterVolumeToAudio` | 补一行调用 |
| 三个参数互相覆盖 | 用一次性三参函数做单项调节 | 单项调节只用 `SetXxx` |
| Wwise Browser 连不上 | Authoring 未开 / 打开的不是同一工程 | 先开 `2026taptap.wproj` |
