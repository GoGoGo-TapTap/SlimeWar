# 手雷模型对接

## 交付资产

路径：`Content/_SlimeWar/Player/Grenades/`

| 资产 | 类型 | 用途 |
|---|---|---|
| SM_Grenade_Assimilation_A | 静态网格体 | A阵营同化雷 |
| SM_Grenade_Assimilation_B | 静态网格体 | B阵营同化雷 |
| SM_Grenade_Assimilation_C | 静态网格体 | C阵营同化雷 |
| SM_Grenade_Tornado | 静态网格体 | 旋风雷（三阵营共用） |
| M_Grenade_Master | 材质 | 四种手雷共享材质 |

## 材质参数

| 参数名 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| FactionTint | Vector Parameter | 蓝色(A阵营) | 运行时设置阵营颜色 |
| EmissiveStrength | Scalar Parameter | 1.5 | 自发光强度 |

颜色合成：`自发光颜色 = FactionTint × EmissiveStrength`，主体为深灰金属色。

## 颜色配置

| 手雷类型 | 颜色 | RGB |
|---|---|---|
| 同化雷 A | #84DCFE | (0.518, 0.863, 0.996) |
| 同化雷 B | #F3BAC4 | (0.953, 0.729, 0.769) |
| 同化雷 C | #FFF3BC | (1.000, 0.953, 0.737) |
| 旋风雷 | #78FF96 | (0.471, 1.000, 0.588) |

## 程序需要做

1. **动态设置颜色**：创建动态材质实例，按类型设置 `FactionTint`：
   - 同化雷 A/B/C 使用对应阵营色
   - 旋风雷使用专属绿色 #78FF96

2. **碰撞**：继续使用现有12cm球体碰撞，**不要用模型碰撞**（模型未生成碰撞）。

3. **朝向**：模型当前引信朝Z轴（朝上），飞行时在组件层面**旋转X轴-90°**，使引信朝X轴（飞行前方）。

4. **尺寸**：模型约24~30cm，轴心在几何中心。
