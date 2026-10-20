# 史莱姆死亡印迹 Decal 美术需求

日期：2026-10-20

## 1. 目标

史莱姆死亡后在地面留下阵营颜色的粘液印迹。

- 只在静态地面显示
- A/B/C 三个阵营使用对应颜色
- 玩家、史莱姆、货物、手雷、站点和矿点不应被 Decal 染色
- Decal 不参与碰撞，不影响导航

## 2. 当前资产

- 材质：`/Game/_SlimeWar/Enemy/Art/M_Slime_Splatter_Decal`
- 纹理：`/Game/_SlimeWar/Enemy/Art/T_Slime_Splatter_01`

当前材质是 `Deferred Decal`，可以作为地面投影使用，但目前没有可编辑的阵营颜色参数，所以所有印迹只能是固定颜色。

## 3. 需要美术修改的材质要求

推荐保留一个材质，新增可编辑参数：

- 材质域：`Deferred Decal`
- 混合模式：保持当前可用的 Decal 透明混合
- `SplatterTexture`：采样 `T_Slime_Splatter_01`
- `TintColor`：Vector Parameter，默认白色，用于阵营染色
- `Opacity`：Scalar Parameter，默认约 0.8

颜色合成建议：

```text
BaseColor = SplatterTexture.RGB × TintColor
Opacity   = SplatterTexture.Alpha × Opacity
```

不要将阵营颜色直接烘死到纹理里，否则后续无法统一调整三个阵营。

如果美术更习惯材质实例，也可以提供：

```text
MI_Slime_Splatter_A
MI_Slime_Splatter_B
MI_Slime_Splatter_C
```

三个实例都基于同一个材质，只改 `TintColor`。

## 4. 阵营颜色

| 阵营 | 颜色 | RGB |
|---|---|---|
| A | 蓝色 `#84DCFE` | `(0.518, 0.863, 0.996)` |
| B | 红色 `#F3BAC4` | `(0.953, 0.729, 0.769)` |
| C | 黄色 `#FFF3BC` | `(1.000, 0.953, 0.737)` |

颜色应与史莱姆身体材质保持同一套色值。

## 5. 纹理要求

- 纹理内容为透明背景的粘液形状
- RGB 使用白色或浅灰白色，方便通过 `TintColor` 染色
- Alpha 表示真实印迹范围
- 不要烘焙阵营颜色
- 建议 1024×1024 或 2048×2048
- 边缘保留透明留白，避免 Decal 边界被硬裁切
- 形状不要依赖固定方向；运行时会随机旋转

## 6. 运行时使用方式

游戏目前会在史莱姆死亡位置生成一个地面 Decal：

- 位置：死亡点上方约 2 cm
- 朝向：向下投影，Yaw 随机
- 大小：根据死亡史莱姆半径计算
- 生命周期：当前保持到本局重置，不做自动淡出

材质应适配这种直接贴地的静态投影，不要使用屏幕空间效果，也不要把玩家/角色轮廓写入材质。

## 7. 验收标准

- A 阵营死亡印迹为蓝色，B 为红色，C 为黄色
- 同一阵营的颜色在不同死亡位置保持一致
- 印迹只出现在地面，不覆盖玩家和史莱姆
- 不出现明显的方形底边、黑色背景或白色方块
- Decal 不闪烁、不陷地、不穿墙
- 场地边缘和有高差的台阶上不会出现严重拉伸
- 材质中没有硬编码颜色；颜色来自 `TintColor` 或对应材质实例

## 8. 交付后程序侧对接

美术完成材质后，程序需要把 Decal 材质按 A/B/C 阵营选择：

- A 使用 A 颜色/material instance
- B 使用 B 颜色/material instance
- C 使用 C 颜色/material instance

如果只提供一个带 `TintColor` 参数的材质，程序在创建 Decal 动态材质实例时设置对应阵营颜色。
