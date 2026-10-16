# USlimeHUDWidget 与 WBP_SlimeHUD 修改记录

## 修改目的

本次修改主要解决以下问题：

1. UE 5.5 `GameplayEffect` 的 Target Tags 组件创建后未注册；
2. HUD 无法正确显示运行配置中的目标分数；
3. 明确 HUD 血量控件和出生点标记的蓝图实现方式。

---

## 一、GameplayEffect 组件注册修复

### 涉及文件

- `Source/SlimeWar/Private/Player/Effects/GE_FireCooldown.cpp`
- `Source/SlimeWar/Private/Player/Effects/GE_Invulnerable.cpp`

### 原问题

两个 GameplayEffect 的构造函数中创建了：

```cpp
UTargetTagsGameplayEffectComponent
```

但没有将组件加入 `UGameplayEffect::GEComponents`。

UE 5.5 初始化 GameplayEffect 时会检查组件是否已经注册，因此会出现类似警告：

```text
Ensure condition failed: GEComponents.Contains(GEComponent)
TargetTags should be added to GEComponents during the constructor
or in PostInitProperties
```

### 修改内容

在创建组件后加入：

```cpp
GEComponents.Add(TargetTags);
```

修改后的逻辑为：

```cpp
UTargetTagsGameplayEffectComponent* TargetTags =
    CreateDefaultSubobject<UTargetTagsGameplayEffectComponent>(TEXT("TargetTags"));

GEComponents.Add(TargetTags);
```

### 修改结果

- `GE_FireCooldown` 和 `GE_Invulnerable` 的 Target Tags 组件已正确注册；
- 避免 GameplayAbilities 在初始化 GameplayEffect 时产生组件未注册警告；
- 不改变技能冷却和无敌效果的设计逻辑。

---

## 二、目标分数同步修复

### 原问题

运行配置中的目标分数已经正确传入 `UScoreSubsystem`：

```text
DA_RunConfig.TargetScore = 300
UScoreSubsystem.TargetScore = 300
```

但是 `ASlimeRunGameState` 在处理初始分数广播时只更新了当前分数：

```cpp
CurrentScore = NewScore;
```

没有同步更新：

```cpp
TargetScore
```

因此 HUD 后续读取到的目标分数仍然是默认值 `0`，导致：

- WBP 中目标分数显示为 0 或不显示；
- 蓝图进行分数比例计算时出现除零警告：

```text
Divide by zero: Divide_IntInt
```

### 涉及文件

- `Source/SlimeWar/Private/Flow/SlimeRunGameState.cpp`
- `Source/SlimeWar/Private/UI/SlimeHUDWidget.cpp`

### C++ 修改内容

#### 1. GameState 同步目标分数

在 `ASlimeRunGameState::HandleScoreChanged()` 中重新读取 ScoreSubsystem 的目标分数：

```cpp
void ASlimeRunGameState::HandleScoreChanged(int32 NewScore)
{
    CurrentScore = NewScore;

    if (const UScoreSubsystem* Score = UScoreSubsystem::Get(this))
    {
        TargetScore = Score->GetTargetScore();
    }

    OnScoreChanged.Broadcast(CurrentScore);
}
```

这样可以保证：

```text
ScoreSubsystem.TargetScore
→ SlimeRunGameState.TargetScore
```

保持同步。

#### 2. HUD 显式传递当前分数和目标分数

在 `USlimeHUDWidget::UpdateAll()` 中分别读取：

```cpp
const int32 CurrentScore = RunState->GetCurrentScore();
const int32 TargetScore = RunState->GetTargetScore();

OnScoreUpdated(CurrentScore, TargetScore);
```

在 `HandleScoreChanged()` 中同样读取目标分数：

```cpp
const int32 TargetScore =
    RunState ? RunState->GetTargetScore() : 0;

OnScoreUpdated(NewScore, TargetScore);
```

### 修改后的数据流

```text
DA_RunConfig.TargetScore
→ URunSubsystem::StartRun()
→ UScoreSubsystem::BeginRun()
→ ASlimeRunGameState::HandleScoreChanged()
→ USlimeHUDWidget::UpdateAll()
→ WBP_SlimeHUD::OnScoreUpdated(Score, TargetScore)
```

### 验证结果

使用：

```text
SlimeRunStart
SlimeRunStatus
```

确认运行日志中的目标分数为：

```text
target 300
```

并确认 HUD 能接收到：

```text
Score = 0
TargetScore = 300
```

蓝图中进行分数比例计算时，仍应在除法前判断：

```text
TargetScore > 0
```

避免目标分数为 0 时发生除零。

---

## 三、WBP_SlimeHUD 血量控件修改

### 修改内容

血量显示使用：

```text
ProgressBar
```

而不是通过 Image 的尺寸缩放模拟血条。

蓝图事件：

```text
OnHealthUpdated(Health, MaxHealth)
```

中计算：

```text
Health / MaxHealth
```

然后将结果传给：

```text
ProgressBar.SetPercent
```

建议在除法前确认：

```text
MaxHealth > 0
```

并将结果限制在：

```text
0.0 ~ 1.0
```

---

## 四、WBP_SlimeHUD 出生点标记颜色修改

### 修改内容

右上角三个出生点标记的默认颜色在 WBP 中改为白色。

这里的三个标记表示三个地图出生点的状态，不表示三个波次。

未匹配上PointId为蓝色

颜色状态由 `NewState` 决定：

| 状态 | 含义 | 建议颜色 |
|---|---|---|
| `AwaitingDeploy` | 尚未开始生成 | 灰色 |
| `Spawning` | 正在生成史莱姆 | 黄色 |
| `DepletedNotCleared` | 已生成完，但仍有存活史莱姆 | 橙色 |
| `Cleared` | 该出生点的普通史莱姆已清空 | 绿色 |

`PointId` 只决定更新哪一个标记：

```text
PointId = 1 → 第一个标记
PointId = 2 → 第二个标记
PointId = 3 → 第三个标记
```

具体映射必须以地图中 `ASpawnPoint` 的 `Point Id` 配置为准。
