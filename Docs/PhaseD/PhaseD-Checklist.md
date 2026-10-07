# Phase D · 编辑器步骤与 CP-4 验收

> 代码侧已落地（阶段机 / 结算 / 重试 / 暂停 / UI 基类 / 序列播放）。
> 下面第 2~6 步只能在你自己的机器上做（`.uasset` / `.umap` 无法在编辑器外生成）。
> 顺序有依赖：**先填 `DA_RunConfig`（含 `Auto Start Run` 取消勾选），再建地图与序列，最后进 PIE**。
>
> ⚠️ **最容易踩的一条**：`bAutoStartRun` 若仍是勾选状态，准备页会被跳过、直接进局内，
> 看起来就像"UI 没生效"。

---

## 1. 打开工程

1. 关掉正在运行的编辑器。
2. 双击 `SlimeWar.uproject`；若提示需要重新编译模块，选 Yes。
   本次 UI 基类与序列播放引入了 `UMG` / `LevelSequence` 模块依赖，Output Log 里不应出现加载失败。
3. 确认 Output Log 没有 `USlimeUISubsystem: ... widget class is not set` 之类的警告
   （有也没关系，见第 3 步——只是说明 UI 还没接上）。

---

## 2. 给 `DA_RunConfig` 填 Phase D 字段

打开 `/Game/_SlimeWar/Core/Data/DA_RunConfig`：

| 分类 | 字段 | 值 | 说明 |
|---|---|---|---|
| Run | **Auto Start Run** | **取消勾选** | Phase D 由 `BeginDeployment` 开局，不再自动开始 |
| Run | Deploy Duration | `2.0` | 投放演出时长 = `LS_Deploy_*` 的序列长度 |
| Run | Result Orbit Duration | `2.5` | 结算俯瞰时长 = `LS_Result` 的序列长度 |
| Run | Run Duration / Target Score | `180` / `300` | Phase C 已填，确认即可 |
| Tutorial | Tutorial Fusion Proximity | `1500` | "首个融合附近"的判定半径（cm）；填 0 等于不限制 |

> 序列长度与 `Deploy Duration` / `Result Orbit Duration` 不一致时，启动会打 Warning；
> **玩法门控以 DA 值为准**，序列只是表现。

---

## 3. 接上 4 个 UI 控件

`Config/DefaultGame.ini` 里已经有这四行（由代码提交时写入，路径先指向不存在的资产也不会崩）：

```ini
[/Script/SlimeWar.SlimeGameSettings]
HUDWidgetClass=/Game/_SlimeWar/UI/WBP_SlimeHUD.WBP_SlimeHUD_C
PreparationWidgetClass=/Game/_SlimeWar/UI/WBP_SlimePreparation.WBP_SlimePreparation_C
ResultWidgetClass=/Game/_SlimeWar/UI/WBP_SlimeResult.WBP_SlimeResult_C
PauseWidgetClass=/Game/_SlimeWar/UI/WBP_SlimePause.WBP_SlimePause_C
DeploySequences=...
ResultSequence=...
```

也可以在 **Project Settings → Game → Slime War** 里改（改完写回 ini）。
四个 `WBP` 的**父类**必须分别是：

| WBP | 父类（C++） |
|---|---|
| `WBP_SlimeHUD` | `USlimeHUDWidget` |
| `WBP_SlimePreparation` | `USlimePreparationWidget` |
| `WBP_SlimeResult` | `USlimeResultWidget` |
| `WBP_SlimePause` | `USlimePauseWidget` |

C++ 只负责"什么时候显示、数据是多少"，具体排版与动画在 WBP 里用 **BlueprintImplementableEvent** 实现
（事件名见各自的头文件，例如 `OnScoreChanged` / `OnPhaseChanged` / `OnResultReady`）。

> **第 4~6 步没做完之前**：缺哪个界面就跳过哪个界面，不会崩。但要特别注意**准备页**——
> 它是进入局内的唯一入口，所以缺准备页时 UI 子系统会自动**部署到落点 0**，并在日志里说明；
> 若 `DA_SpawnLayout::DropPoints` 也是空的，它会再退一步直接开始倒计时。
> 换句话说：**准备页没做之前，游戏不会卡在准备阶段，但也不会让你选落点**。
> 想在没 UI 的情况下直接进局内，控制台敲 `SlimeRunStart` 最省事。

---

## 4. 新建三点位白盒地图（PD-01）

1. 新建关卡并另存为 `Content/_SlimeWar/Maps/L_Whitebox_CityPlaza`。
2. 按设计 7.1 摆白盒：单层 **60 m × 50 m**，主路宽 4~6 m、环路互通、无跳跃点、
   四周用墙体或围栏封边（**不设可坠落边缘**）。
3. 放三根 `ASpawnPoint` 锚点，`Point Id` 分别填 `1` / `2` / `3`，位置对应
   **A(15,15) / B(45,15) / C(30,38)**（单位：米）。锚点放在点位中心，不是第一个出生位。
4. 每个点位周围留出**直径 ≥10 m** 的可移动空地；三点之间不要被同一处 18 m 射程站位全覆盖。
5. 放一个覆盖全场的 `NavMeshBoundsVolume`；放一个 `PlayerStart`（准备页阶段的站立点，位置不敏感）。
6. 分别选中三根锚点 → Details 面板 **Spawn Point Editor** → `Create Default Slots`
   （8 普通 + 2 追兵 + 3 备用）→ 视口里拖拽微调 → `Validate Layout` 直到没有 `BAD` →
   `Export All Points In Level` → **保存关卡与 `DA_SpawnLayout`**。
7. 打开 `DA_SpawnLayout`，确认 `Points` 有 3 条（`PointId` 1/2/3），
   `Drop Points` 为 **D1(800, 800, 0)、D2(4500, 3800, 0)**。
   ⚠️ **`DropPoints` 的单位是 cm**（和槽位一致）：设计图上的 D1(8,8)、D2(45,38) 是米，要 ×100。
   Z 填 0 即可——`BeginDeployment` 会把玩家放在落点上方 150cm 再靠重力落地。

> 生成点编辑器的完整用法见 [PhaseC-Checklist.md §3](../PhaseC/PhaseC-Checklist.md)。

---

## 5. 制作 3 条 Level Sequence（PD-07 / PD-11）

放在 `Content/_SlimeWar/UI/Cinematics/`：

| 资产 | 长度 | 内容 |
|---|---|---|
| `LS_Deploy_D1` | 2.0 s | 镜头从 D1 上方俯冲到落点高度 |
| `LS_Deploy_D2` | 2.0 s | 同上，起点按 D2 的位置 |
| `LS_Result` | 2.5 s | 覆盖三个点位的俯瞰环绕（或推轨） |

1. 用 **Camera Cut Track** + 一个 `Cine Camera Actor`；若不想放相机进关卡，
   在 Sequencer 里用 **Spawnable** 相机。
2. 序列里**不要**放任何会与玩法冲突的轨道（不要动敌人、不要动玩家）；
   结算俯瞰的"定住"由代码负责（`SetRunFrozen`），不靠 Sequencer。
3. 把三条序列按顺序填进 **Project Settings → Game → Slime War** 的 `DeploySequences`
   （下标 = 落点序号：0 是 D1，1 是 D2）与 `ResultSequence`。
4. 自检：进 PIE 时若序列长度与 DA 不符，Output Log 会出现
   `sequence length ... does not match DeployDuration ...` 的警告，按提示改长度或改 DA 值。

---

## 6. 制作 4 个 WBP 与俯瞰贴图

1. `T_Overhead_CityPlaza`：一张正交俯瞰图（准备页背景）。
2. `WBP_SlimePreparation`：背景贴图 + 5 个标记（3 个生成点只读、2 个落点可点）。
   标记位置**手工摆放**，只把索引对上 `DA_SpawnLayout::DropPoints` 的顺序（0=D1，1=D2）。
   交互三段：悬停高亮 → 点击出预览标记 + 确认按钮 → 再次点击或按确认 →
   调用 C++ 的 `ConfirmDropPoint(Index)`。
3. `WBP_SlimeHUD`：左上 分数 + 300 目标、右上 倒计时、底部 生命 + 弹匣、3 个点位标记。
   点位标记只做三态（生成中 / 耗尽未清 / 已清空），**不显示精确潜在分数**。
4. `WBP_SlimeResult`：得分 / 最低目标 / 普通击杀数 / 清空点位数 / 是否过关（+ 最佳分）；
   三个按钮分别调用 `RequestRetry()` / `RequestReselect()` / `RequestQuit()`。
5. `WBP_SlimePause`：继续 / 重试 / 重新选落点 / 退出；P 键呼出。

---

## 7. 跑自动化测试（最快的一步）

编辑器菜单 **Tools → Session Frontend → Automation** → 过滤 `SlimeWar.` → 运行，应全绿：

| 测试 | 覆盖 |
|---|---|
| `SlimeWar.Flow.RunPhase` | 阶段迁移合法性 + 重复调用幂等（新增） |
| `SlimeWar.Flow.RunResult` | `bPassed` 规则：只有"时间到且达标"为真，死亡即使超分也算失败（新增） |
| `SlimeWar.Flow.DropPointSelection` | 落点索引越界与回退（新增） |
| `SlimeWar.Flow.RunSchedule` / `ScoringRules` / `PointState` / `SpawnSupply` | Phase C 回归 |
| `SlimeWar.Spawn.LayoutArrays` / `DuplicateSlots` / `AnchorSpace` | Phase C 回归 |
| `SlimeWar.Enemy.ActivityArea` | Phase C 回归 |

命令行等价写法：

```text
UnrealEditor-Cmd.exe "<...>\SlimeWar.uproject" -ExecCmds="Automation RunTests SlimeWar" -unattended -nopause -nullrhi -testexit="Automation Test Queue Empty"
```

---

## 8. CP-4 实机验收

用 `L_Whitebox_CityPlaza` 开 PIE，控制台先开：

```text
Slime.Debug.CombatLog 1
Slime.Debug.DrawRun 1
```

| # | 场景 | 怎么造 | 期望 |
|---|---|---|---|
| 1 | 准备页 | PIE 启动 | 不自动开局；世界无敌人；鼠标可见、可点落点；点击 → 预览 → 确认 |
| 2 | 投放演出 | 确认落点 | 播 `LS_Deploy_D*` 约 2s（世界无敌人、不可操作）；结束后 0.5s 混回玩家视角并可操作 |
| 3 | 三点同时启动 | 进入局内 | 三个点位**同时**收到同一批次；`SlimeRunStatus` 列出 3 个点位 |
| 4 | 300 分不提前结束 | `SlimeRunAddScore 300` | 分数继续累加，阶段仍是 `Running` |
| 5 | 死亡即失败 | 打够 350 分后 `SlimeKillPlayer` | 结算显示**未过关**；最佳分**未被刷新** |
| 6 | 时间到判定 | `SlimeRunSetTime 1` 且分数 ≥300 | 过关；最佳分被刷新 |
| 7 | 结算俯瞰 | 任意方式结束 | 镜头 2~3s 内无输入、敌人与玩家定住；**镜头结束后**才出现结算页 |
| 8 | 重试 ×3 | 结算页点"重试"三次 | 每次：满血满弹、0 分、180s、三点位 `AwaitingDeploy`、无残留敌人；**落点沿用上一次** |
| 9 | 重新选落点 ×3 | 结算页点"重新选落点"三次 | 每次回到准备页；换另一个落点后开局正确 |
| 10 | 暂停菜单 | 局内按 **P** | 世界冻结、菜单可点；继续 / 重试 / 重新选落点 / 退出 都通 |
| 11 | 教学提示 | 依次触发 5 条 | 各只出现一次；不暂停战斗、不抢焦点 |
| 12 | 序列时长 | 进 PIE 看 Output Log | 序列长度与 `DeployDuration` / `ResultOrbitDuration` 不一致时有 Warning |

> ⚠️ 第 10 条**必须按 P**：设计案 3.1 写的是"Esc"，但实际 `IMC_SlimeWar` 里 `IA_Pause` 绑的是 P。
> 二者不一致已登记在 [PhaseD-Plan.md §9](PhaseD-Plan.md)，要改的话二选一（改设计案文字，或改 IMC 按键）。

---

## 9. 常用命令速查

| 命令 | 作用 |
|---|---|
| `SlimeRunDeploy <Index>` | 跳过准备页，直接以指定落点开始投放（0=D1，1=D2） |
| `SlimeRunResult` | 打印结算数据（得分 / 目标 / 最佳 / 击杀 / 清空点位 / 是否过关 / 结束原因） |
| `SlimeRetry` | 走真实的重载关卡重试路径 |
| `SlimeRunStart` | 直接跳进局内（跳过投放），供 CP-3 回归用 |
| `SlimeRunEnd` | 以"时间到"结束本局 |
| `SlimeRunSetTime <Seconds>` | 跳到指定剩余时间（测最后 15 秒提示 / 倒计时归零） |
| `SlimeRunAddScore <Points>` | 加分数（测跨 300 分且不提前结束） |
| `SlimeRunTimeScale 10` | 时间轴 10 倍速 |
| `SlimeRunStatus` | 打印 CP-3 需要的全部数字（含每体量明细） |
| `SlimeKillPlayer` | 走真实伤害链杀死玩家 |
| `SlimeClearEnemies` | 清空场上敌人 |
| `Slime.Debug.DrawRun 0` | 关闭运行调试 HUD |

---

## 10. 提交前别忘了

| 要改的资产 | 说明 |
|---|---|
| `DA_RunConfig.uasset` | 第 2 步的 5 个字段（**含取消 Auto Start Run**） |
| `DA_SpawnLayout.uasset` | 3 条点位定义 + 2 个落点（导出工具写入） |
| `L_Whitebox_CityPlaza.umap` | 第 4 步的新地图 |
| `LS_Deploy_D1` / `LS_Deploy_D2` / `LS_Result` | 第 5 步 |
| `T_Overhead_CityPlaza` + 4 个 `WBP_*` | 第 6 步 |

提交前 `git status` 自查：

- [ ] 没有 `Binaries/` `Intermediate/` `Saved/` `DerivedDataCache/` `.vs/` `*.sln`
- [ ] `Docs/PhaseD/*`、`Config/DefaultGame.ini`、`Source/SlimeWar/**` 都在
- [ ] 改了 `.uasset` / `.umap` 前已在群里认领（GitHub 不支持文件锁，见 `Docs/Collaboration.md`）
