# Phase D 操作指南 —— 从零到能跑完整一局

> 这是**线性照做**的版本。每一步都有"怎么确认做对了"。
> 需要展开细节时看这三份：[资产制作指南](PhaseD-AssetGuide.md)（贴图/WBP/序列怎么做）、
> [验收清单](PhaseD-Checklist.md)（CP-4 全量）、[实现计划](PhaseD-Plan.md)（为什么这么设计）。
>
> 顺序：**0 编译 → 1 数据 → 2 地图 → 3 贴图 → 4 WBP → 5 序列 → 6 验收 → 7 提交**。
> 预计：数据+编译半小时，地图与资产是大头（半天到一天）。

---

## 0. 先让最新代码进 DLL（10 分钟，必做）

**当前状态**：Phase C + D 的源码都在工作树里，但 `Binaries/Win64/UnrealEditor-SlimeWar.dll`
是 `16:44` 的，比最后一处源码改动（`17:13`）旧——也就是说**最后两个 UI 修复还没进二进制**。
同时有一个 `UnrealEditor.exe` 开着，占住了这个 DLL，所以链接没完成。

1. **关掉编辑器**（如果开着）。
2. 双击 `SlimeWar.uproject`；提示"模块需要重新编译"时选 **Yes**。
   （或从 Visual Studio 编译 `SlimeWarEditor` 目标；或在编辑器里用 Live Coding `Ctrl+Alt+F11`，
   但改过头文件后重启编辑器更稳。）
3. **确认做对了**：编辑器打开后，PIE 控制台（`~`）输入 `Slime` 应能自动补全出
   `SlimeRunDeploy`、`SlimeRunResult`、`SlimeRetry`、`SlimePause`。
   ⚠️ 补全不出来 = 编辑器仍在用旧 DLL，回去重新编译。

---

## 1. 填数据（`DA_RunConfig`，5 分钟）

打开 `/Game/_SlimeWar/Core/Data/DA_RunConfig`，按分类填：

| 分类 | 字段 | 值 |
|---|---|---|
| Run | **Auto Start Run** | **取消勾选** ← 忘了这步准备页会被跳过 |
| Run | Run Duration | `180` |
| Run | Target Score | `300` |
| Run | Deploy Duration | `2.0` |
| Run | Result Orbit Duration | `2.5` |
| Tutorial | Tutorial Fusion Proximity | `1500`（cm；0 = 不限距离） |
| Spawn | Batch Count / Interval / Normal / Aggro | `6` / `20` / `8` / `2`（Phase C 已填，确认） |
| Spawn | Player Min Distance / Retry Window / Retry Interval / Warning Lead | `300` / `2` / `0.25` / `1` |

**确认做对了**：PIE 进去应该停在准备页而不是直接开局；`SlimeRunStatus` 不报
"RunDuration / SpawnBatchCount ... are not filled"。

---

## 2. 建三点位白盒地图并摆点位（PD-01，1~2 小时）

1. 新建关卡，另存为 `Content/_SlimeWar/Maps/L_Whitebox_CityPlaza`。
2. 按设计 7.1 摆白盒：单层 **60×50 m**、主路 4~6 m、环路互通、无跳跃点、四周封边（不设可坠落边缘）。
3. 放三根 **`ASpawnPoint`** 锚点，`Point Id` = `1`/`2`/`3`，位置对应 **A(15,15) / B(45,15) / C(30,38)**
   （米）。锚点是**点位中心**，不是第一个出生位。
4. 每个点位周围留 **直径 ≥10 m** 的空地；放一个覆盖全场的 `NavMeshBoundsVolume`；放一个 `PlayerStart`。
5. 逐根选中锚点 → Details 的 **Spawn Point Editor** → `Create Default Slots` → 视口里拖拽微调
   → `Validate Layout` 直到没有 `BAD` → `Export All Points In Level` → **保存关卡与 `DA_SpawnLayout`**。
6. 打开 `DA_SpawnLayout` 确认：`Points` 有 3 条；`Drop Points` = **D1(800, 800, 0)、D2(4500, 3800, 0)**
   （⚠️ **单位是 cm**：设计图上的 8,8 / 45,38 是米，×100。别填成 8,8）。

**确认做对了**：PIE 里 `SlimeRunStatus` 列出 **3 个点位**；进局内后三点同时开始出怪。
（生成点编辑器的详细操作见 [PhaseC-Checklist.md §3](../PhaseC/PhaseC-Checklist.md)。）

---

## 3. 俯瞰贴图 `T_Overhead_CityPlaza`（30 分钟）

推荐**直接画示意图**（标记位置自己说了算，不用做坐标换算）：

1. 画一个 60×50 的矩形，按设计 7.1 标出 A/B/C 三个生成点、D1/D2 两个落点、主路/环路与几块不可通行体块。
2. 存成 `T_Overhead_CityPlaza.png`（**ASCII 命名**），导入到 `Content/_SlimeWar/UI/`。
3. 双击贴图 → `Compression Settings = UserInterface2D (RGBA)`、`Mip Gen Settings = NoMipmaps`、`Texture Group = UI`。

想用真实顶视图截图的做法（含坐标→像素换算）见 [资产指南 §1](PhaseD-AssetGuide.md)。

---

## 4. 四个 WBP（2~4 小时，建议按这个顺序，每个做完都 PIE 一次）

**每个 WBP 都是同样三步**：

1. `Content/_SlimeWar/UI/` → 右键 → **User Interface → Widget Blueprint** → 严格按表命名。
2. 打开 → 菜单 **File → Reparent Blueprint** → 选对应的 C++ 父类。
   ⚠️ 不 Reparent 就找不到事件。
3. Graph 左侧 **My Blueprint → Functions / Events** → 找到事件 → Implement → 拉线做逻辑。

| 顺序 | WBP | 父类 | 事件/动作 |
|---|---|---|---|
| 1 | `WBP_SlimeHUD` | `USlimeHUDWidget` | `OnScoreUpdated` / `OnTimeUpdated` / `OnPointStateUpdated` / `OnHealthUpdated` / `OnAmmoUpdated` / `OnBatchIncoming` / `OnScoreEarned` / `ShowTutorial` |
| 2 | `WBP_SlimePreparation` | `USlimePreparationWidget` | `OnPreparationReady` / `OnDropPointPreviewed`；按钮调 `PreviewDropPoint(i)` / `ConfirmPreviewedDropPoint()` |
| 3 | `WBP_SlimeResult` | `USlimeResultWidget` | `OnResultReady`；按钮调 `RequestRetry()` / `RequestReselectDropPoint()` / `RequestQuit()` |
| 4 | `WBP_SlimePause` | `USlimePauseWidget` | 四个按钮调 `Resume()` / `RequestRetry()` / `RequestReselectDropPoint()` / `RequestQuit()` |

控件的布局样式与事件细节见 [资产指南 §2](PhaseD-AssetGuide.md)。

**确认做对了**：每次 PIE 后看 Output Log——

| 日志 | 含义 |
|---|---|
| `USlimeUISubsystem: HUD widget class is not set` | 这个名字的 WBP 还没建，或路径不对 |
| 没有这条日志 + 界面上有内容 | 接上了 ✓ |

**只做完 HUD 也能验**：C++ 里"缺资产就跳过、只打警告"，所以缺哪个界面都不影响其它界面和玩法。

---

## 5. 三条 Level Sequence（1~2 小时）

**必须在 `L_Whitebox_CityPlaza` 里做**（Sequencer 记的是世界坐标）。

| 资产 | 长度 | 内容 |
|---|---|---|
| `LS_Deploy_D1` | 2.0 s | 从 D1 上方俯冲到落地机位 |
| `LS_Deploy_D2` | 2.0 s | 同上，起点按 D2 |
| `LS_Result` | 2.5 s | 能看清三个点位的俯瞰环绕 |

核心步骤（详细见 [资产指南 §3](PhaseD-AssetGuide.md)）：

1. **Cinematics → Add Level Sequence** → 存到 `Content/_SlimeWar/UI/Cinematics/`。
2. **+ Track → Camera Cut Track** → 在该轨道加 **Spawnable Cine Camera**（不要往关卡里丢相机 Actor）。
3. 选中相机轨道 → **Pilot** 进入相机视角 → 移动相机 → 在 0 帧和结束帧各按 **S** 打关键帧。
4. **把播放范围改成 2.0 s（或 2.5 s）**，默认的 5 s 一定要改。
5. 序列里**只动相机**：敌人与玩家的"定住"由代码冻结，不要 K 他们。

**确认做对了**：PIE 时 Output Log 没有
`'LS_...' is 5.00s but DeployDuration is 2.00s` 这类 Warning；投放能看到俯冲，结算能看到环绕。

---

## 6. 跑一遍完整验收（CP-4，30 分钟）

1. 自动化测试（最快的一步）：编辑器 **Tools → Session Frontend → Automation** → 过滤 `SlimeWar.` → 全绿
   （应 11 项：Phase C 的 8 项 + 新增的 RunPhase / RunResult / DropPointSelection）。
2. 用 `L_Whitebox_CityPlaza` 开 PIE，控制台先敲：

```text
Slime.Debug.CombatLog 1
Slime.Debug.DrawRun 1
```

3. 依次确认（完整 12 条见 [Checklist §8](PhaseD-Checklist.md)）：

| 检查 | 怎么造 | 期望 |
|---|---|---|
| 准备页 | 启动 | 不自动开局；可点落点 → 预览 → 确认 |
| 投放 | 确认落点 | 约 2 s 俯冲；结束后 0.5 s 混回玩家视角并可操作 |
| 三点同时 | 进局内 | 三个点位同时出怪 |
| 300 分不提前结束 | `SlimeRunAddScore 300` | 仍在局内，分数继续涨 |
| 死亡即失败 | 打够分后 `SlimeKillPlayer` | 结算"未过关"，最佳分**未**刷新 |
| 时间到过关 | `SlimeRunSetTime 1`（分数 ≥300） | 过关，最佳分刷新 |
| 俯瞰先于结算页 | 任意方式结束 | 镜头放完**之后**才出结算页 |
| 重试 ×3 | 结算页点"重试" | 每次满血满弹、0 分、180 s、三点位 `AwaitingDeploy`；落点沿用上次 |
| 重新选落点 ×3 | 结算页点"重新选落点" | 每次回到准备页 |
| 暂停 | 局内按 **P** | 世界冻结、菜单可点，四个按钮都通 |
| 教学 5 条 | 挨个触发 | 各只出现一次，不暂停战斗 |

---

## 7. 提交

1. 改 `.uasset` / `.umap` 前先在群里**认领**（GitHub 不支持文件锁，见 `Docs/Collaboration.md`）。
2. 提交前 `git status` 自查：
   - [ ] 没有 `Binaries/` `Intermediate/` `Saved/` `DerivedDataCache/` `.vs/` `*.sln`
   - [ ] 改了哪些 DA / 地图 / Sequence / WBP，心里有数并能说出来
3. 别忘了把本轮差异回填 `Docs/ProgramTaskList.md`：新增 **PD-13（暂停菜单）**、
   两处契约变更（`OnEnemyFused` 带位置、`ESlimeRunState` 追加两值）、
   `PA-12` 依赖改为 `PD-09`（清单见 [Plan §9](PhaseD-Plan.md)）。

---

## 附：准备页要写的那段"操作说明"（可直接抄进 WBP）

| 按键 | 作用 |
|---|---|
| `W A S D` | 移动 |
| 鼠标 | 转动视角 |
| **鼠标左键** | 射击（**可按住**连发） |
| **鼠标右键** | 辅助瞄准（不加伤害、不减速） |
| `R` | 换弹（**换弹期间可以移动和瞄准**，只是不能开枪） |
| `P` | 暂停 / 继续 |

> ⚠️ 设计案 3.1 写的是 **Esc** 暂停，但 `IMC_SlimeWar` 里 `IA_Pause` 实际绑的是 **P**。
> 现在实现沿用 **P**，所以界面文字要写 P；要改成 Esc 的话，改 IMC 之后记得同步改这里的设计案与文字
> （差异已登记在 [Plan §9](PhaseD-Plan.md)）。
