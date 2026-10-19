# 程序任务清单与模块框架

> 依据：`策划案/策划案.md`（《粘液城市》Game Jam 策划案）
> 目标：给 2 名远程协作程序一份**不打架**的模块划分 + **先落地**的框架设计 + **可并行**的任务清单
> 约束：本文只做**模块边界、目录结构、接口契约、任务排期、协作规则**；不写具体实现代码，不做数值决策
>
> **已确认的技术决策**（2026 讨论定稿）：
> - **GAS 接入范围 = 分层方案**：**玩家全量 GAS**（ASC + AttributeSet + GA + GE），**敌人不挂 ASC**（走 `HealthComponent` 权威 + 轻量 `StateComponent`）
> - **数值纪律 = 强制**：所有 `GameplayEffect` **只放结构不放数值**，数值一律来自 `DataTable`/`DA`（见铁律 5）
> - **标签 = 原生定义**：`UE_DEFINE_GAMEPLAY_TAG`，**禁止**新建 `DefaultGameplayTags.ini`
> - **`IBattleDirector` 契约不变**：GAS 不得泄漏进 `Public/Core/*`（见铁律 6、C18 红线）

---

## 1. 总览

### 1.1 交付范围（来自策划案第 1.5 节，就这么多，不要扩）

| 系统 | 内容 |
|---|---|
| 玩家 | 单人 TPS 移动 / 瞄准 / 射击 / 换弹 / 生命 |
| 敌人 | 普通史莱姆（融合成长）、攻击性个体（**由普通个体融合产生**，非独立生成）、批量生成 |
| 敌方组织 | **公用警戒**（受击 / 发现玩家 → 局部传播、延迟、衰减）、**三类功能性队伍**（收集 / 侦察 / 巡逻） |
| 资源 | **分层资源点**（低资源点近出生点、储量有限；高资源点近地图中央、储量无限）+ 收集交付循环 |
| 局内反馈 | 计分、倒计时、点位状态、终局判定 |
| 关卡 | 单层 60×50 白盒，3 生成点 + 2 落点 |
| 演出 | 投放（2s）→ 局内（180s）→ 结算俯瞰（2~3s） |

**明确不做**：网络同步 / 存档进度 / 货币商店 / 技能树 / 跳跃冲刺翻滚 / 主动技能 / 特殊敌人 / 多关卡。

> 📌 **2026-10-11 更新**：上表第 2 行原为「普通史莱姆（融合）、攻击性史莱姆（追击）」。
> 《10月9日机制补充案》**把攻击性个体改为普通个体融合的产物**，并新增警戒 / 队伍 / 资源三套系统；
> 策划案明写「前后如有机制设计冲突以本案为主」，故按新案更新，落地任务见 **§6.6 Phase E**。
>
> **边界说明**：Phase E 的「收集队 / 侦察队 / 巡逻队」是**功能性队伍**——同一套史莱姆按任务分工，
> 不引入新物种、新技能或新关卡机制，因此**不算**"明确不做"里的"特殊敌人"。
> 若后续真要加独立的新敌人类型，仍按本条拒绝。

### 1.2 协作策略（6 条铁律）

1. **先框架，后并行**——Phase 0 由两人一起定完接口再分头开工；接口一旦冻结，改接口必须走"变更流程"（见 §7.4）。
2. **按功能分目录，不按文件类型分**——`Public/Core/`、`Private/Player/`、`Private/Enemy/`。**一个模块的所有文件（类、组件、子系统、UI）全在同一功能文件夹内**。
3. **单向依赖**——只允许依赖"层号更小"的模块；**同层之间用接口/委托通信，禁止互相 include 实现头**。
4. **谁的文件谁改，跨模块只改自己的实现**——需要别人的模块提供能力时，提需求给对方加接口，**不要自己去改对方的 .h**。
5. **【GAS 纪律】数值只准来自 DataTable/DA**——所有 `GameplayEffect` **只放结构**（改哪个属性、加法/乘法、时长、标签、免疫关系）与**表行引用**，**禁止把具体数值写进 GE 资产或蓝图**。
   > 理由：调一个数字若需开编辑器 + 合并 uasset + 走地图锁，2 人远程协作成本立刻翻倍。此规则对 GE 与 GA 蓝图同样强制。
6. **【GAS 纪律】`IBattleDirector` 是唯一对外契约**——引入 GAS 后**不得**在 `Public/Core/` 契约中暴露 `UAbilitySystemComponent` / `FGameplayAttribute` / `FActiveGameplayEffectHandle`。玩家与敌人的承伤路径差异必须被 `USlimeHealthComponent` 隔离掉，**外部只认 `OnDeath` → `IBattleDirector`**。

### 1.3 为什么这样分能减少冲突

| 冲突来源 | 本方案的处理 |
|---|---|
| 两人改同一个 `.h` | 功能分目录，每模块只有 1 个 Public 头（接口），实现细节全在 Private |
| 两人改同一个 `Build.cs` | 由 **P1 独占**，Phase 0 一次性把所有依赖模块加齐 |
| 两人改同一张表 | 按功能建 3 张 DataTable，各自独占 |
| 两人改同一张地图 | **地图锁 + 手递**协议（见 §7.3），或改用关卡实例/DataAsset 承载 |
| 两人改同一套数值 | 数值全走 DataTable/DA，**代码里禁止出现魔法数字**（铁律 5 对 GAS 同样强制） |
| 两人改同一批 GE/GA 蓝图资产 | GE **全部用 C++ 类**（`UGameplayEffect` 子类），蓝图 GE 最多留 0~2 个给策划实验；GA 同理（见 §3.6） |
| 两人抢改 `DefaultGameplayTags.ini` | **标签全部原生定义**（`UE_DEFINE_GAMEPLAY_TAG`），不新建 ini，从源头消除该冲突文件 |
| 编译互相阻塞 | 接口头保持"无实现依赖"（只用前置声明 + UHT 允许的最小 include）；GAS 头只在 `.cpp` 里 include |

---

## 2. 基线（本文写作时已存在）

| 项 | 现状 |
|---|---|
| 引擎版本 | **UE 5.5**（`SlimeWar.uproject` → `EngineAssociation: "5.5"`） |
| 工程位置 | `E:\UE\Unreal Projects\SlimeWar\SlimeWar\` |
| 模块 | 单个 `SlimeWar`（Runtime / Default） |
| 现有源码 | `SlimeWarCharacter`（第三人称模板：SpringArm + Camera + EnhancedInput 的 Move/Look/Jump）、`SlimeWarGameMode`、`SlimeWar.cpp/.h` |
| 现有依赖 | `Core, CoreUObject, Engine, InputCore, EnhancedInput`（**尚未启用 GAS**，需在 Phase 0 增补，见 §4） |
| GAS 现状 | ❌ `GameplayAbilities` 插件未启用；`Build.cs` 无 `GameplayAbilities`/`GameplayTags`/`GameplayTasks` |
| 现有资产 | 第三人称模板（Manny/Quinn、ABP、Prototype 网格材质）、`ThirdPersonMap` |
| 默认地图 / GameMode | `ThirdPersonMap` / `/Script/SlimeWar.SlimeWarGameMode` |
| 版本库 | ❌ **无 `.git`、无 `vcs.md`** —— 2 人远程协作**必须先建仓**（§7.1） |

> 结论：**现有源码是模板，可全部改造**。不会为了"复用"而保留不符合策划案的移动/输入结构。

---

## 3. 框架设计

### 3.1 分层与依赖方向（核心图）

```mermaid
flowchart TD
    L0["<b>L0 Core</b> 共享类型 / 接口 / 纯数据<br/>CoreTypes · IBattleDirector · FSlimeStatRow<br/>SlimeGameplayTags.h（原生标签）"]
    L1["<b>L1 GameplayFramework</b> 战斗基类 / 数据表加载<br/>ASlimeWarGameMode · UStatTableProvider<br/>SlimeAbilitySystemGlobals（GAS 全局）"]
    LP["<b>L2 Player</b> 玩家侧【GAS 全量】<br/>SlimeWarCharacter+IAbilitySystemInterface<br/>PlayerAttributeSet · GA_Fire/Reload/Hit/Die<br/>WeaponComponent · HealthComponent(代理)"]
    LE["<b>L2 Enemy</b> 敌人侧【GAS 分层：不挂 ASC】<br/>SlimeEnemyBase · SlimeNormal · SlimeAggro<br/>SlimeFusionComponent · HealthComponent(权威)"]
    LF["<b>L3 Flow</b> 局流程【与 GAS 无关】<br/>SpawnPoint · RunSubsystem<br/>ScoreSubsystem · GameState"]
    LU["<b>L4 UI</b> 界面<br/>HUD · 投放 · 结算"]
    LD["<b>L5 Debug</b> 调试<br/>CheatManager · DebugDraw<br/>showdebug abilitysystem"]
    LC["<b>L6 Content</b> 资产<br/>数据表 · DA · 白盒地图"]

    L0 --> L1
    L1 --> LP
    L1 --> LE
    LP --> LF
    LE --> LF
    LF --> LU
    LP --> LU
    LE --> LU
    L0 --> LD
    LF --> LD
    LF --> LC
    LU --> LC

    style L0 fill:#2d4a6b,color:#fff
    style L1 fill:#2d4a6b,color:#fff
    style LP fill:#4a6b2d,color:#fff
    style LE fill:#6b4a2d,color:#fff
    style LF fill:#5c2d6b,color:#fff
    style LU fill:#6b5c2d,color:#fff
    style LD fill:#444,color:#fff
```

**GAS 的层位（已确认方案：分层）**：
- GAS **不是一层**，而是横切 L1（全局配置）+ L2-Player（全量实现）
- **L3 Flow / L4 UI / L0 Core 内部不得出现任何 GAS 类型**（`UAbilitySystemComponent`、`FGameplayAttribute`、`FGameplayEffectSpec` 等）
- 敌人侧**不挂 ASC**：不为 180 只敌人付 `ASC + AttributeSet` 的代价；敌人状态标签用轻量 `USlimeStateComponent`（见 §3.4 ⑦）

**红色禁区（违反即为返工）**：
- ❌ Enemy → Player 的实现依赖（只允许对 `AActor*`/**接口**的依赖）
- ❌ Player / Enemy → ScoreSubsystem（**只允许通过 `IBattleDirector`**）
- ❌ Core 依赖任何上层（Core 必须零业务依赖）
- ❌ 任何模块 include 另一模块的 `Private/` 头
- ❌ **`Public/Core/*` 出现任何 GAS 类型**（契约必须与实现框架解耦，否则以后换框架要全量返工）
- ❌ **`Public/*` 头文件 include GAS 头**（`AbilitySystemComponent.h` / `GameplayEffect.h` 只在 `.cpp` include，用前置声明）
- ❌ **在 GE 资产/蓝图里写具体数值**（违反铁律 5，直接导致 uasset 冲突）

### 3.2 接口即合同：6 组跨模块契约

这 6 组东西是**两人唯一的共同工作面**，Phase 0 必须一次性定死。

```mermaid
flowchart LR
    subgraph 契约1["① CoreTypes.h"]
        E1["ETargetKind 普通/攻击性"]
        E2["ERunEndReason"]
        E3["ESpawnPointState"]
        E4["FSlimeStatRow"]
    end
    subgraph 契约2["② IBattleDirector"]
        I1["OnEnemyKilled/OnEnemyFused"]
        I2["OnPointChanged"]
        I3["OnPlayerDied/EndRun"]
    end
    subgraph 契约3["③ UStatTableProvider"]
        S1["按体量查 FSlimeStatRow"]
    end
    subgraph 契约4["④ 伤害入口（两条路径）"]
        D1["ApplyDamage（武器统一入口）"]
        D2["敌人：HealthComponent 权威"]
        D3["玩家：GE_Damage → AttributeSet<br/>HealthComponent 只读代理"]
    end
    subgraph 契约5["⑤ 数据表/DA 路径"]
        T1["DT_SlimeStats"]
        T2["DT_WeaponStats"]
        T3["DA_RunConfig"]
    end
    subgraph 契约6["⑥ 标签与状态（GAS 新增）"]
        G1["SlimeGameplayTags.h 原生标签"]
        G2["USlimeStateComponent"]
        G3["USlimePlayerAttributeSet"]
    end

    ENEMY["Enemy 模块（无 ASC）"] --> E1
    ENEMY --> I1
    ENEMY --> S1
    ENEMY --> D2
    ENEMY --> G2
    WEAPON["Player/Weapon（有 ASC）"] --> D1
    WEAPON --> D3
    WEAPON --> G3
    FLOW["Flow 模块"] --> I1
    UI["UI 模块"] --> I2
    UI --> I3
    UI --> G2
```

| 契约组 | 提供方 | 使用方 | 冻结时机 |
|---|---|---|---|
| ① `CoreTypes.h` 全部枚举/结构体 | P1 | 两人 | **Phase 0 结束前** |
| ② `IBattleDirector` | P1 声明 / P1 实现 | Enemy + UI | **Phase 0 结束前** |
| ③ `UStatTableProvider` | P1 | Enemy + UI | **Phase 0 结束前** |
| ④ 伤害入口（**两条承伤路径**，见 §3.4 ④） | P2（玩家侧）/ P1（敌人侧） | Weapon → Enemy | **Phase 0 结束前** |
| ⑤ 数据资产路径与字段 | P1 建表 / 双方填 | 两人 | **PA-06 前** |
| ⑥ **标签常量 + `USlimeStateComponent` + `AttributeSet`** | P1（标签/状态）/ P2（属性集） | 两人 | **Phase 0 结束前** |

### 3.3 目录结构（UE 5.5 默认 Public/Private 结构）

```
SlimeWar\Source\SlimeWar\
├── SlimeWar.Build.cs                     ← 【P1 独占】依赖只在这里加
├── SlimeWar.cpp / SlimeWar.h             ← 【P1 独占】仅模块启动
│
├── Public\
│   ├── Core\                             ← 【P1 独占】L0，其他模块只读
│   │   ├── SlimeWarCoreTypes.h           ← 契约①：枚举 + FSlimeStatRow
│   │   ├── BattleDirectorInterface.h     ← 契约②：UINTERFACE
│   │   ├── SlimeWarLog.h                 ← 日志分类
│   │   └── SlimeWarCVars.h               ← CVar 声明
│   ├── GameplayFramework\                ← 【P1 独占】L1
│   │   ├── SlimeWarGameMode.h
│   │   └── StatTableProvider.h           ← 契约③
│   ├── Player\                           ← 【P2 独占】L2
│   │   ├── SlimeWarCharacter.h
│   │   ├── SlimeWarPlayerController.h
│   │   ├── SlimeWeaponComponent.h
│   │   └── SlimeHealthComponent.h
│   ├── Enemy\                            ← 【P1 独占】L2
│   │   ├── SlimeEnemyBase.h
│   │   ├── SlimeNormal.h
│   │   ├── SlimeAggro.h
│   │   └── SlimeFusionComponent.h
│   ├── Flow\                             ← 【P1 独占】L3
│   │   ├── SpawnPoint.h
│   │   ├── RunSubsystem.h
│   │   ├── ScoreSubsystem.h
│   │   └── SlimeRunGameState.h
│   ├── UI\                               ← 【P2 独占】L4
│   │   ├── SlimeHUDWidget.h
│   │   ├── DeploymentWidget.h
│   │   └── ResultWidget.h
│   └── Debug\                            ← 【P1 独占】L5
│       ├── SlimeCheatManager.h
│       └── SlimeDebugDraw.h
│
└── Private\
    ├── Core\                             ← 【P1】
    ├── GameplayFramework\                ← 【P1】
    ├── Player\
    │   ├── Abilities\                    ← 【P2 独占】GAS 能力
    │   │   ├── GA_Fire.cpp/.h
    │   │   ├── GA_Reload.cpp/.h
    │   │   ├── GA_HitProtection.cpp/.h
    │   │   └── GA_Die.cpp/.h
    │   ├── Effects\                      ← 【P2 独占】GE 全部 C++ 类（铁律 5）
    │   │   ├── GE_Damage.cpp/.h
    │   │   ├── GE_FireCooldown.cpp/.h
    │   │   └── GE_Invulnerable.cpp/.h
    │   └── PlayerAttributeSet.cpp/.h
    ├── Enemy\                            ← 【P1】
    ├── Flow\                             ← 【P1】
    ├── UI\                               ← 【P2】
    └── Debug\                            ← 【P1】

SlimeWar\Content\
├── _SlimeWar\
│   ├── Core\Data\                        ← 【P1 建表，双方填】
│   │   ├── DT_SlimeStats.uasset
│   │   ├── DT_WeaponStats.uasset
│   │   ├── DA_RunConfig.uasset
│   │   └── DA_SpawnLayout.uasset
│   ├── Player\                           ← 【P2 独占】
│   │   ├── BP_SlimeWarCharacter.uasset
│   │   └── Input\                        ← IMC / IA（含射击/换弹/暂停）
│   ├── Enemy\                            ← 【P1 独占】
│   ├── Flow\                             ← 【P1 独占】
│   ├── UI\                               ← 【P2 独占】
│   └── Maps\
│       ├── L_Whitebox_CityPlaza.umap     ← 【地图锁】见 §7.3
│       └── L_Sandbox_OnePoint.umap       ← 【P1】单点联调沙盒
└── ThirdPerson\                          ← 【模板保留，不改】
```

> **GAS 资产纪律（铁律 5 的落地形式）**：
> - **GA / GE 首选 C++ 类**，`Content/_SlimeWar/Player/` 下**不建 `Abilities/`、`Effects/` 资产目录**
> - 若策划确实需要在编辑器里试参数，只允许建**至多 2 个**蓝图 GE，命名带 `_EXP` 前缀（`GE_Reload_EXP`），并在文档 §5.4 登记；试完必须把结论落回 DataTable 行
> - **绝不新建 `Config/DefaultGameplayTags.ini`**——标签走 §3.4 ⑦ 的原生定义

### 3.4 类骨架（Phase 0 要写的文件，只列声明意图）

> 说明：以下是**符号级契约**，不是实现。`// TODO` 表示数值/规则来自数据表，代码内**禁止硬编码**。

**① `Public/Core/SlimeWarCoreTypes.h`**

```cpp
UENUM(BlueprintType)
enum class ETargetKind : uint8 { Normal, Aggressive };

UENUM(BlueprintType)
enum class ESpawnPointState : uint8 { AwaitingDeploy, Spawning, DepletedNotCleared, Cleared };

UENUM(BlueprintType)
enum class ERunEndReason : uint8 { None, TimeUp, PlayerDied };

USTRUCT(BlueprintType)
struct FSlimeStatRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere) int32   Mass        = 1;      // 体量 1..8
    UPROPERTY(EditAnywhere) float   MaxHealth   = 0.f;    // 待策划确认
    UPROPERTY(EditAnywhere) float   MoveSpeed   = 0.f;    // 待策划确认
    UPROPERTY(EditAnywhere) float   BodyRadius  = 0.f;    // 待策划确认
    UPROPERTY(EditAnywhere) int32   KillScore   = 0;      // 待策划确认
    UPROPERTY(EditAnywhere) TSoftObjectPtr<UStaticMesh> Mesh;
};
```

**② `Public/Core/BattleDirectorInterface.h`** —— 全局**唯一**的跨模块通信口

```cpp
UINTERFACE(MinimalAPI, BlueprintType)
class UBattleDirector : public UInterface { GENERATED_BODY() };

class IBattleDirector
{
    GENERATED_BODY()
public:
    /** 普通目标死亡 → 按体量计分（唯一计分入口） */
    virtual void OnEnemyKilled(ETargetKind Kind, int32 Mass) = 0;
    /** 融合成功 → 只做统计/表现，不计分 */
    virtual void OnEnemyFused(int32 ResultMass) = 0;
    /** 点位状态变化 → 驱动 HUD 标记 */
    virtual void OnPointStateChanged(int32 PointId, ESpawnPointState NewState) = 0;
    /** 玩家死亡 → 立即终局（任务失败） */
    virtual void OnPlayerDied() = 0;
};
```

配套访问器（放 `GameplayFramework`，避免循环依赖）：

```cpp
// SlimeWarGameMode.h
ASlimeWarGameMode* GetSlimeGameMode(const UObject* WorldContext);  // 内部做 CastChecked + 日志
```

> **设计要点**：Enemy 与 Weapon **完全不认识计分系统**，只认识 `IBattleDirector`。这就是两人能真正并行的原因。

**③ `Public/GameplayFramework/StatTableProvider.h`**

```cpp
UCLASS()
class UStatTableProvider : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    /** 按体量查表；找不到行 → 报错并返回 CDO 默认行（降级策略） */
    UFUNCTION(BlueprintCallable)
    bool GetSlimeStat(int32 Mass, FSlimeStatRow& Out) const;

    UFUNCTION(BlueprintCallable)
    bool GetWeaponStat(FName WeaponId, FWeaponStatRow& Out) const;

protected:
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UDataTable> SlimeStatTable;
    UPROPERTY(EditDefaultsOnly) TSoftObjectPtr<UDataTable> WeaponStatTable;
};
```

**④ 伤害入口（不新建类，用引擎原生）** —— **两条承伤路径（分层方案的核心，见 §3.6）**

```cpp
// 武器：唯一开火入口（不分玩家/敌人，统一走 ApplyDamage）
UGameplayStatics::ApplyDamage(HitActor, WeaponRow.Damage, Controller, this, UDamageType::StaticClass());

// ── 敌人（无 ASC）：HealthComponent 是权威 ──
virtual float TakeDamage(float Damage, FDamageEvent const&, AController*, AActor*) override;
//   → USlimeHealthComponent::ApplyDamage() → 广播 OnHealthChanged / OnDeath

// ── 玩家（有 ASC）：HealthComponent 是 AttributeSet 的【只读代理】──
//   AttributeSet 是权威，HealthComponent 只转发，绝不持有第二份血量
virtual float TakeDamage(...) override;
//   → AbilitySystem->ApplyGameplayEffectToSelf(GE_Damage) → PostGameplayEffectExecute 处理死亡
//   → USlimeHealthComponent 订阅 AttributeSet 的变化并对外广播同一个 OnDeath
```

> **隔离原则**：两条路径的差异**必须全部关在 `USlimeHealthComponent` 内部**。
> 对外（`IBattleDirector` / UI / Flow）**只有一个 `OnDeath`**，外部代码无法也不该知道玩家用了 GAS。

**⑤ 敌人基类**（分层方案下**不挂 ASC**，只挂 `HealthComponent` + 轻量 `StateComponent`）

```cpp
UCLASS(Abstract)
class ASlimeEnemyBase : public ACharacter
{
    GENERATED_BODY()
public:
    virtual float TakeDamage(...) override;
protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USlimeHealthComponent> Health;   // ← 权威
    UPROPERTY(VisibleAnywhere) TObjectPtr<USlimeStateComponent>  State;    // ← 状态标签（非 GAS）
    UPROPERTY(EditDefaultsOnly)  ETargetKind TargetKind = ETargetKind::Normal;
    UPROPERTY(EditDefaultsOnly)  int32 Mass = 1;

    UFUNCTION() virtual void HandleDeath();
    /** 死亡：查表 → 通知 Director（Normal 才计分）→ 播喷溅 → 回调池 */
    void NotifyDirectorOnDeath();
    /** 融合后统一重算：查 FSlimeStatRow 应用血量/速度/体型/分值 */
    void ApplyStatRow(int32 NewMass);
};
```

**⑥ GAS 类骨架（玩家侧，P2 独占）**

```cpp
// ── 6.1 全局：Public/GameplayFramework/SlimeAbilitySystemGlobals.h ──
UCLASS()
class USlimeAbilitySystemGlobals : public UAbilitySystemGlobals
{
    GENERATED_BODY()
    // 用途：集中覆写 GAS 全局默认值；并在需要时保证 InitGlobalData 已执行（见 §8 Q10）
};

// ── 6.2 属性集：Private/Player/PlayerAttributeSet.h ──
UCLASS()
class USlimePlayerAttributeSet : public UAttributeSet
{
    GENERATED_BODY()
public:
    // 只放"玩家真正需要"的属性；数值一律由 DataTable 初始化，不写死
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData Health;         // 初始值来自 DA_RunConfig
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MaxHealth;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData WeaponDamage;   // 来自 DT_WeaponStats
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData MagazineSize;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData ReloadDuration;
    UPROPERTY(BlueprintReadOnly) FGameplayAttributeData FireRate;
    // ⚠️ MoveSpeed 不进 AttributeSet —— 权威仍在 CharacterMovementComponent

    virtual void PreAttributeChange(const FGameplayAttribute&, float& NewValue) override;          // Clamp
    virtual void PostGameplayEffectExecute(const FGameplayEffectModCallbackData&) override;       // 血归零 → 死亡
    ATTRIBUTE_ACCESSORS(USlimePlayerAttributeSet, Health);
    // ... 其余 ATTRIBUTE_ACCESSORS
};

// ── 6.3 角色接入：Public/Player/SlimeWarCharacter.h ──
UCLASS()
class ASlimeWarCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()
public:
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override { return AbilitySystem; }
protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<UAbilitySystemComponent>  AbilitySystem;
    UPROPERTY()                TObjectPtr<USlimePlayerAttributeSet> Attributes;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USlimeHealthComponent>    Health;   // ← 只读代理
    // BeginPlay: InitAbilityActorInfo(this, this)
    //            AbilitySystem->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);  ← 单机
};
```

**⑦ 原生标签：`Public/Core/SlimeGameplayTags.h`（P1 独占，Phase 0 一次性全量定义）**

> 放在 L0，**不依赖 GAS 的 ASC**——敌人也能用 `USlimeStateComponent` 装同一套标签做状态表达与调试。
> **禁止新建 `DefaultGameplayTags.ini`**（那会变成第三个两人争抢的冲突文件）。

```cpp
// 玩家 / 武器状态
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Deploying,       "State.Player.Deploying");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Controllable,    "State.Player.Controllable");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Dead,            "State.Player.Dead");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Result,          "State.Player.Result");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Weapon_Reloading,       "State.Weapon.Reloading");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Player_Invulnerable,    "State.Player.Invulnerable");
// 敌人 AI 状态（给 GameplayDebugger / 调试 HUD 用）
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Normal_Idle,       "State.Enemy.Normal.Idle");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Normal_Fusing,     "State.Enemy.Normal.Fusing");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Normal_MassLocked, "State.Enemy.Normal.MassLocked");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_Chasing,     "State.Enemy.Aggro.Chasing");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_WindingUp,   "State.Enemy.Aggro.WindingUp");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_Recovering,  "State.Enemy.Aggro.Recovering");
UE_DEFINE_GAMEPLAY_TAG(TAG_State_Enemy_Aggro_Cooling,     "State.Enemy.Aggro.Cooling");
// ── 预留位（本次不实现，但先定义，避免以后改契约）──
// State.Player.Stunned / State.Player.Shielded
// State.Enemy.Debuff.Burning / State.Enemy.Debuff.Slowed
```

**⑧ 轻量状态组件：`Public/Core/SlimeStateComponent.h`（P1 独占，非 GAS）**

```cpp
/** 给"不挂 ASC 的对象"（全部敌人）提供标签状态；也是玩家的统一状态查询口径 */
UCLASS(ClassGroup=(SlimeWar), meta=(BlueprintSpawnableComponent))
class USlimeStateComponent : public UActorComponent
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable) void AddStateTag(const FGameplayTag& Tag);
    UFUNCTION(BlueprintCallable) void RemoveStateTag(const FGameplayTag& Tag);
    UFUNCTION(BlueprintCallable) bool HasStateTag(const FGameplayTag& Tag) const;
    /** 供 UI / 调试 HUD / GameplayDebugger 订阅；这是唯一的状态变化出口 */
    DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnStateTagChanged, FGameplayTag, Tag, bool, bAdded);
    UPROPERTY(BlueprintAssignable) FOnStateTagChanged OnStateTagChanged;
};
```

> **为什么不让敌人也走 `LooseGameplayTag` + ASC**：180 只敌人 × (`ASC` + `AttributeSet`) 只为表达几个状态标签，是纯开销。`USlimeStateComponent` 约 30 行即可，且**对外的 `OnStateTagChanged` 接口与 ASC 一致**，以后若某个敌人真需要状态效果，可以单独把它升级成 ASC，不影响其他敌人。

---

## 4. 模块清单与归属

| # | 模块 | 层 | 归属 | 主要内容 | 依赖 |
|---|---|---|---|---|---|
| M0 | `Core` | L0 | **P1** | 枚举、`FSlimeStatRow`、`IBattleDirector`、日志、CVar、**原生标签 `SlimeGameplayTags`、`USlimeStateComponent`** | 仅引擎 + `GameplayTags` |
| M1 | `GameplayFramework` | L1 | **P1** | `ASlimeWarGameMode`（实现 `IBattleDirector`）、`UStatTableProvider`、**`USlimeAbilitySystemGlobals`** | M0 + `GameplayAbilities` |
| M2 | `Player` | L2 | **P2** | Character（**+`IAbilitySystemInterface`/ASC**）、PlayerController、Weapon、Health（**代理**）、**`USlimePlayerAttributeSet`、4×`GA_*`、3×`GE_*`** | M1 |
| M3 | `Enemy` | L2 | **P1** | `ASlimeEnemyBase`（**不挂 ASC**）、`ASlimeNormal`（融合态机）、`ASlimeAggro`（追打态机）、`USlimeFusionComponent` | M1（**不依赖 GAS 模块**） |
| M4 | `Flow` | L3 | **P1** | `ASpawnPoint`、`URunSubsystem`（计时/终局/批次）、`UScoreSubsystem`、`ASlimeRunGameState` | M2接口、M3接口 |
| M5 | `UI` | L4 | **P2** | HUD（分/倒计时/血/弹匣/点位标记）、Deployment、Result | M4接口 |
| M6 | `Debug` | L5 | **P1** | CheatManager、DebugDraw、CVar 实现、**`showdebug abilitysystem` 接线** | M0/M4 |
| M7 | `Content` | L6 | 分治 | 数据表、蓝图、白盒地图、演出 | — |

**新增引擎侧依赖（全部由 P1 在 Phase 0 一次性加进 `Build.cs`）**

| 模块 | 用途 | 必需 |
|---|---|---|
| `GameplayAbilities` | GAS 本体（ASC / GA / GE / AbilityTask） | ✅ |
| `GameplayTags` | 原生标签（`UE_DEFINE_GAMEPLAY_TAG`） | ✅ |
| `GameplayTasks` | `GameplayAbilities` 的传递依赖，显式列出更稳 | ✅ |
| `GameplayAbilitiesEditor` | 仅编辑器下用 GAS 蓝图节点/资产面板 | 🟡 视需要（放在 `if (Target.bBuildEditor)` 里） |

`SlimeWar.uproject` 需启用插件：
```json
{ "Name": "GameplayAbilities", "Enabled": true }
```

> ⚠️ **`M3 Enemy` 不得依赖 GAS 模块**——保持敌人侧与 GAS 解耦（分层方案的全部意义所在）。若发现敌人侧出现 `AbilitySystemComponent.h` 的 include，即为架构违规。

### 4.1 明确归属边界（避免扯皮）

| 资产/文件 | 唯一 Owner | 另一人怎么做 |
|---|---|---|
| `SlimeWar.Build.cs`、`SlimeWar.h/.cpp` | **P1** | 提需求，不要自己改 |
| `Public/Core/*` 全部 | **P1** | 只读引用；需加接口 → 提需求 |
| `DT_SlimeStats`、`DA_RunConfig`、`DA_SpawnLayout` | **P1** | 只填自己关心的列 |
| `DT_WeaponStats` | **P2** | P1 只读 |
| `Content/_SlimeWar/Player/*`、`UI/*` | **P2** | 不动 |
| `Content/_SlimeWar/Enemy/*`、`Flow/*` | **P1** | 不动 |
| `L_Whitebox_CityPlaza.umap` | **地图锁** | 见 §7.3 |
| `L_Sandbox_OnePoint.umap` | **P1** | P2 只用不改 |
| `Config/DefaultEngine.ini`（默认地图/GameMode） | **P1** | 不动 |
| `Public/Core/SlimeGameplayTags.h`（原生标签） | **P1** | 只读引用；需加标签 → 提需求（走 §7.4） |
| `Public/GameplayFramework/SlimeAbilitySystemGlobals.h` | **P1** | 不动 |
| `Content/_SlimeWar/Player/Abilities/*`、`Effects/*`（若存在） | **P2** | 不动 |
| `Config/DefaultGameplayTags.ini` | **禁止创建** | 标签一律原生定义（铁律 5 配套） |

### 4.2 GAS 分层纪律（方案确认项 Q2=A 的落地）

| 层 | 是否挂 `ASC` | 血量权威 | 状态表达 | 说明 |
|---|---|---|---|---|
| **玩家** | ✅ 挂 | `USlimePlayerAttributeSet` | `LooseGameplayTag` + ASC | GAS 全量：属性、GA、GE、标签 |
| **敌人** | ❌ 不挂 | `USlimeHealthComponent` | `USlimeStateComponent`（轻量） | 不为 180 只敌人付 `ASC+AttributeSet` 代价 |
| **Flow / UI / Core** | ❌ 不涉及 | — | 只读 `OnStateTagChanged` / `IBattleDirector` | 内部**禁止**出现任何 GAS 类型 |

**三条判定规则（自查用）**：
1. 文件里出现 `AbilitySystemComponent.h` 且在 `Enemy/`、`Flow/`、`UI/`、`Core/` 下 → **违规**
2. `Public/Core/*.h` 里出现 `FGameplayAttribute` / `FActiveGameplayEffectHandle` → **违规**
3. 玩家血量的可写入口不在 `AttributeSet`，或敌人血量可写入口不在 `HealthComponent` → **双重权威，违规**

**以后"某个敌人需要状态效果"时的升级路径**（不影响其他敌人）：
> 给该敌人单独加 `IAbilitySystemInterface` + 一个最小的 `USlimeEnemyAttributeSet`，**它对外仍然只通过 `OnDeath` → `IBattleDirector` 上报**——`IBattleDirector` 契约不变，玩家侧代码一行不动。

### 4.3 高风险冲突点 Top 8（重点盯防）

| 排名 | 冲突点 | 风险 | 对策 |
|---|---|---|---|
| 1 | `SlimeWar.Build.cs` | 两人各加一个模块依赖 → 必冲突 | P1 独占，Phase 0 一次性加齐（含 GAS 4 个模块） |
| 2 | `Public/Core/SlimeWarCoreTypes.h` | 两人各加一个枚举成员 → 必冲突 | Phase 0 冻结；变更走 §7.4 流程；P1 独占写权 |
| 3 | `Public/Core/SlimeGameplayTags.h` | 两人各加一个标签 → 必冲突 | 同上；Phase 0 一次性定义**含预留标签** |
| 4 | `Content/_SlimeWar/Maps/L_Whitebox_CityPlaza.umap` | 二进制不可合并 | **地图锁 + 手递**：一次只有一人可开 |
| 5 | `DT_SlimeStats` | 行/列互相覆盖 | 建表时一次性定义全部列；各填各行，禁止新增列 |
| 6 | **GE / GA 蓝图资产** | 若允许蓝图 GE 并存，会出现一批不可合并的数值资产 | **GE/GA 首选 C++ 类**；实验用蓝图 GE 上限 2 个且带 `_EXP` 前缀并登记 |
| 7 | `ASlimeWarGameMode` | 两人都往里加逻辑 | GameMode 只实现 `IBattleDirector` 转发给子系统；逻辑写进各自的 Subsystem |
| 8 | `Config/DefaultEngine.ini` | 默认地图/GameMode 被覆盖 | P1 独占 |

---

## 5. 跨模块接口契约（冻结版）

### 5.1 契约明细表

| # | 契约 | 签名 | 提供 | 消费 | 变更成本 |
|---|---|---|---|---|---|
| C1 | `ETargetKind` + **攻击能力** | 原为「生成即定类型」；**机制补充案改为运行时可获得并永久保留**（融合产出：新体量≥2 且警戒≥40，或任一亲代已有攻击能力） | P1 | 双方 | **高（牵动 Enemy 行为、计分与点位清空判定）** |
| C2 | `ESpawnPointState` | `AwaitingDeploy/Spawning/DepletedNotCleared/Cleared` | P1 | P1, P2(UI) | 低 |
| C3 | `ERunEndReason` | `None/TimeUp/PlayerDied` | P1 | P1, P2(UI) | 低 |
| C4 | `FSlimeStatRow` | `Mass/MaxHealth/MoveSpeed/BodyRadius/KillScore/Mesh` | P1 | 双方 | **高（牵动数值）** |
| C5 | `IBattleDirector::OnEnemyKilled(Kind, Mass)` | 唯一计分入口 | P1 声明 / P1 实现 | M3 | **高** |
| C6 | `IBattleDirector::OnEnemyFused(ResultMass, Location)` | 统计/表现（**Phase D 追加 `Location`**，用于"首个融合附近"教学） | P1 | M3 | 中 |
| C7 | `IBattleDirector::OnPointStateChanged(PointId, State)` | HUD 驱动 | P1 | M4→M5 | 中 |
| C8 | `IBattleDirector::OnPlayerDied()` | 立即终局 | P1 | M2 | 中 |
| C9 | `UStatTableProvider::GetSlimeStat(Mass, Out)` | 查表 | P1 | 双方 | 低 |
| C10 | `TakeDamage/ApplyDamage` | 原生 | 引擎 | M2→M3 | 低 |
| C11 | 数据资产路径 | 见 §3.3；**Phase C v1.0 `SpawnSlots` → `Points`；v1.1 删除 `FSlimeSpawnPointDef::Center`、槽位改为锚点局部坐标 `RelativeLocation`/`RelativeRotation`** | P1 | 双方 | 低 |
| C12 | 委托广播 | `OnHealthChanged/OnDeath/OnAmmoChanged/OnScoreChanged/OnTimeChanged` | 各 Owner | 各 Owner | 中 |
| C13 | 原生标签常量 | `TAG_State_*`（见 §3.4 ⑦） | P1 | 双方 | **中（Phase 0 全量定义含预留）** |
| C14 | `USlimeStateComponent` | `Add/Remove/HasStateTag` + `OnStateTagChanged` | P1 | 双方 | 低 |
| C15 | `USlimePlayerAttributeSet` | 6 个属性 + `PreAttributeChange`/`PostGameplayEffectExecute` | P2 | P2 / UI（只读） | 中 |
| C16 | 玩家承伤路径 | `TakeDamage` → `GE_Damage` → `AttributeSet` → `HealthComponent` 代理广播 | P2 | M2 | **高（分层方案唯一分叉点）** |
| C17 | 4 个能力 + 3 个 GE | `GA_Fire/Reload/HitProtection/Die`、`GE_Damage/FireCooldown/Invulnerable` | P2 | M2 | 中 |
| C18 | **GAS 隔离边界** | `Core/Flow/UI` 不得出现 GAS 类型；`Public/*` 头不得 include GAS 头 | P1 强制审查 | 双方 | **高（架构红线）** |
| C19 | **警戒值** | `0~100` 的个体数值 + 传播规则（延迟 / 半径 / 发送冷却 / 接收免疫 / 衰减）；对外只通过只读镜像暴露 | P1 | P1（AI）/ P2（调试 HUD） | 中（新增） |
| C20 | `ESlimeSquadType` | `Collection / Scout / Patrol`；队伍编制、上限与开启门槛见 §6.6 PE-12/PE-13 | P1 | P1 / P2（调试 HUD） | 中（新增） |
| C21 | **资源点与资源层级** | `ESlimeResourceTier{Low, High}` + `FSlimeResourceNodeDef`（`DA_ResourceLayout`，P1 独占）；占用 / 锁定 / 释放语义 | P1 | P1（收集队） | 中（新增） |
| C22 | **队伍变化与警报广播** | `ASlimeWarGameMode` 新增两类 multicast（队伍变化 / 警报）；`Flow` 订阅、`ASlimeRunGameState` 只读镜像。沿用 Phase C 的「GameMode 只广播、不持有逻辑」做法 | P1 声明 | P1 / UI | 中（新增） |
| C23 | `ESlimeRunState` | **Phase D 由 3 值扩为 5 值**：`Idle / Deploying / Running / Result / Ended`（`Idle` 即准备阶段，不另设 `Preparing`） | P1 | P1 / P2(UI) | 中（Phase D 已有测试覆盖迁移规则） |

### 5.2 通信方式约定

| 场景 | 用什么 | 禁止 |
|---|---|---|
| 一个对象通知多个订阅者 | **动态多播委托**（`OnDeath`、`OnScoreChanged`、`OnStateTagChanged`） | 直接互相 Cast 调用 |
| 跨模块的功能请求 | **UINTERFACE**（`IBattleDirector`） | `#include` 对方 Private 头 |
| 每帧需要的状态 | **只读 Getter** | 每帧 `FindComponentByClass` |
| 全局唯一服务 | **UGameInstanceSubsystem / UWorldSubsystem** | 全局单例指针 |
| **GAS 属性读取** | 只读 Getter / `HealthComponent` 代理 | ❗ 外部直接 `SetNumericAttributeBase` 改别人的属性 |
| **GAS 类型出现在公共头** | 前置声明 + `.cpp` 内 include | ❗ `Public/*.h` 里 include `AbilitySystemComponent.h` |

### 5.3 未来扩展如何"不改契约"（上 GAS 要兑现的承诺）

| 未来要加的内容 | 加什么 | **不需要动什么** |
|---|---|---|
| 新史莱姆类型/新敌人 | `DT_SlimeStats` 加行 + `ETargetKind` 加值（或子类） | `IBattleDirector`、玩家侧、Flow |
| 新武器 | `DT_WeaponStats` 加行 + 一个 `GA_*` | 敌人侧、Flow、Core |
| 新技能（玩家主动技） | 一个新 `GA_*` + 必要 `GE_*` | 敌人侧、Flow、Core、`IBattleDirector` |
| 玩家的新状态（眩晕/护盾/易伤） | 一个 `GE_*` + 已在 §3.4⑦ 预留的标签 + 在对应 `GA` 的 `ActivationBlockedTags` 里加一项 | 敌人侧、Flow、Core |
| **某个敌人**需要状态效果 | 单独给该敌人加 `IAbilitySystemInterface` + 最小 `AttributeSet` | **其他敌人、玩家侧、`IBattleDirector`** |
| 新关卡/新点位 | `DA_SpawnLayout` + 地图 | 全部 C++ |
| 新终局条件（主动撤离/全清模式） | `IBattleDirector` 加一个方法（走 §7.4） | 敌人侧、玩家侧 |

> **这张表就是"上 GAS 买到的扩展性"的实际内容**——重点看最后两行：**契约的稳定性来自 `IBattleDirector` 与数据驱动，而不是来自 GAS。** GAS 买到的是"新技能/新状态不用改承伤路径"。

---

## 6. 任务清单

> 规则：**每个任务只属于一个人**（Owner 列）。同一 Phase 内的任务可并行；有依赖的按序号先后。
> 状态列：`[ ]` 未开始 / `[~]` 进行中 / `[x]` 完成 / `[!]` 阻塞

### 6.0 里程碑与检查点

| 里程碑 | 内容 | 集成检查点 | 退出条件 |
|---|---|---|---|
| **Phase 0** | 框架落地：目录、CoreTypes、接口、数据表、空类 | **CP-0 骨架编译** | 两人本地能编译出空框架，数据表能加载 |
| **Phase A** | 玩家枪 + 敌人基类 + 受击/扣血 + 计分主链路 | **CP-1 主链路通** | 玩家能移动射击，史莱姆能死给分，追兵能打到玩家 |
| **Phase B** | 两只普通史莱姆融合 + 追兵攻击态机 | **CP-2 融合通** | 融合不产生击杀分；追兵可被打断，攻击可落空 |
| **Phase C** | 单点位 6 批次循环 + 单点闭环 | **CP-3 单点闭环** | 一个点能跑完 6 批并正确进入"已清空" |
| **Phase D** | 三点 + 180s + 终局 + 结算俯瞰 + HUD | **CP-4 整局闭环** | 一局完整结束，重试不残留状态 |
| **Phase E** | 机制补充案：融合重做 + 公用警戒 + 三类队伍 + 资源采集 | **CP-5 机制补充案通** | 一局里"融合成长 → 警戒传播 → 队伍生成与行动 → 资源采集交付"整条链都能观察到 |
| **Phase F** | 投放演出、爆裂、融合表现、性能压测 | **CP-6 可提交** | 干净机器能启动并完成一局 |

### 6.1 Phase 0 —— 框架落地（**两人一起，别并行**）

| # | 任务 | Owner | 依赖 | 产出 | 状态 |
|---|---|---|---|---|---|
| FZ-01 | **接口冻结评审**：逐条过 §5.1 的 **C1~C18**，双方签字 | P1+P2 | — | 评审结论 | `[ ]` |
| M0-01 | 建仓库 + 分支规范 + `.gitignore`（含 LFS）+ `vcs.md` | P1 | — | 见 §7.1 | `[ ]` |
| M0-02 | 建立 §3.3 目录骨架（空文件夹 + 迁移模板文件） | P1 | — | 目录 | `[ ]` |
| M0-03 | `SlimeWar.Build.cs` 一次性加齐依赖（**含 `GameplayAbilities`/`GameplayTags`/`GameplayTasks`**）；`uproject` 启用 `GameplayAbilities` 插件 | P1 | M0-02 | Build.cs + uproject | `[ ]` |
| M0-04 | `SlimeWarCoreTypes.h`（C1~C4） | P1 | M0-03 | 头文件 | `[ ]` |
| M0-05 | `BattleDirectorInterface.h`（C5~C8）+ `GetSlimeGameMode` 访问器 | P1 | M0-04 | 头文件 | `[ ]` |
| M0-06 | `SlimeWarLog.h` + `SlimeWarCVars.h`（声明，留空实现） | P1 | M0-03 | 头文件 | `[ ]` |
| M0-07 | `UStatTableProvider` + 3 张 DataTable 的**空表结构**（列名定死） | P1 | M0-04 | 类 + 资产 | `[ ]` |
| M0-08 | `ASlimeWarGameMode` 实现 `IBattleDirector`（空转发） | P1 | M0-05 | 类 | `[ ]` |
| M0-09 | `ASlimeEnemyBase` + `USlimeHealthComponent`（只有承伤/死亡，无 AI） | P1 | M0-05 | 类 | `[ ]` |
| M0-10 | `ASlimeWarCharacter`：删掉 Jump，只留 Move/Look/Camera | P2 | M0-02 | 类 | `[ ]` |
| M0-11 | `USlimeWeaponComponent` 空壳（只有成员与委托声明） | P2 | M0-03 | 类 | `[ ]` |
| M0-12 | `ASlimeWarPlayerController` 空壳 + 拿 Director 的辅助函数 | P2 | M0-05 | 类 | `[ ]` |
| M0-13 | `L_Sandbox_OnePoint.umap` 单点沙盒地图（P2 可直接用） | P1 | M0-09 | 地图 | `[ ]` |
| M0-14 | `USlimeCheatManager` 空壳 | P1 | M0-06 | 类 | `[ ]` |
| M0-15 | **`SlimeGameplayTags.h`：原生标签全量定义（含 §3.4⑦ 的预留位）** | P1 | M0-03 | 头文件 | `[ ]` |
| M0-16 | **`USlimeStateComponent`（轻量状态组件，非 GAS）** | P1 | M0-15 | 类 | `[ ]` |
| M0-17 | **`USlimeAbilitySystemGlobals` + 确认 `InitGlobalData` 时机**（见 §8 Q10） | P1 | M0-03 | 类 | `[ ]` |
| GAS-01 | **`USlimePlayerAttributeSet` 骨架**：6 个属性 + 两个 virtual 覆写（**初始值全部留 0，等 DataTable**） | P2 | M0-17, M0-10 | 类 | `[ ]` |
| GAS-02 | `ASlimeWarCharacter` 接 `IAbilitySystemInterface` + 挂 `ASC` + `InitAbilityActorInfo` + `ReplicationMode::Minimal` | P2 | GAS-01 | 类改造 | `[ ]` |
| GAS-03 | **属性初始化路径**：从 `DA_RunConfig`/`DT_WeaponStats` 读值写入 AttributeSet（**GE 内不含数值，铁律 5**） | P2 | GAS-02, M0-07 | 代码 | `[ ]` |
| GAS-04 | 定义 **3 个 C++ `GE` 类**（`GE_Damage` / `GE_FireCooldown` / `GE_Invulnerable`），**只放结构（属性/运算/时长/标签），不放数字** | P2 | GAS-01 | 3 个类 | `[ ]` |
| GAS-05 | 定义 **4 个 `GA` 类空壳**（Fire/Reload/HitProtection/Die），标注 `ActivationBlockedTags` | P2 | M0-15, GAS-04 | 4 个类 | `[ ]` |
| GAS-06 | **输入→能力映射**：`AbilityLocalInputPressed/Released` + 输入 ID（对齐策划案 3.1 的键位） | P2 | GAS-05 | 代码 | `[ ]` |
| GAS-07 | **`USlimeHealthComponent` 双层实现**：敌人=权威；玩家=AttributeSet 的只读代理，**对外同一个 `OnDeath`** | P2 | GAS-02, M0-09 | 类改造 | `[ ]` |
| **GAS-08** | **架构自查**：确认 `Core/Flow/UI/Enemy` 下无 GAS 类型、`Public/*.h` 无 GAS include（对应 C18 红线） | P1 | 全部 | 检查记录 | `[ ]` |
| **CP-0** | 两人本地编译通过 + 数据表能加载 + 沙盒地图能进 + `showdebug abilitysystem` 能看到玩家属性 | P1+P2 | 全部 | — | `[ ]` |

### 6.2 Phase A —— 主链路（**P1 / P2 完全并行**）

| # | 任务 | Owner | 依赖 | 状态 |
|---|---|---|---|---|
| PA-01 | 移动参数接入数据表（速度/转身/加减速） | P2 | CP-0 | `[~]` |
| PA-02 | 摄像机：TPS 肩后视角 + 灵敏度配置 | P2 | CP-0 | `[~]` |
| PA-03 | 输入映射：射击（可按住）/ 辅助瞄准 / 换弹 / 暂停 | P2 | CP-0 | `[~]` |
| PA-04 | **`GA_Fire` 实现**：LineTrace 即时命中、枪口遮挡判断、击中第一个目标即停；射速由 `GE_FireCooldown` 控制 | P2 | GAS-06, PA-03 | `[~]` |
| PA-05 | **伤害走 `GE_Damage`**：`GA_Fire` → `ApplyGameplayEffectToTarget`（替代直接 `ApplyDamage`），命中反馈占位（准星/音效钩子） | P2 | PA-04, GAS-04 | `[~]` |
| PA-06 | **`GA_Reload` 实现**：`AbilityTask_WaitDelay` + 弹匣重填；**换弹期间可移动可瞄准不可射击**；无限备弹 | P2 | GAS-05, GAS-06 | `[~]` |
| PA-07 | **`GA_HitProtection` 实现**：`GE_Invulnerable`（**0.6s** + `State.Player.Invulnerable`）+ 受伤方向闪示钩子 | P2 | GAS-04, GAS-07 | `[~]` |
| PA-08 | **玩家状态机（GAS 版）**：投放中/可操作/死亡/结算 用标签 + `ActivationBlockedTags` 表达，替代 enum + switch | P2 | GAS-02, M0-15 | `[~]` |
| PA-09 | 辅助瞄准（右键）：只做轻微吸附，不加伤害不减速 | P2 | PA-02 | `[~]` |
| PA-14 | **弹药计数归宿**：弹匣余量由谁持有（AttributeSet 还是 WeaponComponent）**定死一处**，禁止双份 | P2 | PA-06 | `[~]` |
| PA-15 | 输入阻塞接线：`State.Weapon.Reloading` / `State.Player.Dead` / `State.Player.Result` → 对应 GA 的 `ActivationBlockedTags` | P2 | PA-08 | `[~]` |
| PB-01 | `ASlimeNormal` 行为态机骨架：生成 → **等待 2s** → 找对象 | P1 | CP-0 | `[~]` |
| PB-02 | 本点位 6m 活动区约束 + 找不到对象时的**≤3m 游走 + 停 0.5~1.5s** | P1 | PB-01 | `[~]` |
| PB-03 | 对象筛选：同点位 / 未参与融合 / **体量和 ≤ 8** / 优先最近 | P1 | PB-01 | `[~]` |
| PB-04 | `ASlimeAggro` 追踪态机：**4.8 m/s** 追玩家 + 遇障碍绕行 | P1 | CP-0 | `[~]` |
| PB-05 | 攻击态机：**≤1.2m 且无遮挡且冷却完成** → 停移 + 定方向 + **蓄势 0.5s** | P1 | PB-04 | `[~]` |
| PB-06 | 蓄势结束再判定命中/落空 → **收势 0.35s** → **两次攻击起始间隔 ≥1.6s** | P1 | PB-05 | `[~]` |
| PB-07 | 攻击伤害走 `ApplyDamage`；**不击退 / 不减速 / 不隔墙 / 不瞬移** 的硬约束自查表 | P1 | PB-06 | `[~]` |
| PB-08 | 敌人死亡：查表 → `OnEnemyKilled` → 喷溅占位 → 回池（`ApplyStatRow` 复位） | P1 | M0-09 | `[~]` |
| **CP-1** | 玩家能开枪打死史莱姆并计分；追兵能靠近并打到玩家（扣血+保护生效） | P1+P2 | — | `[ ]` |

> **Phase A 实现注记（2026-10-05）**
> - 代码侧已完成并**编译通过**（`SlimeWarEditor Win64 Development`，0 error / 0 warning）；
>   `[~]` = 代码就位，等编辑器资产（见 `Docs/PhaseA/PhaseA-Checklist.md`）与 CP-1 实机验收。
> - **PA-05 口径修正**：不再写成"GA_Fire → `ApplyGameplayEffectToTarget` 替代 `ApplyDamage`"。
>   敌人不挂 ASC，GE 对敌人无效。实际实现为统一走 `USlimeCombatSubsystem::ApplyDamageTo`，
>   由它按目标有无 ASC 分流（玩家 → `GE_Damage` + SetByCaller，敌人 → `HealthComponent`）。
> - **AI 实现改为 StateTree**（决策变更）：新增 `ASlimeAIController` + `USlimeStateTreeAIComponent`
>   + 2 个 StateTree 资产 + C++ 节点（`Enemy/StateTree/SlimeStateTreeNodes.*`）。
>   所有数值仍从 `DA_RunConfig` / `DT_AggroStats` 读，StateTree 资产只放结构。
> - **PB-08 的对象池推迟**：Phase A 死亡即 `DestroyActor()`，回池留给 Phase C 的 PC-08（已留 TODO）。
> - **新增标签**：`State.Weapon.Cooldown`（由 `GE_FireCooldown` 授予）。
>   GAS 的原生冷却通过 `UGameplayAbility::GetCooldownTags()` → `UGameplayEffect::GetGrantedTags()`
>   判定，而该缓存只由 `UTargetTagsGameplayEffectComponent` 填充，所以 granted tags 必须走组件 API。
> - **新增任务**：PB-17（AI 基建：AIController / StateTree 组件 / 敌人子类）、
>   PA-17（`USlimeCombatSubsystem` 受击保护早退，原属 PA-07 的跨模块改动，由 P1 改）、
>   PB-18（调试生成命令 `SlimeSpawnNormal/SlimeSpawnAggro/SlimeClearEnemies/SlimeKillPlayer`）。

### 6.3 Phase B —— 融合与追兵完整（并行）

| # | 任务 | Owner | 依赖 | 状态 |
|---|---|---|---|---|
| PB-09 | `USlimeFusionComponent`：配对请求/接受/拒绝的握手协议 | P1 | PB-03 | `[~]` |
| PB-10 | 双方**朝彼此之间空地移动**（不穿墙）+ 靠近失败检测 | P1 | PB-09 | `[~]` |
| PB-11 | **持续接触 0.4s** 判定（双方存活、未分开、条件仍成立），任一失效即取消 | P1 | PB-10 | `[~]` |
| PB-12 | 融合结算：体量求和 → `ApplyStatRow` → **按剩余生命比例继承** → 新体量按新最大生命换算 | P1 | PB-11 | `[~]` |
| PB-13 | 融合后 **等待 1s** 才能再融合；**体量 8 锁死为不再融合** | P1 | PB-12 | `[~]` |
| PB-14 | **融合过程中被杀 → 取消融合，只结算被打死那只一次收益** | P1 | PB-12 | `[~]` |
| PB-15 | 三只同时接触：只允许两只融合，第三只等待/另找；同一只不能同时参与两次 | P1 | PB-12 | `[~]` |
| PB-16 | 追兵与普通史莱姆的碰撞关系：可被挡住但需绕行 | P1 | PB-11 | `[~]` |
| PA-10 | **死亡/终局时 `CancelAbilities`**：取消换弹、禁止射击（对齐策划案"死亡和终局取消换弹"） | P2 | PA-06, PA-08 | `[~]` |
| PA-11 | **`GA_Die` 实现**：加 `State.Player.Dead` → `CancelAbilities` → `IBattleDirector::OnPlayerDied()` | P2 | GAS-05, PA-08 | `[~]` |
| PA-16 | **受击保护验证**：`State.Player.Invulnerable` 存在时，后续命中**不扣血**、不误导为连续真实伤害（策划案 4.6.3） | P2 | PA-07 | `[~]` |
| **CP-2** | 两只史莱姆能稳定融合且**不产生击杀分**；融合中被杀能正确取消；追兵攻击前摇/落空/冷却全对 | P1+P2 | — | `[ ]` |
| PB-19 | **【延后】普通史莱姆之间不做碰撞**：`ASlimeNormal` 胶囊把 `SlimeFusion` 通道设为 `Ignore`，消除"配对取消后恢复碰撞 / 胶囊长大后裹住邻居"造成的 20~60cm 深度穿插与永久卡死（详见 PhaseB-Plan §11 已知问题 1） | P1 | CP-2 后 | `[ ]` |
| PB-20 | **【延后】融合配对距离上限**：新增 `FusionMaxPairDistance`，不再出现"隔着 4~5 米配对上、接近 2 秒空转"（实测 gap 496cm / closest 499cm） | P1 | CP-2 后 | `[ ]` |
| PB-21 | **【延后】接近空转即时取消 + 移动结果诊断**：连续若干帧重下移动请求仍无位移就立刻换目标（不再等满 `AIApproachTimeout`）；超时日志补充 `MoveTo` 返回值（`Failed/AlreadyAtGoal/RequestSuccessful`）与移动目标 | P1 | CP-2 后 | `[ ]` |

> **PB-19 ~ PB-21 是 CP-2 之后才考虑的延后项**（2026-10-06 实机反馈登记），**不阻塞 CP-2**。
> 三条的根因、证据与建议做法见 `Docs/PhaseB/PhaseB-Plan.md` → §11 已知问题与后续改进。

> **Phase B 实现注记（2026-10-06）**
> - 代码侧已完成并**编译通过**（`SlimeWarEditor Win64 Development`，0 error / 0 warning）；
>   `[~]` = 代码就位，等编辑器步骤（见 `Docs/PhaseB/PhaseB-Checklist.md`）与 CP-2 实机验收。
> - **新增类 `USlimeFusionComponent`**（`Public/Enemy/`，只挂在 `ASlimeNormal` 上；追兵没有该组件）。
>   配对是**双向握手**（请求 → 接受/拒绝）：任一方已配对就拒绝，这正是 PB-15"三只只融合两只、
>   同一只不参与两次"的实现手段，不依赖额外的仲裁器。
> - **为将来多方融合留的口子**：`ResolvePairing()` 写成对 `Participants` 的折叠（求体量和当前/最大生命），
>   `FusionMaxParticipants` 已进 `DA_RunConfig`。将来只改"放开上限 + 一条仲裁规则"，
>   StateTree 任务、敌人 API 与 `IBattleDirector` 都不动。
> - **StateTree 只改 2 条转换**：`Wander → Hold`、`Pause → Hold`，条件 `Slime: Has Fusion Target`（**不勾 Invert**），
>   且**必须排在该状态其它转换之前**（被动方在游走中被接受配对后要能自己走进融合状态）。
>   `Hold` 状态结构与节点显示名都不动，接近/接触/结算全在 `Slime: Hold Position` 的 C++ 实现里。
> - **`Slime: Has Fusion Target` 语义扩展**：不再读 pawn 上的 `FusionTarget` 镜像，而是问组件
>   "是否处于 Approaching / Contacting / Cooling"，因此 `Hold` 顺带覆盖了"融合后等待 1s"，
>   不需要新增 Cooldown 状态。
> - **`Hold` 任务永远返回 `Running`**（Phase A 亦然），靠 Condition 转换离开；不要给它接
>   `On State Completed`，否则引擎会因"未接续完成转换"把整棵树打回根状态。
> - **新增碰撞通道 `SlimeFusion`**（`Config/DefaultEngine.ini`，`ECC_GameTraceChannel1`，对象通道）：
>   `ASlimeNormal` 胶囊改用它，默认与所有通道 Block；配对期间只用 `IgnoreActorWhenMoving` 放开这一对
>   （PB-10/PB-11），世界/玩家/追兵/武器射线照旧阻挡（PB-16、PB-14 的"融合中仍可被射"）。
>   ⚠️ **任何按对象类型查敌人的代码都必须补上这个通道**——本项目只有辅助瞄准一处，已同步修改。
> - **契约扩展（走 §7.4）**：`USlimeHealthComponent::InitializeHealth(MaxHealth, HealthFraction = 1)` 与
>   `ASlimeEnemyBase::ApplyStatRow(NewMass, HealthFraction = 1)` 各加一个带默认值的参数，用于 PB-12 的
>   "按剩余生命比例继承"；默认 1.0 保证 Phase C 回池复位与追兵生成仍是满血。`IBattleDirector` 未改。
> - **PA-10 的"终局"半边推到 Phase D**：Phase B 只接"死亡 → `CancelAllAbilities`（含取消换弹）→ 禁止射击"；
>   `State.Player.Result` 的接线依赖 `URunSubsystem`，留给 Phase C/D。
> - **偏差说明**：`FSlimeSTTaskHoldPosition` 的 `DisplayName` 保持 `Slime: Hold Position` 不变（避免动 `.uasset`
>   里的节点引用）；它现在就是融合接近任务，以代码注释与本文档为准。
> - **新增调试**：CVar `Slime.Debug.DrawFusion`；控制台命令 `SlimeSpawnNormalAtMass` / `SlimeSetMass` / `SlimeForceFuse`。
> - **修正（2026-10-06，实机反馈）**：`AIApproachTimeout` 原被实现成"配对后 2 秒内必须到达"，
>   导致隔得较远的两只在全程稳定靠近的情况下也反复 `ApproachTimeout` 取消。现改为策划案 4.6.2 的
>   **"连续 2 秒没有继续靠近"**（间隔缩小量 ≥ 到达半径即重置计时）；移动请求带上紧到达半径
>   （`FusionContactTolerance`）并在会合点不可达时退化为 `MoveToActor(对方)`；超时日志补充
>   间隔/接触阈值/最近间隔/移动状态四项诊断信息。
> - **修正 2（2026-10-06，第二次实机反馈）**：`AAIController::MoveToLocation` 的到达判定默认
>   **加算胶囊半径**（`FAIMoveRequest::bReachTestIncludesAgentRadius`），使"到达 10cm 会合点"实际
>   变成"到达 ~50cm"，两只间距 1m 时 `MoveTo` 直接返回 `AlreadyAtGoal`，谁都不动（日志表现：
>   `gap` 恒定 + `move status 0`）。现改用 `FAIMoveRequest` 关闭该加算，按**圆心对圆心**判定，
>   会合点不可达时退化为直接朝对方移动。另：玩家胶囊对 `SlimeFusion` 通道改为 **Overlap**，
>   消除"玩家撞上史莱姆把它挤飞"的互相推挤（追兵仍 Block，PB-16 不受影响）。
>   仅玩家一侧不够：史莱姆自身移动扫掠仍会把玩家当阻挡物而被弹出，故史莱姆胶囊另对玩家做
>   `IgnoreActorWhenMoving`（`ASlimeEnemyBase::IgnorePlayerForMovement`，4 处刷新）。
> - **修正 3（2026-10-06，第三次实机反馈）**：
>   ① 史莱姆胶囊对 `ECC_Camera` 改为 Ignore —— 弹簧臂探针默认被 Pawn 胶囊挡住，玩家钻进史莱姆时
>   镜头会被拉进身体；
>   ② `Slime: Chase Player` 增加"移动 Idle 且玩家在攻击范围外就重新寻路"的自愈逻辑，消除
>   `is stuck and failed to move!` 后原地磨蹭；
>   ③ `USlimeCombatSubsystem::ApplyDamageTo` 对无受击路径的静物不再打 Warning（仅 CombatLog 打开时记 Log）；
>   ④ `ASlimeWarGameMode::OnEnemyKilled` 显式忽略 `Kind != Normal`，保证攻击性个体永不计分（策划案 4.6.3）。
> - **修正 4（2026-10-06，第四次实机反馈）**：
>   ① **体量表口径修正**：设计给的是**直径**（0.4m~1.2m），CSV 却按半径填了 40~120，导致 mass 6~8 的半径
>   超过胶囊半高 96，`UCapsuleComponent::SetCapsuleSize` 把半高强行撑到半径、胶囊下沉约 24~30cm 卡进地板
>   （日志 `stuck ... Actor:Floor_0 PenetrationDepth:29.85`）。CSV 改为真半径 `20 / 25.5 / 31.5 / 37 / 43 /
>   48.5 / 54.5 / 60`（追兵 30），并新增 `ApplyBodyRadius()`：改半径后半高若被动变高就同步抬高角色，
>   保证脚不离地；`SlimeSpawnNormalAtMass` 的间距也改为按体量自适应。**需要 Reimport 两张 CSV。**
>   ② **新增 Detour Crowd 寻路**：`ASlimeAIController` 改用 `UCrowdFollowingComponent`，追兵开启避让、
>   普通史莱姆仅作为障碍（保持可重叠以完成融合）；`DefaultEngine.ini` 增 `CrowdManager.MaxAgents=100`。
>   ③ **新增 `Slime.Debug.DrawAggroPath`**（默认开）：青色画追兵的计划 NavMesh 路径、橙色渐变画过去
>   约 8 秒真实轨迹、头顶标签显示移动状态与速度，用于判断避让是否真的绕开了阻挡物。
> - **延后项登记（2026-10-06）**：CP-2 之后再考虑的三项——**PB-19** 普通史莱姆互不碰撞（消除 20~60cm
>   深度穿插造成的永久卡死）、**PB-20** 融合配对距离上限（消除 4~5 米远距离配对空转）、
>   **PB-21** 接近空转即时取消 + 移动结果诊断。实机日志已确认 Detour Crowd 生效
>   （`crowd: obstacle only` / `crowd: enabled`）。详细根因与建议见 `Docs/PhaseB/PhaseB-Plan.md` §11。

### 6.4 Phase C —— 生成点与单点闭环

| # | 任务 | Owner | 依赖 | 状态 |
|---|---|---|---|---|
| PC-01 | `ASpawnPoint`：出生位（8 普通位 + 2 攻击位）、活动区、备用位数组 | P1 | CP-2 | `[~]` |
| PC-02 | 批次逻辑：**0/20/40/60/80/100s，共 6 批，每批 8 普通 + 2 攻击** | P1 | PC-01 | `[~]` |
| PC-03 | 出生点校验：可站立 且 **距玩家 ≥3m** → 否则备用位 → **最多延迟 2s** → 否则取消该只（**不补发**） | P1 | PC-02 | `[~]` |
| PC-04 | 后续批次**提前 1s 提示**（形式待定 → 先出委托，UI 再挂） | P1 | PC-02 | `[~]` |
| PC-05 | 点位四状态机：`等待投放→生成中→耗尽未清→已清空`；**清空判定只看普通目标，不含存活追兵** | P1 | PC-02 | `[~]` |
| PC-06 | `URunSubsystem`：开局启动三点、**180s 倒计时**、驱动批次 | P1 | PC-02 | `[~]` |
| PC-07 | `UScoreSubsystem`：按体量计分、总分、最佳分（失败成绩**不刷新最佳分**） | P1 | M0-05 | `[~]` |
| PC-08 | **【口径变更】不做 Actor 对象池**：改为 `USlimeEnemyManagerSubsystem` 轻量管理 + 存活/峰值统计（理由见 PhaseC-Plan §6） | P1 | PB-08 | `[~]` |
| PC-09 | `ASlimeRunGameState`：把分数/时间/点位状态暴露成只读状态 | P1 | PC-06 | `[~]` |
| PC-10 | **【v1.1 新增】生成点编辑器**：Editor 模块（视口手柄 + DA 往返 + 校验 + PIE 前一致性检查） | P1 | PC-01 | `[~]` |
| PA-12 | 计分 UI 挂钩 + 粘液图标飞入动画（纯表现）**【依赖修正：挂钩处是 HUD，不是计分子系统】** | P2 | **PD-09** | `[ ]` |
| PA-13 | 交互式调试 HUD：显示当前体量/生命/状态（进 P1 的 Debug） | P2 | PC-09 | `[~]` |
| **CP-3** | 单点跑完 6 批，正确进入"已清空"；总分与手算一致 | P1+P2 | — | `[ ]` |

> **Phase C 实现注记（2026-10-07）**
> - 代码侧已完成并**编译通过**（`SlimeWarEditor Win64 Development`，0 error / 0 warning）；
>   `[~]` = 代码就位，等编辑器资产（见 `Docs/PhaseC/PhaseC-Checklist.md`）与 CP-3 实机验收。
> - **新增类（全部在 `Source/SlimeWar/{Public,Private}/Flow/`）**：
>   `ASpawnPoint`（PC-01~PC-05）、`URunSubsystem`（PC-02/06）、`UScoreSubsystem`（PC-07）、
>   `ASlimeRunGameState`（PC-09）、`USlimeEnemyManagerSubsystem`（PC-08 降级形态）。
> - **`ASlimeWarGameMode` 改为纯广播源**：四个 `IBattleDirector` 回调只做日志 + 广播（`OnEnemyKilledEvent`
>   / `OnEnemyFusedEvent` / `OnPointStateChangedEvent` / `OnPlayerDiedEvent`），
>   Flow 侧在 `OnWorldBeginPlay` 订阅。这样 GameMode 不 include 任何 Flow 头，L1→L3 的向上依赖被消除
>   （决策 A，见 §7.4 记录）。GameMode 仍不持有任何局内逻辑。
> - **GameState 用软类路径接线**：`AGameModeBase` 把 `GameStateClass` 写死在构造函数、并在
>   `PreInitializeComponents()` 里生成 GameState，所以 `ASlimeWarGameMode` 在那里用
>   `UPROPERTY(config) TSoftClassPtr<AGameStateBase> RunGameStateClass`（`DefaultGame.ini` 指向
>   `/Script/SlimeWar.SlimeRunGameState`）覆盖，避免 include Flow 头。
> - **`UWorldSubsystem::OnWorldBeginPlay` 早于 `GameMode::StartPlay()`**（已核对引擎源码
>   `World.cpp`）：立即开跑会让 GameState / 生成点锚点错过首次广播，因此自动开跑被延后一帧，
>   且 GameState 在 `BeginPlay` 先拉一次快照再订阅。
> - **契约变更（走 §7.4）**：`USlimeSpawnLayout` 的 `TArray<FSlimeSpawnSlot> SpawnSlots`
>   替换为 `TArray<FSlimeSpawnPointDef> Points`（C11 数据资产路径与字段）。改造前该字段全工程无人读取，
>   因此没有兼容层；代价是 `DA_SpawnLayout` 需要在编辑器里重填（Checklist 步骤 3）。
> - **数值纪律**：Phase C 的 8 个新数值（批次数/间隔/每批数量/最小距离/重试窗口/重试间隔/提示提前量/自动开跑）
>   全部进 `USlimeRunConfig`，C++ 默认值故意留 0，**未填则拒绝开局并报错**，不会静默用魔法数字。
> - **终局范围**：Phase C 只保证「停止生成 + 禁止新增得分 + 广播 `OnRunEnded`」；
>   敌人/玩家的冻结与结算俯瞰仍属 Phase D（避免 Enemy→Flow 反向依赖）。
> - **新增调试**：CVar `Slime.Debug.DrawRun`（默认开，HUD 显示分数/倒计时/点位状态/存活与峰值）、
>   CVar `Slime.Run.TimeScale`；命令 `SlimeRunStart` / `SlimeRunEnd` / `SlimeRunTimeScale` / `SlimeRunStatus`。
> - **新增自动化测试**：`Private/Tests/SlimeFlowMathTests.cpp`（4 个测试，覆盖批次时间表、计分与最佳分规则、
>   点位状态机、供给与重试窗口），经 `SlimeFlowMath.h` 的纯函数驱动。

> **Phase C v1.1 修订（2026-10-07）—— 锚点即点位 + 生成点编辑器**
> - **背景**：v1.0 首次实机暴露三个缺陷——校验探针锚在"槽位填的 Z"上（Z 低于地面时探针扎进地板，必然报
>   `blocked by static geometry`）、生成点直接用该坐标且没加胶囊半高（史莱姆半埋）、`FallbackSlots` 里未填的
>   `(0,0,0)` 被当成合法坐标且它的失败原因盖住了首选位。三处均已修复。
> - **锚点即点位**：`ASpawnPoint` 的 Transform 就是点位中心与活动圈圆心；`FSlimeSpawnPointDef` 删除 `Center`，
>   `FSlimeSpawnSlot::Location` → `RelativeLocation` + `RelativeRotation`（锚点局部、含旋转）。移动/旋转锚点
>   即移动/旋转整组槽位，DA 不用改。
> - **槽位 Z 不作数**：运行时只信 XY，用 `SlimeSlotResolver::Resolve` 沿 Z 向下找地面，生成点 = 地面 +
>   胶囊半高（从生成类 CDO 读）。该解析函数是**运行时与编辑器工具的唯一共享实现**，避免两边规则漂移。
> - **新增 runtime 组件**：`USlimeSpawnSlotMarker`（`Role`/`SlotIndex`）与 `USlimeSpawnPointAnchor`，
>   都是 `bIsEditorOnly = true` + `bHiddenInGame`，只作编辑期手柄，打包被剥离、运行时零成本；DA 仍是运行时唯一数据源。
> - **新增 Editor 模块 `Source/SlimeWarEditor`**（`Type: Editor`，`PostEngineInit`；`.uproject` 与
>   `SlimeWarEditor.Target.cs` 同步登记）：`FSlimeSpawnPointVisualizer`（活动圈/连线/HUD 含同步状态）、
>   `FSlimeSpawnPointDetails`（补齐默认槽位/增删/吸附地面/校验/导出/导入）、`FSlimeSpawnSlotMarkerDetails`
>   （Remove Slot 与按角色改色）、PIE 前一致性强校验（只警告，不自动写入资产）。
>   **运行时模块不含任何编辑器代码**；升级成 ComponentVisualizer 拖拽手柄（B）或 `UEdMode`（C）只需扩这个模块。
> - **契约变更（§7.4，第二次）**：C11 —— 删除 `Center`、槽位改局部坐标；旧 `DA_SpawnLayout` 数据需用工具重摆一次。
> - **为跨模块调用补齐导出宏**：`SLIMEWAR_API` 加在 `ASpawnPoint` / `USlimeSpawnSlotMarker` /
>   `USlimeSpawnPointAnchor` / `USlimeSpawnLayout` / `USlimeRunConfig` / `USlimeGameSettings` 与 `LogSlimeWar` 上。
> - **新增自动化测试**：`Private/Tests/SlimeSpawnLayoutTests.cpp`（`SlimeWar.Spawn.LayoutArrays` /
>   `DuplicateSlots` / `AnchorSpace`）。
> - **新增任务 `PC-10`**（生成点编辑器）；`Docs/PhaseC/PhaseC-Plan.md` 升为 v1.1，
>   `PhaseC-Checklist.md` 改为"用工具摆位 + 导出 + 校验"的流程。
> - **补完 Phase A 遗留的活动半径缺口**：新增 Core 纯函数 `SlimeActivityArea`（判定 / 夹回）；
>   `ASlimeEnemyBase::InitializeFromSpawn` 增加 `ActivityRadius`，由生成点传入**该点位的有效半径**；
>   `Slime: Wander Step` 把随机游走目标夹回活动圈，漂到圈外则先走回最近圈内点；调试 HUD 改画
>   **该敌人自己的半径**（未设置才回退全局值）。设计 5.1「普通目标只在本点位活动」由此变成真实约束，
>   不再只是画出来的圈。新增测试 `SlimeWar.Enemy.ActivityArea`。

### 6.5 Phase D —— 整局闭环

| # | 任务 | Owner | 依赖 | 状态 |
|---|---|---|---|---|
| PD-01 | 扩到 **3 个点位**，同时启动，使用**相同序列** | P1 | CP-3 | `[~]` |
| PD-02 | 终局判定：**只有 生命耗尽 / 倒计时结束** 两种；达 300 分不提前结束 | P1 | PC-06 | `[~]` |
| PD-03 | 结算数据：得分 / 最低目标 / 普通击杀数 / 清空点位数 / 是否过关 | P1 | PD-02 | `[~]` |
| PD-04 | **【口径变更】重试"不残留状态"**：改为**关卡重载** + `USlimeSessionSubsystem` 跨重载记住落点 / 重试意图（原"逐项清池清场景"的写法作废；"清场景痕迹"仍依赖 Phase F 的 `PF-01`） | P1 | PD-03 | `[~]` |
| PD-05 | 局外准备页：单关卡信息（180s / 300 分 / 操作说明）+ 静态俯瞰图 | P2 | PD-01 | `[~]` |
| PD-06 | 选落点流程：点击 → 预览标记 → 再点确认（D1/D2 + 三个生成点标记） | P2 | PD-05 | `[~]` |
| PD-07 | 投放演出 **2s** + 落地 **0.5s** 镜头解锁；**演出期间世界不发展**（首批未生成，无生成/融合/追踪/伤害） | P2 | PD-06 | `[~]` |
| PD-08 | 进入局内的唯一入口：发放控制权 → 启动倒计时 → 激活所有点位与首批 | P2 | PD-07 | `[~]` |
| PD-09 | HUD：左上计分+300 目标 / 右上倒计时 / 底部生命+弹匣 / 点位状态标记 | P2 | PC-09 | `[~]` |
| PD-10 | 结算页：数字 + 重试 / 重新选落点 / 退出 | P2 | PD-03 | `[~]` |
| PD-11 | **结算俯瞰镜头 2~3s**（敌人与玩家冻结），展示喷溅痕迹 | P2 | PD-10 | `[~]` |
| PD-12 | 提示与教学：首次落地、首个融合附近、首次换弹、达标提示、最后 15 秒 | P2 | PD-09 | `[~]` |
| **PD-13** | **【本次补登】暂停菜单**：P 键呼出，继续 / 重试 / 重新选落点 / 退出（冻结世界 + 锁输入） | P2 | PD-09 | `[~]` |
| **CP-4** | 一局从选点到结算完整跑通；连续重试 3 次状态无残留 | P1+P2 | — | `[ ]` |

> **Phase D 实现注记（2026-10-07）**
> - **代码与资产均已落地并合并**（提交 `PhaseDFinish`；`SlimeWarEditor Win64 Development` 0 error；
>   自动化测试 11 项全绿）。`[~]` = 已交付，等 **CP-4 实机验收**跑完；跑完请把上表与 CP-4 改成 `[x]`。
> - **阶段机**：`ESlimeRunState` 由 3 值扩为 **5 值**（`Idle / Deploying / Running / Result / Ended`，C23）。
>   `Idle` 就是准备阶段，不另设 `Preparing`；迁移规则抽成 `SlimeFlowMath::IsValidRunPhaseTransition` 并有测试。
> - **进局唯一入口**：`URunSubsystem::BeginDeployment(DropPointIndex)` → `ConfirmDeployment()`；
>   `bAutoStartRun` 关闭后由准备页调用，`SlimeRunStart` 仍保留为跳投放的调试通道。
> - **终局两段**：`EndRun` 先"停生成 + 锁计分 + 刷最佳分 + **冻结敌人**（`ASlimeEnemyBase::SetRunFrozen`）+
>   玩家进 `State.Player.Result`（取消换弹，PA-10 终局半边）"，进 `Result` 播俯瞰，`ResultOrbitDuration` 到点才进 `Ended` 出结算页。
> - **重试 = 关卡重载**：`USlimeSessionSubsystem`（GameInstance）承载落点与重试意图；最佳分本就是 GameInstance 级，
>   因此没有"重置矩阵"要维护。**契约变更（§7.4）**：C6 追加 `Location`；C23 追加两个枚举值。
> - **UI 分层**：新增 `USlimeUISubsystem`（WorldSubsystem，L4）+ 4 个 `UUserWidget` 基类
>   （HUD / 准备 / 结算 / 暂停）+ `USlimePresentationDirector`（Level Sequence 播放）。
>   界面资产缺失时**只警告并跳过**；准备页是唯一入口，因此它缺失时自动降级为"部署到落点 0"。
> - **演出以数据为准**：投放 / 俯瞰时长取 `DeployDuration` / `ResultOrbitDuration`，
>   序列长度不一致只打 Warning；缺序列则跳过演出但流程不断。
> - **新增命令**：`SlimeRunDeploy` / `SlimeRunResult` / `SlimeRetry` / `SlimeRunSetTime` / `SlimeRunAddScore` / `SlimePause`；
>   新增测试 `SlimeWar.Flow.RunPhase` / `RunResult` / `DropPointSelection`（8 → **11 项**）。
> - **新增资产**：`L_Whitebox_CityPlaza.umap`、`WBP_SlimeHUD` / `WBP_SlimePreparation` / `WBP_SlimeResult` / `WBP_SlimePause`、
>   `LS_Deploy_D1` / `LS_Deploy_D2` / `LS_Result`、`T_Overhead_CityPlaza`（做法见 `Docs/PhaseD/*`）。

### 6.6 Phase E —— 机制补充案（警戒 / 队伍 / 资源）

> **来源**：`Docs/GameDesign/GameDesign/策划案.md` 第十二章《10月9日机制补充案》
> （同内容的 `.docx` 在 `图片和附件/机制补充案.docx`）。
> 该案明写「**前后如有机制设计冲突以本案为主**」，因此本节与 §6.2 / §6.3 / §6.4 的相关条款冲突时，**以本节为准**；
> 具体被取代的旧条款见本节末尾的「与已实现口径的冲突登记」。
>
> **数值纪律照旧**：本节所有数字（判定间隔、传播半径、编制人数……）一律进 `DA_RunConfig` / 新 DA，
> 代码里不写魔法数字（铁律 5）；缺的数值见 §8 `Q15~Q20`。

| # | 任务 | Owner | 依赖 | 状态 |
|---|---|---|---|---|
| PE-01 | 基础行为判定：每 **2~4s** 判定一次，**60% 继续游窜 / 40% 尝试融合**；融合冷却中继续游窜 | P1 | CP-4 | `[ ]` |
| PE-02 | 融合意图与配对：进入意图后原地等待 → 找**同出生点、也有融合意图**的合法对象 → 锁定 → 靠近阶段可解除 / 开始接触后仅死亡才取消 | P1 | PE-01 | `[ ]` |
| PE-03 | 融合时序数值：首次出生等待 **3s**、寻伴范围 **5m**、双方靠近各 **0.6 m/s**、接触融合 **1.5s**、成功冷却 **10s**（替换旧的 0.4s 接触 / 1s 冷却） | P1 | PE-02 | `[ ]` |
| PE-04 | 融合结果重做：体量和（上限 8）、新警戒按**体量加权平均**、新生命 = 新最大生命 × 双方当前生命和 ÷ 双方最大生命和、消耗旧个体**不计击杀分** | P1 | PE-03 | `[ ]` |
| PE-05 | **攻击能力**：新体量≥2 且警戒≥40，或任一亲代已有攻击能力 → 新个体获得并**永久保留**；`ETargetKind` 改为运行时可获得状态（契约变更 C1） | P1 | PE-04 | `[ ]` |
| PE-06 | **移除"生成点每批直接刷 2 只追兵"**：批次只产普通个体；旧 `ASlimeAggro` 直刷通道标为**过渡实现**，保留到新链路验收通过再拆 | P1 | PE-05 | `[ ]` |
| PE-07 | 警戒累积：范围 **0~100**；**受击或首次发现玩家 +40**，**有效接收传播 +20**；持续看到同一玩家不连续叠加 | P1 | CP-4 | `[ ]` |
| PE-08 | 警戒传播：新一轮警戒开始后**延迟 2s** 向**半径 4m** 传播一次；同轮刺激不重置倒计时、不增加传播次数；**发送冷却 3s**、成功接收后 **5s 接收免疫**；直接受击仍可增加 | P1 | PE-07 | `[ ]` |
| PE-09 | 警戒衰减：无新刺激 **2s 后每秒 −10**；归零后**取消尚未发送的传播** | P1 | PE-07 | `[ ]` |
| PE-10 | 警戒行为响应：普通个体**远离**刺激位置；攻击性个体与巡逻队**接近已知目标**；侦察队**接近警报发生点**；目标位置取实际观察或警报记录 | P1 | PE-07 | `[ ]` |
| PE-11 | 队伍判定与成员选择：**各出生点独立统计**；从周围 **3m** 内处于**游窜**且**未被其他队伍占用**的个体中选成员 | P1 | CP-4 | `[ ]` |
| PE-12 | 队伍编制与上限：收集队（普通史莱姆，**3~4 人优先 4**，每点上限 **5 支**）、侦察队（**体量>2 的普通**，**2 人**，上限 **2 支**）、巡逻队（**体量>2 的攻击性**，**3 人**，上限 **3 支**） | P1 | PE-11 | `[ ]` |
| PE-13 | 队伍开启门槛：**收集队 ≥3 支 → 开启侦察队判定；侦察队 ≥1 支 → 开启巡逻队判定** | P1 | PE-12 | `[ ]` |
| PE-14 | 收集队行为：成队后**暂停融合** → 搜索并锁定资源 → 前往采集 → **满载返程交付** → 出发冷却结束再收集；**不足 3 人提前返程**、交付后解散；**全队死亡立即释放资源锁定与队伍名额**；资源失效/不可达则释放锁定重新搜索 | P1 | PE-12, PE-17 | `[ ]` |
| PE-15 | 侦察队行为：2 人巡游（**半径 4m、队形间距 1m、速度取成员较小值**）；发现玩家或成员受击 → 记录位置并发**位置警报**，持续看到同一目标不重复发；警报向**同出生点**即时广播、每队**发送冷却 1s、有效期 8s**；**人数不足 2 立即停止侦察并释放名额**，存活成员返回出生点后恢复独立活动 | P1 | PE-12, PE-07 | `[ ]` |
| PE-16 | 巡逻队行为：成队后**停止融合**、沿**固定路线**巡逻、**共享成员实际观察到的目标**；发现玩家时**保存路线断点**进入交战、任务结束回断点继续；**基地外忽略基地位置警报**；基地内收到有效警报则前往记录位置搜索；返程阶段仅自卫、不接受新搜索任务；**人数不足 3 立即保存断点、关闭共享视野、解散并释放名额**，存活个体独立返回并**保留攻击能力**，新队伍形成后**接续原路线断点** | P1 | PE-12, PE-07 | `[ ]` |
| PE-17 | 资源点与分层：**低资源点**（靠出生点、储量有限）+ **高资源点**（靠地图中央、储量无限）；多个点位共用；资源点的**占用 / 锁定 / 释放**与"失效或不可达"处理 | P1 | CP-4 | `[ ]` |
| PE-18 | 交付循环与成长衔接：满载返回出生点交付资源；交付产出为后续成长系统**预留衔接位**（产出内容待 Q16） | P1 | PE-14 | `[ ]` |
| PE-19 | 数据接入：`DA_RunConfig` 新增 `AI\|Alert` / `Squad\|*` / `Resource\|*` 分组；新建 **`DA_ResourceLayout`** 承载低/高资源点与巡逻路线 | P1 | Q15, Q17 | `[ ]` |
| PE-20 | 广播与只读镜像：`ASlimeWarGameMode` 新增「**队伍变化**」「**警报**」两类 multicast（C22），`Flow` 订阅、`ASlimeRunGameState` 暴露只读镜像（沿用 Phase C 分层做法，GameMode 不持有逻辑） | P1 | PE-12, PE-15 | `[ ]` |
| PE-21 | 调试可视化：`Slime.Debug.*` 下显示**警戒值 / 队伍类型与成员 / 警报位置与有效期 / 资源点与占用**；`SlimeRunStatus` 增补只读摘要 | P1 | PE-20 | `[ ]` |
| PE-22 | 自动化测试：警戒累积·传播·衰减边界、队伍编制与开启门槛、融合新生命公式与攻击能力判定、资源锁定/释放状态机（抽纯函数，做法参照 `SlimeFlowMath`） | P2 | PE-01~PE-21 | `[ ]` |
| **CP-5** | 一局内能观察到完整链条：**融合成长 → 警戒传播 → 三类队伍生成与行动 → 资源采集交付** | P1+P2 | — | `[ ]` |

> **与已实现口径的冲突登记（以本节为准）**
>
> | 被取代的旧条款 | 位置 | 处理 |
> |---|---|---|
> | 「攻击性史莱姆」作为独立类型，由生成点**直接生成** | §1.1、§6.2 `PA-*`、§6.3 `PB-04`、§6.4 `PC-02` | 新案改为**由普通个体融合产生**（PE-05/PE-06）；旧通道保留为过渡实现，验收后拆 |
> | 每批 **8 普通 + 2 攻击** | §6.4 `PC-02` | 「2 攻击」部分作废，批次只产普通个体（PE-06）；`DA_RunConfig` 的 `Spawn Aggro Per Batch` 随之停用 |
> | 融合：接触 0.4s / 容差 10cm / 成功冷却 1s | §6.3 `PB-10~PB-13`、§8 `Q14` | 由 PE-03 的 **3s / 5m / 0.6 m/s / 1.5s / 10s** 取代；`Q14` 见 §8 注记 |
> | 融合后"按剩余生命比例继承" | §6.3 `PB-12` | 公式改为 PE-04（分母是**双方最大生命之和**，语义等价但写死在新案，以后者为准） |
> | 追兵生命 60 / 攻击伤害 20（`DT_AggroStats`） | §8 `Q6`、`Q13` | 攻击性个体现在来自融合，"用哪张表给生命/伤害"重新待定 → **`Q18`** |
>
> **尚未实现、因此本节任务全部为 `[ ]`**；`PE-06` 的"拆旧通道"必须等 `CP-5` 通过后再做。

### 6.7 Phase F —— 表现与提交

| # | 任务 | Owner | 依赖 | 状态 |
|---|---|---|---|---|
| PF-01 | 命中/死亡喷溅、场景染色、痕迹保留与**超量后最早淡出** | P2 | CP-5 | `[ ]` |
| PF-02 | 融合表现：体积/材质/轮廓变化，**形状+色彩双重区分**普通与攻击性 | P1 | CP-5 | `[ ]` |
| PF-03 | 攻击前摇的音画提示**先于伤害**，不被爆裂盖住 | P1 | CP-5 | `[ ]` |
| PF-04 | 受伤反馈：方向闪示不改变瞄准中心、镜头轻震不长期锁定 | P2 | CP-5 | `[ ]` |
| PF-05 | **性能压测：不打不融、全场峰值 180 只** 下的帧率与内存 | P1 | PC-08 | `[ ]` |
| PF-06 | 打包验证：干净环境/新机器能启动并完成一局 | P1 | PF-05 | `[ ]` |
| PF-07 | 数值易调性验收：所有数值均可**不改代码**调整 | P1 | CP-5 | `[ ]` |

> **重编号说明**：本节原为 `§6.6 Phase E`，任务号 `PE-01~PE-07`；因机制补充案插在表现阶段之前
> （新系统是玩法，应先于表现与打包），旧 Phase E 顺延为 **Phase F**，任务号改为 **`PF-01~PF-07`**，
> 检查点从 `CP-5` 改为 **`CP-6`**。依赖列已随之更新（`PF-06` 依赖 `PF-05`，其余由 `CP-4` 改为 `CP-5`）。

### 6.8 任务量分布（用于判断是否失衡）

| | P1（敌人/流程侧） | P2（玩家/UI侧） |
|---|---|---|
| Phase 0 | 15 项（M0-01~17 + GAS-08） | 10 项（M0-10~12 + GAS-01~07） |
| Phase A | 8 项 | 11 项 |
| Phase B | 8 项 | 3 项 |
| Phase C | 10 项（含 `PC-10`） | 2 项 |
| Phase D | 4 项 | 9 项（含 `PD-13`） |
| **Phase E** | **21 项**（`PE-01`~`PE-21`） | **1 项**（`PE-22`） |
| Phase F | 3 项 | 3 项 |
| **合计** | **69 项** | **39 项** |

> ⚠️ **Phase E 严重偏向 P1（21 : 1）**：警戒、三类队伍、资源系统全部落在敌人 AI 与 Flow 侧，
> 而按现有模块归属（§4.1：Enemy / Flow / Debug 归 P1），P2 在本阶段几乎没有活。三个可选缓解方向，任选或组合：
> ① 把 `PE-21` 调试可视化与 `PE-22` 测试**交给 P2**（调试绘制与测试不碰 P1 独占的实现头，改动便宜）；
> ② P2 **提前做 Phase F 的表现**（`PF-01`/`PF-04` 依赖 `CP-5`，但贴图/材质/音效资源可以先备）；
> ③ **策划侧承担数值调参与验证**（`Q15~Q20` 定稿后，P1 只负责把字段接进 DA）。
> 具体怎么分请与另一位程序确认后回填到上表。

> **P2 的增量主要来自 GAS（Phase 0 的 `GAS-01`~`GAS-07` 共 7 项）**——这是分层方案的代价集中处（玩家侧吃下全部 GAS 复杂度，敌人侧不受影响）。
> 因此 **Phase 0 的推荐分工是**：P1 铺接口/标签/GAS 全局，P2 啃 AttributeSet + Character 接入 + 三个 GE 骨架。
> **Phase 0 两人都不空转**，且地图（`L_Whitebox_CityPlaza`）此时仍无人碰，避免地图锁冲突。
> 代价估计：**GAS 增量约 +1 天**（相对于纯手写方案）。

---

## 7. 协作规则（远程 / 2 人）

### 7.1 版本库（**已作废，以 `Docs/Collaboration.md` 为准**）

> ⚠️ **2026-10-04 修订**：仓库、LFS、分支与协作规则均已落地，唯一权威是 `Docs/Collaboration.md`。
> 本节以下内容仅作历史参考，**不要再按它执行**（尤其是下面的 `dev` 分支与 `vcs.md`，实际不使用）。
> 另外新增一条硬约束：**仓库内路径必须 ASCII**（中文名/空格会让 UBT 崩溃），见 `Docs/Collaboration.md` §8。

当前工程**没有版本库**，2 人远程协作不建仓等于自杀。建议：

```
主干:      main          —— 只接受通过 CP 检查点的合并
开发:      dev           —— 日常集成
功能:      feat/<模块>-<任务号>   例: feat/enemy-PB-09
热修:      fix/<问题>
```

`vcs.md` 内容建议（模板）：

```markdown
# 版本库信息
- 远端: <git remote url>
- 主干: main / 集成分支: dev
- 分支命名: feat/<模块>-<任务号>
- 提交信息: [<任务号>] <一句话>   例: [PB-12] 实现融合体生命比例继承
- 大文件: 已启用 Git LFS（*.uasset *.umap *.ubulk）
- 地图锁: 见 .dsh/plan/程序任务清单与模块框架.md §7.3
```

> ⚠️ UE 项目**必须上 Git LFS**，否则 `.uasset`/`.umap` 会让仓库爆炸。
> ⚠️ 提交前必须**关闭编辑器**（`.uasset` 独占写锁），并确保 `.gitignore` 排除 `Binaries/ Intermediate/ Saved/ DerivedDataCache/`。

### 7.2 每日节奏

| 时间 | 动作 |
|---|---|
| 开工 | 拉 `dev`，`git merge dev` 到自己的 `feat/*` |
| 每完成一个任务 | 提交一次（粒度 = 任务号），推送 |
| 每日至少 1 次 | 合到 `dev`（**不要攒超过 1 天**） |
| 每次合并前 | 本地**编译 + 进沙盒地图跑一次** |
| 每周 | 过一次"接口是否需要变更"（见 §7.4） |

### 7.3 地图锁（二进制资产唯一解法）

`.umap` / `.uasset` **无法合并**，两人同时改等于一人白干。协议：

1. 打开地图前，在 `vcs.md`（或专用 `map-lock.md`）里写：`L_Whitebox_CityPlaza —— 锁定人: P2 —— 时间: <ts>`
2. 只有锁定人能编辑，编辑完提交并**清除锁定行**
3. 另一人**只进编辑器看，不保存**
4. 每日最多交接 1 次
5. **P2 建议改为"数据驱动布点"**：生成点/落点用 `DA_SpawnLayout`（普通 uasset，冲突面小）记录，地图里只放锚点 → 大幅减少地图编辑次数，把地图锁的使用压到最低

### 7.4 接口变更流程（保护"先框架后并行"）

```
1. 提出方在聊天里发：任务号 + 想加/改的接口签名 + 理由
2. P1（Core 所有者）确认，必要时同步给对方
3. P1 在 feat/core-<号> 分支改 Public/Core/*，单独一个提交，先合 dev
4. 双方拉新 dev，各自适配
5. 若影响已完成任务 → 记入 §8 待办
```

**禁止**：不打招呼直接在对方模块里加函数；直接改 `Public/Core/*`。

#### 契约变更记录：2026-10-19 Phase G 生态重构（M8 清退）

| 原契约 | 新契约 | 原因 |
|---|---|---|
| `IBattleDirector::OnEnemyKilled(ETargetKind, int32)` | `IBattleDirector::OnEnemyKilled(const FSlimeKillContext&)` | 生态击杀需要体量、货物、核心、来源和爆炸上下文；旧 `ETargetKind` 删除 |
| `IBattleDirector::OnEnemyFused(int32 ResultMass)` | `IBattleDirector::OnEnemyFused(const FSlimeFusionEvent&)` | 融合事件需要结果体量、阵营、核心状态、位置和路径 |
| `IBattleDirector::OnPointStateChanged(int32, ESpawnPointState)` | 删除 | 波次点位生命周期退出运行时；站点数据由 `USlimeEcologySubsystem` 提供 |
| `USlimeSpawnLayout::Points`（`FSlimeSpawnPointDef` / `FSlimeSpawnSlot`） | 仅保留 `DropPoints` | 刷点 Actor/槽位/编辑器工具已归档；布局资产只服务投放 |
| `USlimeRunConfig` 的 `bEcologyRun`、AI/Normal、AI/Aggro、旧 Fusion、Spawn 批次字段 | 删除；`RunSubsystem` 固定走生态站点 | Phase E 分队/波次设计废弃 |

兼容说明：`ESpawnPointState` 与 `USlimeHUDWidget` 的两个旧事件暂无运行时来源，仅为现有 WBP 编译保留，M9 重建 HUD 后删除。

#### 契约变更记录：2026-10-19 M9 三阵营扩展

| 契约 | 变更 | 兼容性 |
|---|---|---|
| `FSlimeRunResult` | 末尾追加 `ElapsedTime`、`PlayerDamageTaken` | 只追加字段，现有 WBP/调用方按名称读取，不破坏 |
| `USlimeEcologySubsystem` | 新增 `HasFactionSite(ESlimeFaction)` | 只新增查询 |
| `ASlimeWarCharacter` | 新增 `SelectFactionC()`、`GetTotalDamageTaken()`、`RecordDamageTaken(float)` | 玩家输入/统计增量 |
| `USlimeHUDWidget` | 新增 `OnEcologySitesUpdated(const TArray<FSlimeSiteStatus>&)` | 可选 BlueprintImplementableEvent；现有 WBP 不实现也不报错 |

M9 不修改 `OnGrenadeSelectionUpdated`、`OnPointStateUpdated`、`OnBatchIncoming` 等现有 WBP 签名。

### 7.5 责任人不存在时的兜底

| 情况 | 处理 |
|---|---|
| 对方模块阻塞自己 | 先按**接口签名**写调用方代码 + `// TODO(对方):` 注释，用桩函数/假数据继续，不要等 |
| 必须等对方 | 立刻切到自己任务清单里的**下一个无依赖任务**（每 Phase 都留了双线任务） |
| 数值未定 | 全部走 DataTable，表里先填 **占位值 + `// PLACEHOLDER` 标记**，绝不在代码里写死 |

---

## 8. 待确认事项（阻塞数值，不阻塞框架）

> 这些**不阻塞 Phase 0**（框架不写数值），但**阻塞对应阶段的验收**。来源：原策划案第 4.5 节整节缺失，
> 以及《10月9日机制补充案》未给出的几处（Q15~Q20）。

| # | 待确认 | 阻塞 | 建议默认值（占位） |
|---|---|---|---|
| Q1 | **体量表**：体量 1~8 的 `生命/速度/体积/分值` | PA/PB 全部验收 | `生命=20×体量`、`分值=10×体量`（**PLACEHOLDER，须策划确认**） |
| Q2 | 玩家最大生命 | PA-07 | 100 |
| Q3 | 玩家移动速度 | PA-01 | 6 m/s（7.1 节逆推） |
| Q4 | 弹匣容量 / 换弹时长 / 射速 | PA-06 | 8 发 / 1.0s / 10 发每秒（由"252 发 / 31 次换弹"逆推） |
| Q5 | 每发伤害 | PA-04 | 20（体量 1 一发死 + 追兵 3 发死 逆推） |
| Q6 | 攻击性个体生命 | PB-04 | 60（"36 只满血追兵需 108 次命中"逆推）。⚠️ **受机制补充案影响**（攻击性个体改由融合产生），需与 `Q18` 一起重定 |
| Q7 | 体量对应的**视觉/碰撞尺寸** | PB-08 | 体量 1 直径 0.4m，体量 8 直径 1.2m（图注仅给最大值） |
| Q8 | "后续批次提前 1 秒提示"的形式 | PC-04 | 先出委托，UI 后挂 |
| Q9 | 第 8 章表现 / 8.5 资源清单 / 9.2 分工 | PE 全部 | 文档"待填"，需 @咩 补齐 |
| Q10 | ~~**`UAbilitySystemGlobals::InitGlobalData()` 在你们 UE 5.5 分支的调用时机**~~ **【已核实，可关闭】** | GAS-03 | ✅ 已核对引擎源码：`FGameplayAbilitiesModule::GetAbilitySystemGlobals()` 在首次创建 Globals 时就会调用 `InitGlobalData()`，注释明写 "we call InitGlobalData automatically in UE5.3+"。**不需要手动调用**；但自定义 Globals 类必须登记到 `DefaultGame.ini` 的 `AbilitySystemGlobalsClassName`（已登记） |
| Q11 | 玩家弹匣余量的**唯一归属**（AttributeSet `MagazineAmmo` 还是 `USlimeWeaponComponent`） | PA-14 | 建议：**AttributeSet 只存"容量/换弹时长"等配置值；"当前余弹"放 `WeaponComponent`**（每帧变化的临时状态不宜进 AttributeSet） |
| Q12 | 实验用蓝图 GE 是否需要（铁律 5 的例外额度） | GAS-04 | 默认 **0 个**；策划确需试参数时才开，且上限 2 个并登记 |
| Q13 | 攻击性个体的**攻击伤害**（策划案未给） | PB-04/PB-07 | 20（Phase A 已按占位值落表，须策划确认）。⚠️ **受机制补充案影响**，与 `Q18` 一起重定 |
| Q14 | 融合**接触容差**与**最大参与者数**（Phase B 新增字段） | PB-10/PB-11 | ~~容差 10cm、上限 2~~ ⚠️ **已被机制补充案取代**：参与数上限仍为 2，但接触时长/寻伴范围等改按 `PE-03`（首次 3s、寻伴 5m、靠近 0.6m/s、接触 1.5s、冷却 10s），接触容差保留 10cm 作为吸收帧步长的占位 |
| Q15 | **资源点参数**：低资源点储量上限、高资源点是否真的"无限"、单次采集量与**满载容量**、采集耗时 | PE-14 / PE-17 / PE-18 | 占位：低资源点 4 次采集耗尽；满载 = 队伍人数 × 1；采集 1.5s/次（**PLACEHOLDER，须策划确认**） |
| Q16 | **收集队节奏与产出**：出发冷却时长、交付后的产出（资源交给谁、变成什么） | PE-14 / PE-18 | 占位：出发冷却 5s；交付**只做统计与表现**，不产出实际资源，等后续成长系统定稿再挂（**PLACEHOLDER**） |
| Q17 | **巡逻路线来源**：固定路线怎么定义（关卡摆点 / DA 记录 / 沿活动圈自动生成）与巡逻速度 | PE-16 / PE-19 | 占位：路线点记在新的 `DA_ResourceLayout` 里（与资源点同表、P1 独占）；巡逻速度 = 成员基础速度 × 0.6（**PLACEHOLDER**） |
| Q18 | **攻击能力与 `ETargetKind` 的关系**：复用现有枚举还是新增状态位；攻击性个体的**生命/伤害取哪张表**（连带 `Q6`/`Q13` 重定） | PE-05 / PE-06 | 占位：`ETargetKind` 保留，`Aggressive` 只在运行时被授予、不再由生成点直接指定；生命/伤害沿用 `DT_SlimeStats` 按体量，`DT_AggroStats` 降级为可选覆盖（**PLACEHOLDER**） |
| Q19 | **警戒细节**：新个体"按体量加权平均"的确切公式；+40 刺激与"警戒≥40 才获得攻击能力"是不是同一个门槛 | PE-04 / PE-05 | 占位：`(A警戒×A体量 + B警戒×B体量) ÷ (A体量 + B体量)`；是同一个 40 门槛（**PLACEHOLDER**） |
| Q20 | **上限是否随点位数缩放**：队伍"每出生点上限"与资源点数量，是否按当前 3 个点位缩放 | PE-12 / PE-17 | 占位：就是"每出生点"的**绝对值**（收集 5 / 侦察 2 / 巡逻 3），不随点数缩放（**PLACEHOLDER**） |

> ⚠️ **GAS 方案带来的额外待办（不是缺陷，是选择代价）**：玩家与敌人从此有**两条承伤路径**。
> 若最终发现不需要 GAS 的扩展收益，回退成本 = 删除玩家侧 `ASC`/`AttributeSet`/`GA`/`GE`，把玩家承伤改回 `HealthComponent` 权威——**因为对外只有 `OnDeath` → `IBattleDirector`，回退不会波及敌人侧与 Flow/UI**。这是分层方案刻意保留的"可退路"。

> ⚠️ **另有一条设计层风险**（影响 PB-12 的实现语义，需策划确认）：按 `生命=20×体量` 与"融合后按剩余比例继承"，融合会**增加全场总生命池**（两只体量 1 共 40 血 → 合体体量 2 为 40…若生命非线性则更多）。这直接决定"等待融合"是否成立。**实现上必须让这条规则完全数据驱动**，以便策划快速试数。

---

## 9. 文档规则

- 本文是**程序侧计划**，不替代策划案；**任何规则变更以策划案为源**，本文只记"怎么分工、怎么落地"。
- 任务状态在 §6 表格里就地更新（`[ ] / [~] / [x] / [!]`），不要另建进度文档。
- 接口变更记入 §7.4 流程，并在 §5.1 表格中更新"变更成本"。

---

## 10. 参考文档

> 📌 **2026-10-11 路径已切换**：策划案换成了**新版**，与旧文件并存——
> 现行版本是 `Docs/GameDesign/GameDesign/策划案.md`（含新增第十二章《10月9日机制补充案》）；
> 旧的 `Docs/GameDesign/DesignDoc.md` **已废弃**（顶部有标记），只作历史对照，**不要照着它实现**。
>
> ⚠️ 新路径含中文与空格（`GameDesign/GameDesign/图片和附件/`）。它在 `Docs/` 下、不参与 UBT 构建，
> 因此不影响编译（`Docs/Collaboration.md` §8 的"中文名让构建崩溃"针对的是 `Content/` 下的资产）。

**现行策划案与配图**（前缀 `Docs/GameDesign/GameDesign/`）

| 文档 / 图 | 路径 | 用途 |
|---|---|---|
| 策划案（现行） | `策划案.md` | 规则与体验的唯一来源（含第十二章机制补充案） |
| 机制补充案（docx） | `图片和附件/机制补充案.docx` | 与第十二章同内容，章节号更规整（1~6），**冲突时以它为准** |
| 警戒传播模拟模型 | `图片和附件/Slime_Model_Share_v1.zip` | 策划给的基础警戒传播模拟，用于对齐传播/衰减手感 |
| 整局流程图 | `图片和附件/image 9.png` | 局外→投放→局内→结算 |
| 局内决策循环 | `图片和附件/image 4.png` | 收益/生存/位置三分支 |
| 敌人行为逻辑 | `图片和附件/image.png` | **含全部 AI 数值，实现的主要依据** |
| 刷新点刷怪逻辑 | `图片和附件/image 1.png` | 批次/时序/上限 |
| 白盒示意图 | `图片和附件/image 6.png` | 坐标、距离、活动范围 |
| 补充案 图1 基础行为与融合 | `图片和附件/image 8.png` | PE-01~PE-05 |
| 补充案 图2 警戒行为与传播 | `图片和附件/image 3.png` | PE-07~PE-10 |
| 补充案 图3 队伍生成判定 | `图片和附件/image 5.png` | PE-11~PE-13 |
| 补充案 图4 收集队行动 | `图片和附件/image 10.png` | PE-14、PE-17、PE-18 |
| 补充案 图5 侦察队行动 | `图片和附件/image 2.png` | PE-15 |
| 补充案 图6 巡逻队行动与队伍失效判定 | `图片和附件/image 7.png` | PE-16 |

> **补充案章节顺序提示**：`策划案.md` 里第十二章的编号是「1 基础行为与融合 → **3 警戒机制** → **2 队伍判定**」，
> 这是转录时的错位；`.docx` 的顺序是 1 基础行为与融合 → 2 公用警戒 → 3 队伍判定 → 4 收集队 → 5 侦察队 → 6 巡逻队。
> **实现时以 `.docx` 的顺序编号为准**，本文档不擅自改策划案正文。

---

## 11. 变更记录

- `YYYY-MM-DD HH:mm`：初始创建
  > 时间戳待补：本机子进程/沙箱不可用（`SetNamedSecurityInfoW` 失败），无法读取系统时间。请在首次提交时填上真实时间。
- `YYYY-MM-DD HH:mm`：**接入 GAS（分层方案）**。变更范围：
  - §1 头部新增"已确认的技术决策"；§1.2 铁律 4 条 → **6 条**（新增 GAS 数值纪律、`IBattleDirector` 隔离纪律）
  - §1.3 冲突表新增 GE/GA 资产、`DefaultGameplayTags.ini`、GAS 头隔离三行
  - §2 基线补充 GAS 现状（插件未启用）
  - §3.1 分层图补 GAS 层位（玩家全量 / 敌人不挂 ASC）+ 新增 3 条红色禁区
  - §3.3 目录结构新增 `Private/Player/Abilities|Effects`、`PlayerAttributeSet`
  - §3.4 ④ 伤害入口拆成两条承伤路径；⑤ 敌人基类改为不挂 ASC；**新增 ⑥ GAS 类骨架、⑦ 原生标签、⑧ `USlimeStateComponent`**
  - §4 模块表新增 GAS 内容与依赖清单；**§4.2 新增"GAS 分层纪律"**；§4.3 高风险冲突点 Top 6 → **Top 8**
  - §5.1 契约 C1~C12 → **C1~C18**；§5.2 新增 GAS 通信约定；**§5.3 新增"未来扩展不改契约"对照表**
  - §6.1 新增 `M0-15~17` 与 **`GAS-01~GAS-08`**；§6.2 `PA-04~PA-08` 改为 GAS 实现口径，新增 `PA-14~PA-16`；§6.3 `PA-10/PA-11` 改为 GAS 口径
  - §6.7 任务量重算（P1 47 项 / P2 37 项）；§8 新增 `Q10~Q12` 与"可回退性"说明
- `2026-10-06`：**Phase B 落地（融合）**。变更范围：
  - §6.3 Phase B 全部任务 → `[~]`（代码完成并编译通过，等编辑器步骤与 CP-2 实机验收），并新增"Phase B 实现注记"
  - 新增 `USlimeFusionComponent`（`Public/Enemy/`）；`ASlimeNormal` 新增该组件与 `SlimeFusion` 对象通道
  - `Slime: Hold Position` 升级为融合接近任务（PB-10~PB-13）；`Slime: Has Fusion Target` 语义改为"组件是否 engaged"；
    `Slime: Select Fusion Target` 改为走双向握手
  - `USlimeHealthComponent::InitializeHealth` / `ASlimeEnemyBase::ApplyStatRow` 各新增带默认值的 `HealthFraction`
  - `GA_Die` 落地（PA-11）与 `USlimeWarCharacter::ApplyDeathEffects`（PA-10 死亡半边）；辅助瞄准补查新碰撞通道
  - 新增 CVar `Slime.Debug.DrawFusion` 与 3 条融合调试命令；§8 新增 `Q13`/`Q14`
  - 新增文档 `Docs/PhaseB/PhaseB-Plan.md`、`Docs/PhaseB/PhaseB-Checklist.md`
- `2026-10-07`：**Phase C 落地（生成点与单点闭环）**。变更范围：
  - §6.4 Phase C 任务 → `[~]`（代码完成并编译通过，等编辑器资产与 CP-3 实机验收），并新增"Phase C 实现注记"
  - 新增 `Source/SlimeWar/{Public,Private}/Flow/`：`ASpawnPoint`、`URunSubsystem`、`UScoreSubsystem`、
    `ASlimeRunGameState`、`USlimeEnemyManagerSubsystem`、纯逻辑 `SlimeFlowMath.h`
  - **契约变更（§7.4）**：`USlimeSpawnLayout::SpawnSlots` → `Points`（C11）；改造前该字段无任何读取方，无兼容层
  - **架构变更（决策 A）**：`ASlimeWarGameMode` 改为纯广播源（4 个 multicast 委托），Flow 侧订阅；
    消除 L1→L3 向上依赖，GameMode 不再 include 任何 Flow 头
  - `ASlimeWarGameMode` 新增 `UPROPERTY(config) TSoftClassPtr<AGameStateBase> RunGameStateClass`
    （`DefaultGame.ini` → `/Script/SlimeWar.SlimeRunGameState`），用于在 `PreInitializeComponents()` 覆盖 GameState 类
  - **口径变更**：PC-08 对象池 → `USlimeEnemyManagerSubsystem` 轻量管理；PA-12 计分 UI 延后到 Phase D/E
  - 新增 CVar `Slime.Debug.DrawRun`、`Slime.Run.TimeScale` 与 4 条命令
    （`SlimeRunStart` / `SlimeRunEnd` / `SlimeRunTimeScale` / `SlimeRunStatus`）
  - 新增自动化测试 `Private/Tests/SlimeFlowMathTests.cpp`（4 个测试）
  - 新增文档 `Docs/PhaseC/PhaseC-Plan.md`、`Docs/PhaseC/PhaseC-Checklist.md`
- `2026-10-07`（第二次）：**Phase C v1.1 —— 锚点即点位 + 生成点编辑器**。变更范围：
  - §6.4 新增 `PC-10 生成点编辑器`；新增"Phase C v1.1 修订"实现注记
  - **契约变更（§7.4，第二次）**：`FSlimeSpawnPointDef::Center` 删除；
    `FSlimeSpawnSlot::Location` → `RelativeLocation` + `RelativeRotation`（锚点局部、含旋转）；
    `FSlimeSpawnSlot` 去掉 `PointId` / `SlotIndex` / `Kind`
  - **出生位解析集中到** `SlimeSlotResolver`（运行时与编辑器共用）：NavMesh → 地面射线 → 地面之上的遮挡探针 →
    玩家距离；生成点 = 地面 + 胶囊半高；首次失败逐候选位打印原因。修掉 v1.0 的探针锚点/半埋/原点坑位三个缺陷
  - **新增 runtime 组件** `USlimeSpawnSlotMarker` / `USlimeSpawnPointAnchor`（`bIsEditorOnly`，编辑期手柄）
  - **新增 Editor 模块** `Source/SlimeWarEditor`（视觉化 + 详情面板按钮 + PIE 前一致性检查），
    运行时模块不含编辑器代码；`.uproject` 与 `SlimeWarEditor.Target.cs` 同步登记
  - 为跨模块调用给 `ASpawnPoint` / marker / `USlimeSpawnLayout` / `USlimeRunConfig` / `USlimeGameSettings` /
    `LogSlimeWar` 补 `SLIMEWAR_API`
  - 新增自动化测试 `Private/Tests/SlimeSpawnLayoutTests.cpp`（3 个测试）
  - `Docs/PhaseC/PhaseC-Plan.md` 升为 v1.1；`PhaseC-Checklist.md` 改为工具化流程（旧 DA 数据需重摆）
- `2026-10-07`（第三次）：**Phase D 落地（整局闭环）**。变更范围：
  - §6.5 Phase D 任务 → `[~]`（**代码与资产均已落地并合并**，提交 `PhaseDFinish`；等 CP-4 实机验收），
    并新增"Phase D 实现注记"；**补登 `PD-13` 暂停菜单**（原计划遗漏，Owner P2）
  - **契约变更（§7.4）**：C6 `OnEnemyFused` 追加 `Location`（PD-12"首个融合附近"需要位置）；
    新增 C23 `ESlimeRunState` 3 值 → **5 值**（`Idle / Deploying / Running / Result / Ended`，`Idle` 即准备阶段）
  - **阶段机与入口**：`URunSubsystem` 新增 `BeginDeployment` / `ConfirmDeployment` / `RequestRetry` / `GetRunResult`；
    `bAutoStartRun` 关闭后由准备页调用进局，`SlimeRunStart` 保留为跳投放的调试通道
  - **终局两段**：`EndRun` = 停生成 + 锁计分 + 刷最佳分 + `ASlimeEnemyBase::SetRunFrozen` 冻结敌人 +
    玩家进 `State.Player.Result`（PA-10 终局半边，取消换弹）；`Result` 播俯瞰，到点转 `Ended` 才出结算页
  - **重试 = 关卡重载**（PD-04 口径变更）：新增 `USlimeSessionSubsystem`（GameInstance）承载落点与重试意图；
    不再需要"重置矩阵"，最佳分本就跨重载保留
  - **UI 分层落地**：新增 `USlimeUISubsystem`（WorldSubsystem）+ `USlimeHUDWidget` / `SlimePreparationWidget` /
    `SlimeResultWidget` / `SlimePauseWidget` 四个基类（数据与时机在 C++、排版动画在 WBP）+
    `USlimePresentationDirector`（Level Sequence 播放）。**界面/序列缺失只警告并跳过**；
    准备页是进局唯一入口，缺失时自动降级为"部署到落点 0"
  - **演出以数据为准**：投放 / 俯瞰时长取 `DeployDuration` / `ResultOrbitDuration`，序列长度不符只打 Warning
  - 新增命令 6 条：`SlimeRunDeploy` / `SlimeRunResult` / `SlimeRetry` / `SlimeRunSetTime` / `SlimeRunAddScore` / `SlimePause`；
    新增测试 `SlimeWar.Flow.RunPhase` / `RunResult` / `DropPointSelection`（自动化测试 8 → **11 项**）
  - **新增资产**：`L_Whitebox_CityPlaza.umap`、`WBP_SlimeHUD` / `WBP_SlimePreparation` / `WBP_SlimeResult` /
    `WBP_SlimePause`、`LS_Deploy_D1` / `LS_Deploy_D2` / `LS_Result`、`T_Overhead_CityPlaza`
  - 新增文档 `Docs/PhaseD/PhaseD-Plan.md` / `PhaseD-Checklist.md` / `PhaseD-AssetGuide.md` / `PhaseD-OperationGuide.md`
- `2026-10-11`：**并入《10月9日机制补充案》（Phase E）+ Phase D 补登记**。变更范围：
  - §1.1 交付范围：敌人行改为「普通（融合成长）+ 攻击性（**由融合产生**）」，新增**敌方组织**（警戒 / 三类队伍）与**资源**两行；
    补"功能性队伍 ≠ 特殊敌人"的边界说明
  - §5.1 契约表：C1 改口径（攻击能力改为运行时可获得并永久保留）；新增 **C19 警戒值 / C20 `ESlimeSquadType` /
    C21 资源点与资源层级 / C22 队伍变化与警报广播 / C23 `ESlimeRunState` 五值**
  - §6.0 里程碑：插入 **Phase E 机制补充案（CP-5）**，旧 Phase E 顺延为 **Phase F（CP-6）**
  - **§6.6 新增 Phase E 任务表 `PE-01~PE-22` + `CP-5`**：融合重做（行为判定 / 意图配对 / 时序 / 结果 / 攻击能力 /
    拆旧直刷通道）、公用警戒（累积 / 传播 / 衰减 / 行为响应）、三类队伍（判定与编制 / 开启门槛 / 收集 / 侦察 / 巡逻）、
    资源（分层资源点 / 交付循环）、数据与调试（`DA_ResourceLayout` / 广播镜像 / 可视化 / 测试）；
    并附**与已实现口径的冲突登记表**（追兵独立生成、每批 2 攻击、旧融合数值、旧生命继承公式、Q6/Q13 全部标为被取代）
  - §6.7 Phase F（原 Phase E）：`PE-01~PE-07` → **`PF-01~PF-07`**，依赖列 `CP-4` → `CP-5`
  - §6.8 任务量分布：重编号并重算（Phase C 补 `PC-10`、Phase D 补 `PD-13`、Phase E 计入 21 + 1）
    → **合计 69 / 39 项**；新增 Phase E 严重偏向 P1 的**失衡提醒**与三个缓解方向
  - §8 待确认事项：新增 **Q15~Q20**（资源参数 / 收集队节奏与产出 / 巡逻路线来源 / 攻击能力与 `ETargetKind` /
    警戒加权公式 / 上限是否随点数缩放）；`Q10` 标记为**已核实可关闭**；`Q6` / `Q13` / `Q14` 加"受补充案影响"注记
  - §10 参考文档：切换为 `Docs/GameDesign/GameDesign/策划案.md` 与新的 11 张配图映射；
    登记 `机制补充案.docx` 与 `Slime_Model_Share_v1.zip`；注明旧 `DesignDoc.md` 已废弃；
    补"补充案章节顺序以 `.docx` 为准"的提示
  - §6.4 `PA-12` 依赖修正：`PC-07` → **`PD-09`**（挂钩处是 HUD，不是计分子系统）
  - `Docs/GameDesign/DesignDoc.md` 顶部加"已废弃"标记；`Docs/PhaseD/PhaseD-Plan.md` 的 `PE-01` 引用改为 `PF-01`
