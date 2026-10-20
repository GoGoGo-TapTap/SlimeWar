# 主菜单场景对接

**关卡**：`Content/_SlimeWar/Maps/L_MainMenu`

## 已摆放内容

| 类型 | 名称 | 对应界面 |
|---|---|---|
| 摄像机 | `Camera_MainMenu` | 开始界面 |
| 摄像机 | `Camera_LevelSelect` | 关卡选择 |
| 摄像机 | `Camera_Prepare` | 准备页 |
| 角色 | `Character_MainMenu` | 开始界面 |
| 角色 | `Character_LevelSelect` | 关卡选择 |
| 角色 | `Character_Prepare` | 准备页 |

场景内含飞碟背景模型 + 定向光 + 天光。

## 需要程序做

1. 把 `L_MainMenu` 设为默认启动关卡
2. 进关卡后显示 `WBP_SlimeMainMenu`
3. 界面切换时调用 `SetViewTargetWithBlend` 切到对应摄像机（带平滑过渡）
4. 切摄像机同时用 `SetActorHiddenInGame` 控制角色显隐：
   - 开始界面 → 显示 Character_MainMenu，隐藏其他两个
   - 关卡选择 → 显示 Character_LevelSelect，隐藏其他两个
   - 准备页 → 显示 Character_Prepare，隐藏其他两个
