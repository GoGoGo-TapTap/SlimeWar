# 俯瞰图、落地动画与结算动画制作指南

适用日期：2026-10-20

读者：策划、关卡美术、UE 编辑器操作人员

## 1. 先明确三个概念

| 内容 | 用途 | 配置位置 |
|---|---|---|
| 俯瞰图 | 准备页选落点 | `/Game/_SlimeWar/UI/Textures/` |
| 落地动画 | 玩家确认落点后，镜头从高空俯冲到落点 | `DA_Level_*.DeploySequences` |
| 结算动画 | 回合结束后，结算页出现前的环绕/推轨镜头 | `DA_Level_*.ResultSequence` |

动画和俯瞰图是表现层，不改变玩法数据。落地时长由 `DA_RunConfig.DeployDuration` 门控；结算镜头由 `ResultOrbitDuration` 门控。序列长度只负责画面，不能代替数据时长。

## 2. 制作俯瞰图

每张地图制作一张正交俯视图，不能使用透视截图。

### 2.1 准备

1. 打开要制作的地图。
2. 关闭角色、UI、调试绘制和临时辅助 Actor。
3. 确认地图可玩范围，记录：
   - `MinX / MaxX`
   - `MinY / MaxY`
   - 地图中心

### 2.2 SceneCapture2D 设置

1. Place Actors 搜索 `SceneCapture2D`，放到地图中心。
2. Location 的 Z 设在地图最高建筑上方，例如 `Z=5000`。
3. Rotation：
   - Pitch：`-90`
   - Yaw：`0`
   - Roll：`0`
4. SceneCaptureComponent2D：
   - `Projection Type = Orthographic`
   - `Ortho Width = max(MaxX-MinX, MaxY-MinY)`
   - `Capture Source = Final Color LDR`
5. 创建 `TextureRenderTarget2D`，长边 2048，短边按地图比例。
6. 把 RT 指定给 SceneCapture 的 `Texture Target`。
7. Capture 一次，右键 RT 创建或导出静态纹理。

建议命名：

```text
T_Overhead_RooftopCourt
T_Overhead_LV2
T_Overhead_LV3
```

保存到：

```text
/Game/_SlimeWar/UI/Textures/
```

### 2.3 俯瞰图验收

- 从正上方看，没有透视倾斜
- 地图完整，没有裁掉站点、矿点或落点
- 没有 UI、角色、调试线和调试文字
- 图片比例与地图一致，不要强行拉成 16:9
- 准备页的 marker 与图片使用同一矩形区域

### 2.4 marker 坐标换算

图片坐标以左上角为原点：

```text
u = (DropPoint.X - MinX) / (MaxX - MinX)
v = 1 - (DropPoint.Y - MinY) / (MaxY - MinY)
```

marker 像素位置：

```text
X = u × 图片宽度
Y = v × 图片高度
```

准备页中的 marker 和背景图必须使用同一个 Canvas 矩形；不要用会拉伸的普通 Image 铺满不同比例区域。

## 3. 制作落地动画

每个落点需要一条独立序列，序列索引必须与 `DA_SpawnLayout.DropPoints` 一致。

### 3.1 推荐命名

```text
LS_Deploy_RooftopCourt_D1
LS_Deploy_RooftopCourt_D2
LS_Deploy_LV2_D1
LS_Deploy_LV2_D2
LS_Deploy_LV3_D1
LS_Deploy_LV3_D2
```

放在：

```text
/Game/_SlimeWar/UI/Cinematics/
```

### 3.2 创建 Level Sequence

1. 打开对应地图。
2. Content Browser 右键 → `Cinematics` → `Level Sequence`。
3. 命名并保存。
4. 打开 Sequencer。
5. 设置显示帧率，建议 30 FPS。
6. 在场景中放一个 `CineCameraActor`，作为动画相机。
7. 将相机加入序列：
   - 手动关键帧：把相机作为 Possessable 或可拖入序列的 Actor。
   - 需要多机位时使用 `Camera Cut Track`。

### 3.3 相机动画建议

以落点 `D1` 为例：

1. 在序列开始帧，把相机放在落点上方，面向落点。
2. 在序列结束帧，把相机降到落点附近的最终机位。
3. 选中相机 Transform 轨道，设置关键帧。
4. 把第一帧插值设为平滑曲线，避免俯冲时突然变向。
5. 最后一帧要平稳，不要在落地瞬间继续大幅抖动。
6. 相机不要穿墙、穿地或进入角色身体。
7. 如果在俯冲过程中能看到玩家，玩家位置应与当前 DropPoint 一致。

建议镜头节奏：

```text
0%   高空俯视，能看到落点和周边地形
60% 快速下降，画面开始靠近地面
100% 稳定落在玩家视角附近，给后续 Blend 留空间
```

### 3.4 序列时长

打开 `DA_RunConfig_*`，查看 `DeployDuration`。

序列 Playback Range 必须与它一致，例如：

```text
DeployDuration = 2.0 s
LS_Deploy_* 长度 = 2.0 s
```

不要只改序列长度或只改 RunConfig。两者不一致时，日志会提示序列长度和 `DeployDuration` 不匹配；实际玩法以 RunConfig 为准。

### 3.5 填入关卡资产

打开对应 `DA_Level_*`：

1. 在 `DeploySequences` 数组中按落点顺序添加：
   - 数组 0 = DropPoint 0 的落地序列
   - 数组 1 = DropPoint 1 的落地序列
2. 不要交换数组顺序，否则选第一个落点会播放第二个动画。
3. 如果关卡留空，会回退到全局 `DeploySequences`。

## 4. 制作结算动画

结算动画在回合结束后、结算页出现前播放，每个关卡可单独配置。

### 4.1 推荐命名

```text
LS_Result_RooftopCourt
LS_Result_LV2
LS_Result_LV3
```

### 4.2 镜头目标

结算镜头要让玩家快速看懂本局战场：

- 能看到主要站点和战斗区域
- 能看到大体量史莱姆或核心爆炸后的结果
- 不要只拍天空、墙体或空地面
- 不要把镜头穿进建筑

可以用以下两种方式：

1. **环绕**：相机围绕战场中心旋转 60°~120°。
2. **推轨**：从高处向战场中心缓慢推进。

### 4.3 Sequencer 制作步骤

1. 打开对应地图。
2. 新建 Level Sequence，命名 `LS_Result_*`。
3. 放置一个 `CineCameraActor`。
4. 在序列中加入相机。
5. 第一帧设置广角全景。
6. 最后一帧设置可读性最好的构图。
7. 给相机 Transform、FOV 或焦距打关键帧。
8. 检查镜头是否穿过墙体、地面或大型装饰。
9. 在序列中预览，确保没有 UI 覆盖在画面上。

### 4.4 时长

打开对应 `DA_RunConfig_*`，设置：

```text
ResultOrbitDuration = 序列长度
```

例如：

```text
ResultOrbitDuration = 2.5 s
LS_Result_* 长度 = 2.5 s
```

如果 RunConfig 时长先到，结算页会盖在尚未播放完的镜头上；如果序列先结束，画面会提前静止。两者必须一致。

### 4.5 填入关卡资产

打开对应 `DA_Level_*`：

1. 把结算序列填到 `ResultSequence`。
2. 如果留空，会使用全局 `ResultSequence`。
3. 不要让一张通用结算序列同时服务于构图差异很大的新地图；优先使用关卡自己的序列。

## 5. 编辑器测试流程

### 俯瞰图

1. 打开准备页，检查背景图。
2. 点击每个 marker，确认其位置与地图世界坐标对应。
3. 检查 16:9 和 21:9 下图片没有被拉伸。

### 落地动画

1. 进入关卡准备页。
2. 选择 DropPoint 0，确认播放 `DeploySequences[0]`。
3. 重新进入，选择 DropPoint 1，确认播放 `DeploySequences[1]`。
4. 序列结束后，操作角色，确认控制没有被镜头锁住。
5. 观察镜头是否在落地后平滑 Blend 回玩家。

### 结算动画

1. 进入关卡，控制台执行 `SlimeRunSetTime 1`。
2. 回合结束，确认先播放结算序列，再出现结算页。
3. 检查序列结束时没有突然切镜或黑屏。
4. 检查结算页出现时镜头已经稳定。

### 日志检查

Output Log 搜索：

```text
USlimePresentationDirector
```

如果出现序列长度与 `DeployDuration` / `ResultOrbitDuration` 不一致的 Warning，回到对应 DA 改时长或改序列 Playback Range。

## 6. 交付检查表

- [ ] 三张地图都有正交俯瞰图
- [ ] 俯瞰图没有透视、UI、角色和调试绘制
- [ ] 每个落点有独立落地序列
- [ ] 落地序列索引与 `DropPoints` 顺序一致
- [ ] 落地序列长度等于 `DeployDuration`
- [ ] 结算序列长度等于 `ResultOrbitDuration`
- [ ] 落地序列结束不锁玩家输入
- [ ] 结算序列不会穿墙、穿地
- [ ] 结算序列不会提前出现结算页
- [ ] 序列已填进对应 `DA_Level_*`
- [ ] PIE 中每个落点和每张地图都验收过

## 7. 重要约定

- `DropPoints` 顺序、`DeploySequences` 顺序和准备页 marker 顺序必须一致。
- 不要在 WBP 里自己做镜头动画；镜头由 Level Sequence 驱动。
- 不要让序列时长决定玩法时长；玩法时长以 `DA_RunConfig` 为准。
- 结算序列不要与结算照片相机混为一谈。结算照片相机是独立机位，序列只负责运镜。
- 修改 `.umap` 或 `.uasset` 前先认领资产，避免多人同时覆盖。
