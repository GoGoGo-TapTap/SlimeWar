# RebuildPlan 与《程序实现策划案》一致性审查报告

- 日期：2026-10-19
- 对照文档：`Docs/GameDesign/newDesign/程序实现策划案.docx`
- 对照计划：`Docs/EcologyRebuild-Plan.md`
- 原则：数值冲突只记录，不修改代码、DataAsset、关卡或 Plan 参数。

## 状态说明

- `Recorded – No Change`：数值冲突，已记录，本轮不修改。
- `R1`–`R5`：行为差异，按对应模块修复。
- `Documentation`：Plan 记录不清或内部存在历史矛盾，只修文档。

## 高优先级数值冲突

| ID | 策划案 | 当前 Plan / 实现 | 状态 |
|---|---|---|---|
| N01 | 身体推动基础速度 2.6 m/s、加速度 18 m/s² | 基础 260 cm/s；没有 18 m/s² 推动加速度；使用外力阻尼 500 cm/s² | `Recorded – No Change` |
| N02 | 吞噬每吸收 1 体量恢复 15 生命 | `DevourHealPerMass=5` | `Recorded – No Change` |
| N03 | 跃击允许高度差不超过 1.4 m | `PounceHitHeight=240 cm` | `Recorded – No Change` |
| N04 | 采集/返程速度 1.35 m/s | `CargoSpeed=115 cm/s` | `Recorded – No Change` |
| N05 | 有资源记忆探索半径 3–6 m、无记忆 6–10 m | Plan 使用 `WanderRadius 300–600 cm`，并新增 `ResourceExplorationRange=2000 cm` 长距离 fallback | `Recorded – No Change` |
| N06 | 外力融合捕获间距 0.65 m、普通连接 0.08 m、连接拉近 0.45 m/s | 当前只有 `FusionContactTolerance=18 cm`、`FusionForcedCaptureGap=8 cm`、自主靠近 75 cm/s | `Recorded – No Change` |
| N07 | 射击附加融合冷却至少 0.8 秒，并保留受击前冷却快照 | Plan/规则没有对应字段 | `Approved Change – R4` |
| N08 | 单点追离 15 m、双点追离 19 m | 统一 `CoreLeashDistance=1900 cm` | `Recorded – No Change` |
| N09 | 核心目标直接连锁 | `HostileCoresChainOnly=true`，中性核心不递归连锁 | `Recorded – No Change` |
| N10 | 相机常态距离 4 m、瞄准 2.2 m | 当前 `CameraBoomLength=220 cm` | `Recorded – No Change` |
| N11 | 常态 FOV 75°、瞄准 55° | 当前 `CameraFieldOfView=0`，运行时使用引擎默认 90° | `Recorded – No Change` |
| N12 | 水平加速度 20 m/s² | 当前配置记录为 2048 cm/s² | `Recorded – No Change` |

## 高优先级行为差异

| ID | 行为差异 | 修复模块 |
|---|---|---|
| B01 | 当前没有独立冲刺、跳跃、瞄准减速状态；瞄准不会覆盖冲刺速度 | R1 |
| B02 | 当前没有瞄准相机距离/FOV切换和 Q 换肩 | R1 |
| B03 | 大脑优先级与策划案表格顺序不同，携货、融合、玩家避让和核心战斗的仲裁顺序不一致 | R2 |
| B04 | 锁定/眩晕期间“不刷新观察、暂停普通计时”的规则未形成统一实现和验收 | R2 |
| B05 | 出生初始采集休息、目标保留、拥挤代价、动作/结算距离未完整实现 | R3 |
| B06 | 资源记忆下的探索速度切换和行为计时未实现 | R3 |
| B07 | 融合的外力连接、普通连接拉近、融合眩晕合并未实现 | R4 |
| B08 | 射击打断时缺少“受击前冷却快照”的明确行为模型 | R4 |
| B09 | 核心近身玩家优先切换、最后可见位置和攻击距离组合规则未完整验收 | R4 |
| B10 | 资源争夺累计消退、警戒后空载 4 秒休息、粘液趋近、重复探索历史未完整实现 | R5 |

## Plan 记录缺口

- `Documentation`：M5 仍保留“两个中性核心相邻时第二个也爆裂”的历史结果，与后文 M7 的“中性核心不连锁”修正并存。
- `Documentation`：M7 记录新增 `bEcologyRun`，M8 前置清退又记录删除该字段；需要标注为不同时间点。
- `Documentation`：M6 记录保留 `UGA_Fire` 兼容，M8 清退记录删除旧 `UGA_Fire`；需要统一当前状态。
- `Documentation`：采集动作距离与结算距离、拾取动作距离与结算距离未在 Plan 中分开。
- `Documentation`：资源目标保留时间、矿点拥挤代价、初始采集休息未在 Plan 中记录。
- `Documentation`：粘液感知与趋近行为未在 Plan 中记录。
- `Documentation`：锁定/眩晕期间普通行为计时的暂停范围未写清。
- `Documentation`：`Z/X` 与额外 `C` 阵营键、右键瞄准与磁性吸附常开的关系缺少明确记录。

## 已确认一致的规则

- 玩家胶囊 43×180 cm。
- 体量 1–3 可被身体推动，体量 4–8 阻挡。
- 自动融合上限 2、总融合上限 8、接触 2.4 秒、成功冷却 1.8 秒、打断禁用 5 秒、玩家撮合窗口 3 秒。
- 武器伤害 8、射击间隔 0.22 秒、弹匣 3、换弹 1 秒、射程 18 米。
- 蓄力 0.18/1.2 秒、满蓄力 0 伤害、击退半径 4 米、满蓄力速度 14 米/秒。
- 瞄准捕获 10% 屏高、保留 1.35 倍、换锁 55%。
- 命中眩晕 1.5 秒、分裂眩晕 2 秒、停稳速度 0.1 米/秒、停稳时间 0.1 秒。
- 警戒预警 1 秒、持续 10 秒、传播 5 米、资源争夺 3.4 米/1.5 秒/5 秒冷却。
- 身体分、货物每份 6 分、敌对核心最终得分减半、NPC 击杀不计玩家分。
- 资源账本和体量账本公式。

## 修复模块依赖

```text
R0 审查报告
  -> R1 玩家控制
  -> R2 行为优先级与观察暂停
  -> R3 资源行为语义
  -> R4 融合与核心行为语义
  -> R5 警戒、粘液与探索
  -> M2–M9 回归
```

数值冲突项不会随上述模块修复而改变。

## R4/R5 修复回填（2026-10-19）

- B07 融合外力连接、普通接触连接与眩晕合并：已实现并经自动场景验证。外力路径改为扫掠即时提交，普通接触仍为 2.4 秒；融合眩晕取双方较长值。
- B08 射击打断冷却快照：已实现 N07。旧冷却已满足时 0.8 秒附加冷却不阻止撮合；旧冷却未结束时仍拒绝。
- B09 核心近身玩家优先与目标切换：已实现并通过 `SlimeEcologyCorePriorityTest`。
- B10 警戒后空载休息、粘液趋近、争夺消退和探索历史：已实现并通过 R5 自动场景。
- N07 状态更新为 `Approved Change – R4 Implemented`。
- N06 的 0.65 m / 0.45 m/s 仍为 `Recorded – No Change`；R4 使用既有 8 cm 位移撮合容差，未采用冲突值。
