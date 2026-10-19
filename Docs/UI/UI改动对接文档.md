# UI 改动对接文档

2026-10-19

---

## 一、瞄准层蓝图（WBP_AimPreview）

瞄准层蓝图做好了，需要挂到玩家视口里才能看到。

瞄准层蓝图 WBP_AimPreview，需要程序对接：

蓝图路径：Content/_SlimeWar/UI/WBP_AimPreview

需要在玩家 BeginPlay 时 Create Widget 并 Add to Viewport

需要绑定的控件：

- Img_ChargeRing_Fill：用了材质实例 MI_ChargeRing，蓄力时动态设置参数 ChargePercent（0~1）
- Text_ChargePercent：蓄力百分比文字，格式 "0%" ~ "100%"
- Img_Crosshair_Default：默认十字准星，默认可见
- Img_Crosshair_Weakpoint：弱点准星（黄色），默认已折叠，锁定弱点时切可见

蓄力机制参考：按住 >0.18s 开始蓄力，1.2s 满蓄

---

## 二、受伤红屏后期处理材质（M_HurtIndicator_PP）

受伤红屏后期处理材质：Content/_SlimeWar/UI/M_HurtIndicator_PP

材质域：后期处理

参数：Intensity（0~1），0 = 无红屏，1 = 满红屏

用法：挂到 Post Process Volume 的 Blendables 里，受击时把 Intensity 从 0 设到 1，然后用插值降回 0，实现闪一下淡出

---

## 三、特殊子弹UI（WBP_SlimeHUD）

特殊子弹UI已加在WBP_SlimeHUD里，

控件名：

- 同化弹（键1）：Img_Assimilate_Icon（icon显隐/高亮/变灰）、Text_Special_Assimilate_Ammo（数量文字）
- 旋风弹（键2）：Img_Vortex_Icon（icon显隐/高亮/变灰）、Text_Special_Vortex_Ammo（数量文字）
- 数量默认3，切换时当前选中的icon高亮，数量为0时变灰
