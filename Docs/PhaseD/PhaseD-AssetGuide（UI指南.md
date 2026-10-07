# Phase D 资产制作指南 —— 俯瞰贴图 / WBP / Level Sequence

> 配套 [PhaseD-Checklist.md](PhaseD-Checklist.md) 的第 4~6 步。Checklist 讲"要什么"，本文讲"怎么点出来"。
> 三组资产互相独立，顺序可以换，但**建议**：先准备页贴图 → 再做 WBP → 最后录 LS
> （LS 要在已经摆好点位的关卡里录，最后做可以少返工）。

---

## 0. 前置（没做完这三条，后面全会踩空）

1. **关掉编辑器再让模块重新编译一次**。当前有一个 `UnrealEditor.exe` 占着
   `Binaries/Win64/UnrealEditor-SlimeWar.dll`，所以上一次链接没成功。关掉它之后重新双击
   `SlimeWar.uproject`（或从 VS 编译一次），模块才是最新的。之后可以再打开编辑器做资产。
2. **先在 `L_Whitebox_CityPlaza` 里摆好三根 `ASpawnPoint` 锚点并 `Export To Data Asset`**
   （Checklist §4）。LS 的镜头是照着你摆的点位录的，先录镜头再挪点位等于白录。
3. **取消 `DA_RunConfig` 的 `Auto Start Run`**，否则准备页会被跳过，看起来像"UI 没生效"。

---

## 1. 俯瞰贴图 `T_Overhead_CityPlaza`

准备页是"静态俯瞰图 + 手摆标记"，所以只差一张 2D 图。两条路，任选：

### 方案 A（推荐，半小时内做完）：画一张示意图

设计案 2.2 要的是"标出三个生成点、两个落点、不可通行体块和主要连接路"，示意图完全够用，
而且**标记位置你自己说了算**，不用做世界坐标→像素的换算。

1. 在任意画图工具里画一个 **60×50 的矩形**（就是白盒场地），SW 为原点、东为 X、北为 Y。
2. 按设计 7.1 标出：A(15,15)、B(45,15)、C(30,38) 三个生成点；D1(8,8)、D2(45,38) 两个落点；
   再补上主路/环路和几块不可通行体块。
3. 导出 PNG，命名 **`T_Overhead_CityPlaza.png`**（ASCII，别用中文/空格，见 `Docs/Collaboration.md` §8）。
4. Content Browser → 进 `Content/_SlimeWar/UI/` → **Import** → 选 PNG。
5. 双击贴图 → `Compression Settings = UserInterface2D (RGBA)`、`Mip Gen Settings = NoMipmaps`、`Texture Group = UI`。

### 方案 B（要真实感）：从关卡截正交顶视图

1. 打开 `L_Whitebox_CityPlaza`，把视口切到 **Top** 视角（视口左上角的视角下拉里选 Top）。
2. 让视野正好覆盖 `X∈[0,6000]`、`Y∈[0,5000]`（单位 cm）。做法：在 `(3000, 2500, 0)` 放一个
   空 Actor → 选中它按 **F** 聚焦 → 滚轮把正交宽度调到场地铺满且不要多余留白。
3. 按 **G** 切 Game View，把图标、网格线、gizmo 都藏掉，画面才干净。
4. 截图：视口左上角 **Camera 下拉 → High Resolution Screenshot**，或用控制台 `HighResShot 2048x2048`。
   文件在 `Saved/Screenshots/Windows/`。
5. 按方案 A 的第 4、5 步导入与设置。

> **方案 B 的标记换算**：UGM 的 Y 轴向下，世界 Y 向上（顶视图正放时），所以
> `u = X / 6000`、`v = 1 - Y / 5000`。按这个算出来是
> A(u0.25, v0.70)、B(0.75, 0.70)、C(0.50, 0.24)、D1(0.13, 0.84)、D2(0.75, 0.24)。
> ⚠️ 但如果你的顶视图是"+Y 朝下"，v 要改成 `Y / 5000`。**对一次就知道**：用 C 点当基准，
> 如果 C 的标记落在图的下半部分，说明取景反了。
> 无论哪种，摆完标记后请和关卡顶视图并排比一次，肉眼对齐一下。

---

## 2. 四个 WBP

### 2.0 通用三步（每个 WBP 都一样）

1. **建**：Content Browser → `Content/_SlimeWar/UI/` → 右键 → **User Interface → Widget Blueprint**
   → 命名（严格用下面表格里的名字，`DefaultGame.ini` 已经按这些路径预填好了）。
2. **改父类**：打开 WBP → 菜单 **File → Reparent Blueprint** → 选对应的 C++ 类。
   ⚠️ **必须先 Reparent**，否则 Design 里没有我们的控件、Graph 里也找不到可实现的 Event 节点。
3. **实现事件**：Graph 面板左侧 **My Blueprint → Functions / Events** 列表里找到我们要的事件
   → 双击（或右键 → Implement event）→ 图表中出现 `Event On Xxx` 节点，从它拉线做逻辑。

| WBP | 父类 |
|---|---|
| `WBP_SlimeHUD` | `USlimeHUDWidget` |
| `WBP_SlimePreparation` | `USlimePreparationWidget` |
| `WBP_SlimeResult` | `USlimeResultWidget` |
| `WBP_SlimePause` | `USlimePauseWidget` |

> C++ 只负责"什么时候调用、数据是多少"；排版、颜色、动画全在 WBP 里做。
> 事件是父类声明的空事件，**你只需要实现，不需要调用**——除了标了 `BlueprintCallable` 的动作
> （例如准备页的 `ConfirmDropPoint`），那些要你主动调。

### 2.1 `WBP_SlimeHUD`（PD-09 + PD-12 + PA-12）

**Design（一个铺满的 Canvas Panel）**

- 左上：分数 Text + "目标 300" Text。
- 右上：倒计时 Text。
- 底部中：生命 ProgressBar + 弹匣 Text。
- 3 个点位标记（Image 或 Border，横向排一行）——只做三态：生成中 / 耗尽未清 / 已清空。
- 一个 Toast 区（背景 Border + 一个 Text），初始 `Collapsed`。

**Graph 要实现的事件**

| 事件（签名） | 做什么 |
|---|---|
| `OnScoreUpdated(Score, TargetScore)` | 更新两个 Text |
| `OnTimeUpdated(RemainingSeconds)` | 更新倒计时；小于 15 可改红 |
| `OnPointStateUpdated(PointId, NewState)` | 按 PointId 找标记，按 NewState 换颜色 |
| `OnHealthUpdated(Health, MaxHealth)` | 血条百分比 |
| `OnAmmoUpdated(CurrentAmmo, MagazineSize)` | 弹匣 Text |
| `OnBatchIncoming(BatchIndex, SecondsUntilSpawn)` | 短暂提示"下一批" |
| `OnScoreEarned(NewScore, DeltaScore)` | **PA-12**：飞一个粘液图标到分数处（UMG 动画或 Tick 里插值） |
| `ShowTutorial(Message)` | Toast 显示 Message，2~3 秒后 Collapsed |

> **不要**在 Tick 里自己去查数据：所有数值都是被推过来的。
> `OnScoreEarned` 只在分数**真的上涨**时触发，第一次显示 HUD 不会误触发。

### 2.2 `WBP_SlimePreparation`（PD-05 + PD-06）

**Design**：一个 Image 放 `T_Overhead_CityPlaza`（铺满画布），上面摆 5 个标记：
3 个生成点（只读）与 2 个落点（可点）。落点标记的**摆放顺序必须对应 `DA_SpawnLayout::DropPoints`**
（下标 0 = D1，1 = D2）。

**交互（设计案 2.3 的"点击 → 预览 → 再点确认"）**

1. 落点标记的 **On Clicked** → 调 `PreviewDropPoint(0)` / `PreviewDropPoint(1)`。
2. 实现 `OnDropPointPreviewed(Index)` → 把"预览标记"那张 Image 移到对应位置并显示。
3. 确认按钮 **On Clicked** → 调 `ConfirmPreviewedDropPoint()`。
   （若要"再点一次同一个落点即确认"，就在落点标记的 On Clicked 里先判断
   `GetPreviewedDropPoint()` 是否等于自己，是则调 `ConfirmDropPoint(Index)`。）
4. 实现 `OnPreparationReady(DropPointCount)` → 如果 `DropPointCount < 2`，把多出来的落点标记隐藏
   （DA 没填好时的兜底，避免点到一个不存在的落点）。
5. 信息区写上：180 秒 / 300 分 / 操作说明（WASD、左键射击、右键瞄准、R 换弹、**P 暂停**）。

### 2.3 `WBP_SlimeResult`（PD-10）

- 实现 `OnResultReady(Result)` → 用 **Break Slime Run Result** 拆出字段填 Text：
  `Score` / `TargetScore` / `BestScore` / `NormalKills` / `ClearedPoints` / `bPassed` / `EndReason`。
  - `bPassed = true` 显示"过关"，否则"未达标"；`EndReason == PlayerDied` 显示"任务失败"。
- 三个按钮 On Clicked → `RequestRetry()` / `RequestReselectDropPoint()` / `RequestQuit()`。
- 也可以直接用 `WasPassed()` 这个纯函数节点。

> 这个事件在**结算页真正显示的那一刻**才会带着最终数据触发（镜头放完之后），
> 所以不要担心它拿到空数据。

### 2.4 `WBP_SlimePause`（PD-13）

- 一个 Panel + 四个按钮 → `Resume()` / `RequestRetry()` / `RequestReselectDropPoint()` / `RequestQuit()`。
- **不用处理按键**：P 已经在角色上绑定，按键会转到 UI 子系统再打开这个界面。

---

## 3. 三条 Level Sequence

### 3.1 四条铁律

1. **必须在 `L_Whitebox_CityPlaza` 里做**。Sequencer 记录的是世界坐标，在别的关卡录的镜头换到这张图就飞了。
2. **时长必须等于 DA 值**：投放 `DeployDuration = 2.0s`，结算 `ResultOrbitDuration = 2.5s`。
   不一致 PIE 时会打 Warning（玩法仍按 DA 走，只是画面对不上）。
3. **相机用 Spawnable，不要往关卡里丢相机 Actor**：运行时由序列自己生成，关卡保持干净、少一个二进制改动点。
4. **只动相机**。敌人与玩家的"定住"由代码负责（结算阶段会自动冻结），序列里不要去 K 敌人或玩家。

### 3.2 `LS_Deploy_D1` / `LS_Deploy_D2`（各 2.0 s）

1. 打开 `L_Whitebox_CityPlaza` → 工具栏 **Cinematics → Add Level Sequence** →
   命名 `LS_Deploy_D1`，保存到 `Content/_SlimeWar/UI/Cinematics/`。
2. Sequencer 里 **+ Track → Camera Cut Track**；然后在这条轨道上加相机：
   点轨道上的 **+ Camera**（或右键轨道 → Add Camera）→ 选新建 **Cine Camera Actor**（会作为 Spawnable 加进来）。
3. 选中相机轨道 → 点它的 **Pilot**（小飞机图标）进入相机视角 → 用 WASD/鼠标把相机移到想要的机位
   → 把时间轴移到 0 帧，按 **S** 给相机 Transform 打一个关键帧。
4. 再把时间轴拖到 **2.0 s**，把相机移到"落地后"的机位（例如从 D1 正上方 Z≈2500 俯视，
   降到 Z≈600 并朝三个点位方向），按 **S** 打第二个关键帧。
5. 确认序列的**播放范围是 0 → 2.0 s**（不是默认的 5 s）。
6. `LS_Deploy_D2` 同理，只是起点按 D2 的位置录。

> 建议镜头语言：起点高、俯角大（能看清落点），终点低、朝向场地中心。2 秒够一次干净的俯冲。

### 3.3 `LS_Result`（2.5 s）

1. 同样在当前关卡新建、命名 `LS_Result`，放在同一个 `Cinematics/` 目录。
2. 一条 Camera Cut + Spawnable 相机，2.5 s 内从能同时看清三个点位的高处（例如
   `(3000, 2500, 3000)` 附近）缓慢环绕或推轨 30~45°。
3. 结束那一刻的画面就是结算页出现时的背景。

---

## 4. 把资产接起来（多半是自动的）

`Config/DefaultGame.ini` 里**已经预填**了这些路径，你只要用上面规定的名字与目录建资产，就自动接上：

```ini
HUDWidgetClass=/Game/_SlimeWar/UI/WBP_SlimeHUD.WBP_SlimeHUD_C
PreparationWidgetClass=/Game/_SlimeWar/UI/WBP_SlimePreparation.WBP_SlimePreparation_C
ResultWidgetClass=/Game/_SlimeWar/UI/WBP_SlimeResult.WBP_SlimeResult_C
PauseWidgetClass=/Game/_SlimeWar/UI/WBP_SlimePause.WBP_SlimePause_C
+DeploySequences=/Game/_SlimeWar/UI/Cinematics/LS_Deploy_D1.LS_Deploy_D1
+DeploySequences=/Game/_SlimeWar/UI/Cinematics/LS_Deploy_D2.LS_Deploy_D2
ResultSequence=/Game/_SlimeWar/UI/Cinematics/LS_Result.LS_Result
```

想改名或换目录也可以：**Project Settings → Game → Slime War** 里改这四个 Widget Class，
DeploySequences（下标 0 = D1，1 = D2）与 ResultSequence。

---

## 5. 分阶段自检（每做完一组就能验，不用等全部做完）

| 做完到哪 | PIE 应该看到 |
|---|---|
| 只有贴图 | 没有 WBP 就没有准备页可点，UI 子系统会自动部署到落点 0 让你先进去（日志有说明）——正常 |
| 贴图 + 准备页 | 准备页有图与标记；点落点 → 预览 → 确认 → **没有序列时会立刻进局内**（投放被跳过） |
| + HUD | 进局内后 HUD 有分数/倒计时/生命/弹匣；打一只史莱姆看分数跳、图标飞入 |
| + 结算页 | `SlimeRunSetTime 1` 或 `SlimeKillPlayer` → 冻结 → （没序列则立刻）结算页出现 |
| + 三条 LS | 投放有俯冲、结算有环绕；镜头结束才出结算页 |

看日志定位问题：

| 日志 | 含义 |
|---|---|
| `USlimeUISubsystem: X widget class is not set` | WBP 路径/名字没对上（或还没建） |
| `X widget class could not be loaded (not set in ... / 具体路径)` | 括号里是"没配"还是"配了但资产不存在"，据此定位 |
| `no preparation screen, so a drop point cannot be chosen. Deploying to drop point 0` | 准备页 WBP 还没建——不是错误，是降级 |
| `USlimePresentationDirector: no deploy sequence for drop point N` | 序列路径没对上，或 `DeploySequences` 没填够两条 |
| `... is 5.00s but DeployDuration is 2.00s` | 序列播放范围没改到 2 s |

---

## 6. 常见坑

1. **WBP 没 Reparent** → 事件列表里找不到 `OnScoreUpdated` 之类，怎么翻都翻不到。
2. **LS 在别的关卡录** → 镜头飞到场地外。
3. **序列播放范围忘了改** → 默认 5 s，与 DA 不一致，会打 Warning。
4. **资产名带中文或空格** → 构建/资产路径出问题（`Docs/Collaboration.md` §8）。
5. **忘了取消 Auto Start Run** → 准备页被跳过。
6. **准备页落点标记顺序和 `DropPoints` 不一致** → 点 D1 实际部署到 D2。
7. **改 `.uasset` / `.umap` 前没在群里认领** → GitHub 不支持文件锁，两个人同时改就是一人白干。
