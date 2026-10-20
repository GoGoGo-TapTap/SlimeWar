# SlimeWar 关卡与资源配置执行清单

适用日期：2026-10-20

状态：`[ ]` 未完成 / `[~]` 进行中 / `[x]` 已完成

本文只记录 UE 编辑器中的游戏资产配置，不包含 WBP 制作和控件连线。

## 0. 程序侧状态

- [x] `USlimeEcologyRules` 增加 `FSlimeFactionVisual`、`FactionVisuals`、`FactionColors`
- [x] `USlimeEcologyRules` 增加 `SpikeMesh`、`CoreMesh`、`WeakpointMesh`、`CoreMaterial`、`WeakpointMaterial` 默认资源
- [x] `ASlimeEcologyEnemy` 增加 `Spike_Left`、`Spike_Right`、`CoreMesh`、`WeakpointMesh`
- [x] 阵营变化、融合/分裂、体量变化后重新应用阵营模型和材质
- [x] 攻击性核心显示尖刺，普通状态隐藏
- [x] mass 7/8 弱点网格跟随 Weakpoint 组件显示
- [x] `USlimePaintSubsystem::GetFactionColor` 改为 v2 阵营色
- [x] `ASlimeFactionSite::DisplayColor` 默认改为 A 阵营蓝
- [x] 编译 `SlimeWarEditor Win64 Development`：0 error
- [x] 三个 SpawnLayout 已写入实际落点坐标
- [x] 三张地图已创建站点、矿点、PaintField 并保存
- [x] 粘液改用 `M_Slime_Splatter_Decal` 生成 Decal；`PaintSplatMaterial` / `PaintSurfaceMaterial` 可为空
- [x] Rooftop A 站点已调整为 `(-680, -810, 0)`
- [x] Rooftop 不可再生矿点已调整为 `(0, -210, 0)`
- [x] 矿点默认球体模型已移除，使用场景内真实模型
- [ ] 打开三张地图分别执行 `SlimeBakeNav`，等待导航烘焙完成后保存
- [ ] 在 LV2/LV3 分别确认 A/B/C 阵营材质实例不同
- [ ] 确认 Decal 只落在静态地面，玩家/史莱姆/货物不被染色

## 1. 项目设置

- [ ] **Project Settings → Game → Slime War**
- [ ] `Levels` 顺序为：
  1. `DA_Level_CityPlaza`
  2. `DA_Level_DoublePoint`
  3. `DA_Level_ThreePoint`
- [ ] 第四槽不添加资产
- [ ] 检查全局 `RunConfig` 不是空引用
- [ ] 检查 HUD/Preparation/Result/Pause/MainMenu/Settings/LevelSelect 的类路径没有丢失

## 2. 关卡资产

- [ ] `DA_Level_CityPlaza`
  - `DisplayName = Rooftop Court`
  - `MapAsset = L_Rooftop_Court`
  - `RunConfig = DA_RunConfig_RooftopCourt`
- [ ] `DA_Level_DoublePoint`
  - `DisplayName = LV2`
  - `MapAsset = L_LV2`
  - `RunConfig = DA_RunConfig_DoublePoint`
- [ ] `DA_Level_ThreePoint`
  - `DisplayName = LV3`
  - `MapAsset = L_LV3`
  - `RunConfig = DA_RunConfig_ThreePoint`
- [ ] `PreparationWidgetClass`、`DeploySequences`、`ResultSequence` 按当前 UI/镜头进度填写；没有就留空使用回退

## 3. RunConfig 和 SpawnLayout

- [ ] 复制 `DA_RunConfig_DoublePoint` 为 `DA_RunConfig_RooftopCourt`
- [ ] `bAutoStartRun = false`
- [ ] `SlimeStatTable = DT_SlimeStats`
- [ ] `WeaponStatTable = DT_WeaponStats`
- [ ] `EcologyRules = DA_EcologyRules`
- [ ] `WeaponRules = DA_Weapon_Rifle`
- [ ] `DefaultWeaponId = Rifle`
- [ ] 新建 `DA_SpawnLayout_RooftopCourt`
- [ ] 将 Rooftop SpawnLayout 指定给 `DA_RunConfig_RooftopCourt`
- [ ] 检查 `DA_RunConfig_DoublePoint` 和 `DA_RunConfig_ThreePoint` 的 SpawnLayout 引用
- [ ] 每个 SpawnLayout 的 `DropPoints` 按实际地图重新填 2 项（若实际不是 2 个，按实际数量调整）

## 4. 坐标采集表

每张地图填写一次；坐标单位是 cm。

| 项目 | L_Rooftop_Court | L_LV2 | L_LV3 |
|---|---|---|---|
| MinX |  |  |  |
| MaxX |  |  |  |
| MinY |  |  |  |
| MaxY |  |  |  |
| 地图中心 |  |  |  |
| DropPoint 0 |  |  |  |
| DropPoint 1 |  |  |  |
| Site A |  |  |  |
| Site B |  |  |  |
| Site C |  |  |  |
| PlayerStart |  |  |  |
| PaintField Center |  |  |  |
| PaintField WorldSize |  |  |  |

### 地图边界采集方法

1. 视口切到 Top 视图。
2. 隐藏天空、角色和无关装饰。
3. 检查地面/外墙边界。
4. 必要时在四个角临时放 `TargetPoint`，读取 Transform → Location。
5. 记录后删除临时 TargetPoint。

### marker 归一化位置

```text
u = (DropPoint.X - MinX) / (MaxX - MinX)
v = 1 - (DropPoint.Y - MinY) / (MaxY - MinY)
```

准备页 marker 位置：

```text
X = u × 图片宽度
Y = v × 图片高度
```

## 5. 地图 Actor

### ASlimeFactionSite

- [ ] 每张地图的站点数量符合设计
- [ ] `SiteId = A/B/C`
- [ ] `Faction` 与 SiteId 对应
- [ ] `InitialPopulation = 10`
- [ ] `ActivityRadius = 1200`
- [ ] `DeliveryRadius = 130`
- [ ] `SpawnRingMin = 250`
- [ ] `SpawnRingMax = 400`
- [ ] A 站点 `DisplayColor = (0.518, 0.863, 0.996, 1)`
- [ ] B 站点 `DisplayColor = (0.953, 0.729, 0.769, 1)`
- [ ] C 站点 `DisplayColor = (1.000, 0.953, 0.737, 1)`
- [ ] 站点位于 NavMesh 上、墙体外部、落点之外

### ASlimeMineralPatch

- [ ] 每张地图的矿点数量符合设计
- [ ] `PatchId` 唯一
- [ ] 填写 `InitialStock`、`MaxStock`
- [ ] 按设计填写 `bRegenerates`、`RegrowSeconds`
- [ ] `GatherRange = 120`
- [ ] 矿点位于可导航地面

### ASlimePaintField

- [ ] 每张地图只放一个
- [ ] 位置在地图中心，Z 约 2
- [ ] `WorldSize = (MaxX-MinX, MaxY-MinY)`
- [ ] `PaintMaterial` 先留空，使用 `DA_EcologyRules.PaintSurfaceMaterial`

## 6. 正交俯瞰图

每张地图单独执行：

- [ ] 放置 `SceneCapture2D`
- [ ] 位置在地图中心上方，Z 约 5000
- [ ] Rotation：Pitch `-90`，Yaw `0`，Roll `0`
- [ ] `Projection Type = Orthographic`
- [ ] `Ortho Width` 取地图长边
- [ ] 创建 RenderTarget：
  - `T_Overhead_RooftopCourt_RT`
  - `T_Overhead_LV2_RT`
  - `T_Overhead_LV3_RT`
- [ ] 长边 2048，另一边按地图比例
- [ ] `Capture Source = Final Color LDR`
- [ ] 隐藏 UI、角色、调试绘制和临时辅助 Actor
- [ ] 导出/创建静态纹理：
  - `T_Overhead_RooftopCourt`
  - `T_Overhead_LV2`
  - `T_Overhead_LV3`
- [ ] 静态纹理放到 `/Game/_SlimeWar/UI/Textures/`
- [ ] 删除或隐藏临时 SceneCapture

## 7. 史莱姆材质和模型

### 材质

- [ ] 打开 `M_Slime_Base`
- [ ] 确认有 `TintNormal`、`Opacity`、`RimStrength`、`JiggleSpeed`、`JiggleStrength`
- [ ] 身体 `Opacity` 约 0.7
- [ ] 没有 `TintAggro` / `DangerTint` 依赖
- [ ] `MI_Slime_FactionA.TintNormal = #84DCFE`
- [ ] `MI_Slime_FactionB.TintNormal = #F3BAC4`
- [ ] `MI_Slime_FactionC.TintNormal = #FFF3BC`
- [ ] `MI_Slime_Core` 深色不透明
- [ ] `MI_Slime_Weakpoint` 黄色不透明

### DA_EcologyRules

- [ ] `FactionVisuals[A].BodyMesh = SM_Slime_Base` 或独立 A 模型
- [ ] `FactionVisuals[A].BodyMaterial = MI_Slime_FactionA`
- [ ] `FactionVisuals[B]` 同理
- [ ] `FactionVisuals[C]` 同理
- [ ] `SpikeMesh = SM_Slime_Spike`
- [ ] `CoreMesh = SM_Slime_Core`
- [ ] `WeakpointMesh = SM_Slime_Weakpoint`
- [ ] `CoreMaterial = MI_Slime_Core`
- [ ] `WeakpointMaterial = MI_Slime_Weakpoint`
- [ ] `FactionColors` 使用 v2 三种颜色（若资产中显示该字段）

### 视觉表现检查

- [ ] Core 所有史莱姆默认显示
- [ ] Weakpoint 仅在 mass 7/8 显示
- [ ] Spike 仅在攻击性核心显示
- [ ] 同化雷换阵营后模型/材质即时变化
- [ ] 融合、分裂、体量变化后外观不丢失
- [ ] 三个配件无网格碰撞

## 8. 导航和最终验收

- [ ] 地面有 Simple Collision
- [ ] 墙体/货架使用 Static Mobility + BlockAll
- [ ] 可站区域有 NavMesh
- [ ] 控制台执行 `SlimeBakeNav`
- [ ] 三张地图准备页落点与 SpawnLayout 一致
- [ ] `SlimeRunDeploy 0`、`SlimeRunDeploy 1` 落点正确
- [ ] 三张地图都能从选关进入
- [ ] A/B/C 站点颜色和出生史莱姆正确
- [ ] 矿点、粘液、Cargo、站点标签无明显错位
- [ ] 日志无缺失资产/缺失面板引用警告
- [ ] 关闭编辑器后编译 `SlimeWarEditor Win64 Development`，0 error
