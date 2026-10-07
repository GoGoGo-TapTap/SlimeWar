# Phase C · 编辑器步骤与 CP-3 验收

> 代码侧已编译通过（`SlimeWarEditor` 与 `SlimeWar` 两个目标，0 error）。
> 下面第 2~4 步只能在你自己的机器上做（`.uasset` / `.umap` 无法在编辑器外生成）。
> 顺序有依赖：**先填 DA_RunConfig，再用生成点编辑器摆位并导出，最后才进 PIE。**

---

## 0. 本次修订打破了旧数据（先看这条）

v1.1 把点位中心改成了**锚点的 Transform**，槽位改成**锚点局部坐标**：

- `FSlimeSpawnPointDef::Center` 字段已删除；
- `FSlimeSpawnSlot::Location` 改为 `RelativeLocation`（相对锚点，含旋转）。

所以 `DA_SpawnLayout` 里旧的绝对坐标**不再有正确含义**，需要用第 3 步的工具重摆一次。
锚点的世界位置可以沿用你现在摆的那一处。

---

## 1. 打开工程

1. 关掉正在运行的编辑器。
2. 双击 `SlimeWar.uproject`；若提示需要重新编译模块，选 Yes。
   本次新增了 `SlimeWarEditor` 模块，Output Log 里不应出现 `SlimeWarEditor` 加载失败。
3. 确认 Output Log 里没有 `DA_RunConfig ... are not filled` 之类的报错。

---

## 2. 给 `DA_RunConfig` 填 Phase C 字段

打开 `/Game/_SlimeWar/Core/Data/DA_RunConfig`，展开 **Spawn** 分类：

| 字段 | 值 | 说明 |
|---|---|---|
| Spawn Batch Count | `6` | 每点 6 批 |
| Spawn Batch Interval | `20` | 每 20 秒一批，首批 t=0，末批 t=100s |
| Spawn Normal Per Batch | `8` | 每批 8 只普通 |
| Spawn Aggro Per Batch | `2` | 每批 2 只追兵 |
| Spawn Player Min Distance | `300` | cm，距玩家至少 3m |
| Spawn Retry Window | `2` | 秒，出生位不可用的最长延迟 |
| Spawn Retry Interval | `0.25` | 秒，延迟重试节流 |
| Spawn Batch Warning Lead | `1` | 秒，提前 1s 提示 |

再确认 **Run** 分类：`Run Duration = 180`、`Target Score = 300`、`Auto Start Run` 勾选。

> **填 0 会怎样**：`SpawnBatchCount` / `SpawnNormalPerBatch` / `RunDuration` 任一为 0 时开局被拒绝并报错。

---

## 3. 用生成点编辑器摆位（PC-10）

### 3.1 摆放锚点

1. 打开 `/Game/_SlimeWar/Maps/L_Sandbox_OnePoint`。
2. Place Actors → 搜 `SpawnPoint` → 放到你想要的**点位中心**。
3. Details → `Point Id` 填 `1`（必须与 `DA_SpawnLayout` 里那条定义一致）。

   > **锚点就是点位**：它的位置/旋转决定活动圈圆心和整组槽位的参考系，所以先把锚点摆对。
   > 拖动锚点（青色箭头）时整组槽位会跟着走，不用改任何数据。

4. 确认地图里有覆盖该区域的 `NavMeshBoundsVolume` 和 `PlayerStart`。

### 3.2 生成并微调槽位

选中锚点 → Details 面板出现 **Spawn Point Editor** 分类：

1. 点 **Create Default Slots** → 生成 8 个普通（绿）+ 2 个追兵（红）+ 3 个备用（黄）手柄。
2. 在视口里直接拖拽/旋转每个手柄：
   - 手柄是普通场景组件，用 UE 原生操纵器即可（框选选不中箭头，请**单击**或在 World Outliner 里选）；
   - **Z 不用对**：运行时只按 XY 找地面，拖到空中也会落在正下方的地上；
   - 想加/删就用手柄自己的 Details 面板：`Role`（改角色会改颜色）、`Slot Index`、**Remove Slot**。
3. 不满意就点 `+ Normal` / `+ Aggro` / `+ Fallback` 追加，或重新 `Create Default Slots`。

### 3.3 校验与导出

1. **Snap To Ground**：把所有手柄的 Z 贴到地面（可选，纯视觉；运行时本来就会吸附）。
2. **Validate Layout**：在 Output Log 里逐槽位打印 `OK / BAD` 与原因（`off the navmesh` /
   `no ground under the slot` / `blocked by static geometry` / `too close to the player`）。
   **先修到没有 BAD 再继续**。
3. **Export To Data Asset**：把本点位的槽位写进 `DA_SpawnLayout`（面板顶部状态应从
   `OUT OF SYNC` 变成 `in sync`）。多个点位时可点 `Export All Points In Level`。
4. 保存关卡与 `DA_SpawnLayout`（`Ctrl+S` → Save All）。

> `Import From Data Asset` 是反方向：用资产里的数据重建手柄（换机器 / 误删手柄时用）。

---

## 4. 跑自动化测试（最快的一步）

编辑器菜单 **Tools → Session Frontend → Automation** → 过滤 `SlimeWar.` → 运行，应全绿：

| 测试 | 覆盖 |
|---|---|
| `SlimeWar.Flow.RunSchedule` | 批次时间表 0/20/40/60/80/100 与提前 1s 提示 |
| `SlimeWar.Flow.ScoringRules` | 按体量计分、攻击性不计分、最佳分只在达标时刷新 |
| `SlimeWar.Flow.PointState` | 四状态迁移，只有普通清零才算 Cleared |
| `SlimeWar.Flow.SpawnSupply` | 单点 48/12、全场 180、2s 窗口 9 次尝试 |
| `SlimeWar.Spawn.LayoutArrays` | 手柄 `(Role, Index)` → 三个槽位数组，排序与往返 |
| `SlimeWar.Spawn.DuplicateSlots` | 重复 XY 检测 |
| `SlimeWar.Spawn.AnchorSpace` | 锚点平移/旋转后槽位世界坐标与生成 Z |
| `SlimeWar.Enemy.ActivityArea` | 活动圈判定与夹回（高度不参与；未设置半径/圆心时不约束） |

> 也可以不开编辑器直接跑：
> `UnrealEditor-Cmd.exe "<...>\SlimeWar.uproject" -ExecCmds="Automation RunTests SlimeWar" -unattended -nopause -nullrhi -testexit="Automation Test Queue Empty"`
> —— 上次结果：**8 tests performed，0 失败**。

（命令行等价写法：`UnrealEditor-Cmd.exe "<...>\SlimeWar.uproject" -ExecCmds="Automation RunTests SlimeWar" -unattended -nopause -testexit="Automation Test Queue Empty"`）

---

## 5. CP-3 实机验收（沙盒单点闭环）

1. 用 `L_Sandbox_OnePoint` 开 PIE（默认 GameMode 已是 `SlimeWarGameMode`）。
   - **先看 Output Log**：若出现 `Spawn point N is OUT OF SYNC ...`，说明你改了手柄但没导出，回去做 §3.3。
2. 控制台（`~`）敲 `SlimeRunTimeScale 10`：时间轴 10 倍速，约 10 秒跑完 6 批。
3. 逐项确认：

   | 检查 | 期望 |
   |---|---|
   | t=0 | 立即出现 8 普通 + 2 追兵，全部**站在地面上**（不再半埋/卡住） |
   | 点位状态 | `Spawning` → 第 6 批后仍有存活普通则 `DepletedNotCleared` → 普通清光 `Cleared` |
   | 存活追兵 | 不阻止清空；清空后仍继续追击；从不计分 |
   | 活动圈 | 普通史莱姆始终留在圈内（`Slime.Debug.DrawEnemyState 1` 可看到圈与中心）；把它推出圈外它会自己走回来；追兵不受限 |
   | 倒计时归零 | `run Ended`，不再生成、不再新增得分 |
   | 出生位不可用 | 日志给出**逐候选位**原因与备用位，2s 后取消且不补发 |

4. 关键数字核对：随时敲

   ```
   SlimeRunStatus
   ```

   输出含：每体量击杀数与得分明细、总分、`per-mass sum`、目标分、最佳分、击杀数、清空点数、
   融合次数、存活/峰值，以及每个点位的状态 / 已生成 / 存活 / 待生成。
   **手算**：`mass 1 → 10 分/只`、…、`mass 8 → 80`（`DT_SlimeStats` 的 `KillScore`），
   明细相乘再相加应等于 `score`。

5. **最佳分与失败不刷新**：
   - 让倒计时自然归零且分数 ≥ 300 → 重开一局，`BEST` 应保留；
   - 敲 `SlimeKillPlayer` 立即死亡终局 → 再开一局，`BEST` **不应**被失败成绩覆盖。

---

## 6. 常用命令速查

| 命令 | 作用 |
|---|---|
| `SlimeRunStart` | 立刻开始（或重开）本局 |
| `SlimeRunEnd` | 以「时间到」结束本局 |
| `SlimeRunTimeScale 10` | 时间轴 10 倍速；`0` 冻结 |
| `SlimeRunStatus` | 打印 CP-3 需要的全部数字 |
| `SlimeKillPlayer` | 走真实伤害链杀死玩家（验证 `PlayerDied` 终局） |
| `SlimeClearEnemies` | 清空场上敌人（配合检查清空判定） |
| `Slime.Debug.DrawRun 0` | 关闭 Phase C 调试 HUD |
