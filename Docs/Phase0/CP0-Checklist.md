# Phase 0 · 编辑器步骤与 CP-0 验收

> 代码已就位（`Source/SlimeWar/` 全部骨架 + `Config/DefaultGame.ini`）。
> **下面 1~3 步只能在 UE 编辑器里做**：`.uasset` / `.umap` 无法在编辑器外生成。
> CSV 已经准备好了，导入即可，不用手打数值。

---

## 1. 导入两张 DataTable

目标路径：`/Game/_SlimeWar/Core/Data/`

| 源文件 | 目标资产 | Row Struct |
|---|---|---|
| `Content/_SlimeWar/Core/Data/DT_SlimeStats.csv` | `DT_SlimeStats` | `SlimeStatRow` |
| `Content/_SlimeWar/Core/Data/DT_WeaponStats.csv` | `DT_WeaponStats` | `WeaponStatRow` |

做法（二选一）：

- **推荐**：在 Content Browser 里选中 CSV → 右键 **Import** → 选择对应的 **Row Struct** → 命名与上表一致。
- 或者：右键 → Miscellaneous → Data Table → 选 Row Struct → 打开后 **Reimport** 选 CSV。

导入后核对：

- `DT_SlimeStats` 有 8 行，行名 `1`~`8`。
- `DT_WeaponStats` 有 1 行，行名 `Rifle`。

> ⚠️ 表里的数字**全部是 PLACEHOLDER**（来自计划 §8 Q1/Q4/Q5/Q7 的建议值），
> 不是策划确认值。等策划给出 4.5 节数值表后只改表，不要改代码。
> `Mesh` 列留空即可，Phase A/PE 再挂网格。

## 2. 建两个 DataAsset

同样放在 `/Game/_SlimeWar/Core/Data/`：

### 2.1 `DA_RunConfig`（Class: `SlimeRunConfig`）

 | 字段 | 填值（PLACEHOLDER） |
|---|---|
| Run Duration | 180 |
| Target Score | 300 |
| Deploy Duration | 2 |
| Result Orbit Duration | 2.5 |
| Player Max Health | 100 |
| Player Move Speed | 600（cm/s，即 6 m/s） |
| Player Turn Rate Deg Per Sec | 500 |
| Hit Protection Duration | 0.6 |
| Slime Stat Table | `DT_SlimeStats` |
| Weapon Stat Table | `DT_WeaponStats` |
| Spawn Layout | `DA_SpawnLayout`（第 3 步建完再回来选） |
| Default Weapon Id | `Rifle` |

### 2.2 `DA_SpawnLayout`（Class: `SlimeSpawnLayout`）

Phase 0 建空资产即可，Phase C（PC-01/PC-02）再填 `Spawn Slots` 与 `Drop Points`。

> `Config/DefaultGame.ini` 里已经写好软引用了：
> ```ini
> [/Script/SlimeWar.SlimeGameSettings]
> RunConfig=/Game/_SlimeWar/Core/Data/DA_RunConfig.DA_RunConfig
> DamageEffectClass=/Script/SlimeWar.GE_Damage
> ```
> 所以 `DA_RunConfig` 只要**放在这个路径、用这个名字**就会被自动找到，
> 也可以在 Project Settings → Game → Slime War 里核对。

## 3. 建沙盒地图 `L_Sandbox_OnePoint`

1. File → New Level → **Basic**（不要开 World Partition）。
2. 存到 `/Game/_SlimeWar/Maps/L_Sandbox_OnePoint`。
3. 场景内容：一块地面（Plane 或 Cube 拉平，够跑就行）+ 一个 PlayerStart。
4. 放 **1 个 `SlimeEnemyBase`** 实例（用来验证承伤链路），`Target Kind` = Normal，`Mass` = 1。
5. 打光：Directional Light + Sky Light（Basic 模板自带）。

> **不要改默认地图**：`Config/DefaultEngine.ini` 保持指向 `ThirdPersonMap`，
> 沙盒地图手动打开即可（这是 Phase 0 的既定决策）。

---

## 4. CP-0 验收清单

编译（关掉编辑器后）：

```powershell
& "E:\UE\UE_5.5\Engine\Build\BatchFiles\Build.bat" SlimeWarEditor Win64 Development -Project="E:\UE\Unreal Projects\SlimeWar\SlimeWar\SlimeWar.uproject"
```

逐条走：

| # | 检查 | 怎么验 | 期望 |
|---|---|---|---|
| 1 | 编译 | 上面那条命令 | 0 error |
| 2 | 进图 | PIE 打开 `ThirdPersonMap`，再手动打开 `L_Sandbox_OnePoint` | 能进，WASD 能动，鼠标能转视角，**空格无反应**（Jump 已删） |
| 3 | 玩家属性 | 控制台 `showdebug abilitysystem` | 能看到 6 个属性，`MaxHealth=100`、`Health=100`、`WeaponDamage=20`、`MagazineSize=8` |
| 4 | 数据表 | 控制台 `SlimeDumpTables` | 打印两张表路径 + 行名 `1`~`8` / `Rifle`，无 `Error` |
| 5 | 承伤端到端 | 在 `L_Sandbox_OnePoint` 里：`Slime.Debug.CombatLog 1` → `SlimeDamageNearestEnemy 999` | 日志依次出现：`ApplyDamageTo` → `took 999 damage` → `died` → `[BattleDirector] OnEnemyKilled: Kind=0 Mass=1` |
| 6 | 静默红线 | 见下面第 5 节 | 无输出 |
| 7 | 数值纪律 | 见下面第 5 节 | 只命中 DataTable/DataAsset 与 PLACEHOLDER 注释 |

## 5. 静态红线自查（可直接复制）

```powershell
cd "E:\UE\Unreal Projects\SlimeWar\SlimeWar\Source\SlimeWar"

# C18: Core / Enemy / Flow / UI 不得出现任何 GAS 类型
rg -n "AbilitySystem|GameplayEffect|GameplayAbility|AttributeSet" Public\Core Public\Enemy Public\Flow Public\UI Private\Core Private\Enemy Private\Flow Private\UI

# Public/Core 不得出现 GAS 类型
rg -n "AbilitySystemComponent|FGameplayAttribute|FActiveGameplayEffectHandle" Public\Core

# 重头 GAS 头只允许出现在 .cpp（AbilitySystemInterface.h 是允许的例外）
rg -n '#include "(AbilitySystemComponent|AttributeSet|GameplayEffect|GameplayAbility|AbilitySystemGlobals)\.h"' Public
```

预期：前两条**无输出**；第三条只允许 `Public/GameplayFramework/SlimeAbilitySystemGlobals.h`
（它本身就是 GAS 全局类，按修订后的 C18 属于 L1，允许）。

数值纪律抽查：

```powershell
rg -n "MaxHealth\s*=\s*[1-9]|Damage\s*=\s*[1-9]|KillScore\s*=\s*[1-9]" Source\SlimeWar
```

预期：只命中 `FSlimeStatRow`/`FWeaponStatRow` 里的 `= 0.f` 之外的默认值与注释；
任何真实的伤害/血量数字都必须出现在 DataTable/DataAsset 里。

## 6. 已知的 Phase 0 剩余 TODO（不是缺陷）

| 项 | 落在哪个任务 |
|---|---|
| `GE_Invulnerable` 还没有 GRANTED `State.Player.Invulnerable`，`GE_FireCooldown` 还没有接线 GA | PA-07 / PA-15 |
| 敌人没有 AI（只有 `ApplyStatRow` + 承伤 + 死亡回调） | PB-01 ~ PB-08 |
| 玩家 GA 只建了空壳，没有 grant 也没有 input action 资产 | PA-03 ~ PA-08 |
| 弹匣余量在 `USlimeWeaponComponent`，AttributeSet 只存配置值 | PA-14 已按此实现，Phase A 复核 |
| 融合、点位、批次、计分、UI 全部未开始 | Phase B/C/D |
