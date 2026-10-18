# UI 接口蓝图使用手册

> **给谁看**：负责 WBP 的人。  
> **怎么用**：§2 / §3 是两张核心页面的分步操作；§1 是代码改动速查；§7 是数据配置。
>
> **维护约定**：以后只要有新接口或语义变化，**必须同步到 §1 对应类的那张表格**里，  
> 再去相应页面章节补「怎么接」。表格是索引，漏了表就等于漏了接口。

---

## 0. 读图约定（先看这个）

文档里的连线图混用了两种线，先分清它们：

| 线             | 颜色          | 作用                         |
| ------------- | ----------- | -------------------------- |
| **exec（执行线）** | **白色**，三角引脚 | 决定**先跑谁**。只有沿着白线连到的节点才会被执行 |
| 数据线           | 彩色圆点        | 只传值，**不决定执行**              |

常见引脚颜色：

```text
白  执行流          Event 右侧、Bind 左侧、Set Visibility 两侧
蓝  对象引用        Btn_DropPoint0、Self
绿  整数/浮点       Drop Point Count、常数 0 / 1
红  布尔           Equal 的输出、Is Enabled
粉  字符串/FText    Print String 的 In String
```

**三条规则：**

1. **一个 exec 输出只能接一条线。** 想同时做两件事，中间放一个  
   `Sequence` 节点（它的 `Then 0`、`Then 1` 才是并联出口）。  
   直接从一个三角拖两条线出去，引擎只会执行其中一条，而且不确定是哪条——这是最常见的隐形 bug。
2. **纯函数节点没有 exec。** `Get Drop Point Count`、`Equal`、`Less`、`Select` 这类节点  
   只有数据引脚，它们在"输出被拉取的那一刻"计算，不需要也不能连白线。
3. **Event 节点的白线是起点。** `Event Construct`、`Event On Preparation Ready`  
   右侧的白三角就是这条执行流的开头，所有动作都从这里往后接。

---

## 1. 代码调整一览

按文件列出改动。`蓝图` 列标记该改动是否需要在 WBP 里配合。

### `SlimeUISubsystem.h / .cpp`

| 项                     | 类型     | 内容               | 蓝图        |
| --------------------- | ------ | ---------------- | --------- |
| `ShowMainMenu()`      | 新增 函数  | 只切屏，不重载关卡        | 用：菜单返回    |
| `ReturnToMainMenu()`  | 语义变更   | 仍是重载世界，只给"离开一局"用 | 不要用于菜单间返回 |
| `GetCurrentLevelId()` | 新增 纯函数 | 当前关卡 ID，未选关为空    | 可选：调试显示   |
| `OpenLevelSelect()`   | 不变     | 显示选关页            | 主菜单已接好    |
| `RequestQuit()`       | 语义变更   | 回主菜单（不再是退出游戏）    | 见 §5      |
| 落点页创建时机               | 行为变更   | 改成第一次要显示时才创建     | 无感        |

### `SlimePreparationWidget.h / .cpp`

| 项                             | 类型     | 内容             | 蓝图          |
| ----------------------------- | ------ | -------------- | ----------- |
| `OnPreparationReady(int32)`   | 事件     | 传入落点数量，构造时触发一次 | **必须实现**    |
| `OnDropPointPreviewed(int32)` | 事件     | 某个落点被选中预览      | **必须实现**    |
| `PreviewDropPoint(int32)`     | 函数     | 记录预览下标并触发上面的事件 | 点 marker 时调 |
| `ConfirmDropPoint(int32)`     | 函数     | 预览 + 确认一步完成    | 单击即进局才用     |
| `ConfirmPreviewedDropPoint()` | 函数     | 确认当前预览的那个      | 用：`Btn_Go`  |
| `RequestBackToMainMenu()`     | 新增 函数  | 离开落点页回主菜单      | 用：返回按钮      |
| `GetPreviewedDropPoint()`     | 纯函数    | 当前预览下标         | 可选          |
| `GetLevelId()`                | 新增 纯函数 | 本页所属关卡 ID      | 可选          |
| `GetDropPointCount()`         | 新增 纯函数 | 本关落点数量         | 用：marker 自检 |

### `SlimeLevelSelectWidget.h / .cpp`

| 项                            | 类型     | 内容               | 蓝图        |
| ---------------------------- | ------ | ---------------- | --------- |
| `GetLevelDisplayName(int32)` | 新增 纯函数 | 槽位关卡名            | 用：节点标题    |
| `GetLevelId(int32)`          | 新增 纯函数 | 槽位关卡 ID          | 可选：缩略图命名  |
| `GetConfiguredLevelCount()`  | 新增 纯函数 | 已配置的关卡数（≤ 4）     | 用：动态生成节点时 |
| `GetLevelCount()`            | 不变     | 仍返回 4（UI 槽位数）    | 固定节点时用它   |
| `IsLevelAvailable(int32)`    | 语义变更   | 现在看数据资产配没配地图     | 用：锁定态     |
| `EnterSelectedLevel()`       | 行为变更   | 读关卡资产并记录关卡身份     | 不变        |
| `ReturnToMainMenu()`         | 行为变更   | 改成**纯切屏**，不再重载关卡 | 不变（体验变好）  |
| `SetReturnWidget()`          | **删除** | 无 WBP 调用方        | 连过的话删掉这根线 |

### `SlimeMainMenuWidget.h / .cpp`

| 项                | 类型   | 内容                              | 蓝图 |
| ---------------- | ---- | ------------------------------- | -- |
| `StartGame()`    | 行为变更 | 改成调 `OpenLevelSelect()`，不再直接开地图 | 不变 |
| `OpenSettings()` | 不变   | 临时创建设置页                         | 不变 |
| `QuitGame()`     | 不变   | 真正退出游戏，唯一入口                     | 不变 |

### `SlimeSessionSubsystem.h / .cpp`

| 项                        | 类型     | 内容          | 蓝图    |
| ------------------------ | ------ | ----------- | ----- |
| `GetSelectedLevelId()`   | 新增 纯函数 | 会话记录的关卡 ID  | 可选    |
| `GetSelectedDropPoint()` | 不变     | 已记录的落点下标    | 可选    |
| 其余（请求标志）                 | 新增     | 仅供 C++ 内部流转 | 蓝图不要碰 |

### `SlimePauseWidget.h` / `SlimeResultWidget` / `SlimeSettingsWidget`

| 项               | 类型   | 内容         | 蓝图   |
| --------------- | ---- | ---------- | ---- |
| `RequestQuit()` | 语义变更 | 转发到"回主菜单"  | 见 §5 |
| 其他函数与事件         | 不变   | 签名与触发时机都没动 | 不变   |

### `SlimeGameSettings.h` / `SlimeLevelDefinition.h`（配置类）

| 项                                    | 类型     | 内容                      | 蓝图        |
| ------------------------------------ | ------ | ----------------------- | --------- |
| `Levels`                             | 新增 数组  | 关卡清单，顺序 = 选关页槽位         | 用：§7.3 加项 |
| `bSkipMainMenuInPIE`                 | 新增 开关  | PIE 跳过主菜单直接进落点页，默认关     | 用：§4.6 调试 |
| `PreparationWidgetClass`             | 降级为回退值 | 关卡没配落点 WBP 时才用它         | 不用动       |
| `DeploySequences` / `ResultSequence` | 降级为回退值 | 关卡没配时才用它                | 不用动       |
| `USlimeLevelDefinition` 的 7 个字段      | 新增 数据类 | 地图 / 运行配置 / 落点 WBP / 序列 | 用：§7.2 填写 |

### 与蓝图无关的内部改动

```text
SlimePresentationDirector.h/.cpp   部署/结算序列与时长改按关卡取
StatTableProvider.h/.cpp           新增 SelectLevel()，开局切换本关 RunConfig
RunSubsystem.cpp                   开局先切 RunConfig；重选落点补进游戏标志
SlimeHUDWidget.cpp                 只删了一行调试日志
```

---

## 2. `WBP_SlimePreparation`（落点选择页）

### 2.1 先把三个词说清楚

你说看不懂，多半卡在这几个词上：

| 词           | 指的是什么                                        | 在哪                                                   |
| ----------- | -------------------------------------------- | ---------------------------------------------------- |
| **落点**      | 玩家开局站的位置，本质是一个世界坐标                           | `DA_SpawnLayout::DropPoints`，一个数组                    |
| **marker**  | 俯瞰图上那个**可以点的圆点/图钉**，代表一个落点                   | WBP Designer 里，名字是 `Btn_DropPoint0`、`Btn_DropPoint1` |
| **预览 / 确认** | 点 marker 只是"我选这个"，**还没进游戏**；按 `Btn_Go` 才真正开始 | 两个不同的按钮                                              |

所以「marker 数量必须等于 `DropPoints` 数量」的意思是：  
**你在俯瞰图上摆了几个可点的圆点，`DA_SpawnLayout` 里就必须有几个坐标。**

### 2.2 marker 不是 UButton

这是最容易卡住的地方。`Btn_DropPoint0` 在层级里看着像一个按钮，但它其实是  
**`WBP_SelectableButton` 这个子控件的实例**——里面已经包好了一个 Button、一个选中态图标、三套状态函数。

所以：

```text
它没有 On Clicked 这个事件
它只有自己广播的 On Button Clicked，而且必须先 Bind Event to 才能用
```

这就是为什么照着"按钮 OnClicked → 调函数"的思路在它身上找不到东西。

### 2.3 这张页要达成什么

```text
玩家看到俯瞰图 -> 点一个圆点（预览）-> 点"确认"（进游戏）
```

五个部件：

| 部件   | 建议命名                              | 类型                               |
| ---- | --------------------------------- | -------------------------------- |
| 俯瞰图  | `Img_Map`                         | Image                            |
| 落点圆点 | `Btn_DropPoint0`、`Btn_DropPoint1` | `WBP_SelectableButton` 实例        |
| 确认按钮 | `Btn_Go`                          | 裸 `Button`                       |
| 返回按钮 | `Btn_Back`                        | 裸 `Button` 或 `WBP_GenericButton` |

### 2.4 第 1 步：在 Designer 里摆好

1. 打开 `WBP_SlimePreparation` 的 **Designer**。
2. `Img_Map` 铺底，换成这一关的俯瞰截图。
3. 从 Content Browser 把 `WBP_SelectableButton` 拖两个进来，命名 `Btn_DropPoint0`、`Btn_DropPoint1`。
4. 把它们拖到俯瞰图上**对应落点世界坐标的位置**。
5. 拖一个普通 `Button`，命名 `Btn_Go`，放在右下角。

**为什么位置要对应**：C++ 只认下标，不知道屏幕坐标。玩家点第 1 个圆点，游戏就传送到  
`DropPoints[0]` 那个世界坐标。所以圆点摆错位置，人就会落在错误的地方。

> **圆点必须放在 `Canvas Panel` 里，不能放在 `Overlay` 里。**  
> `Overlay` 的语义是"所有子控件铺满同一块区域、互相叠加"，它没有 `Position`，只有 `Padding`  
> 和 `Alignment`。拿 `Padding` 当偏移用，控件本身仍然占着一大块区域，两个圆点会完全重叠，  
> 点哪里都命中层级最上面那个，另一个像"消失了"。
>
> 正确结构：
>
> ```text
> [Root]
>   ├─ Img_Map           铺满，Hit Test Invisible
>   ├─ Canvas_Markers    Canvas Panel，铺满（承载圆点）
>   │    ├─ Btn_DropPoint0   Canvas Slot: Position (x1,y1) Size 48x48
>   │    └─ Btn_DropPoint1   Canvas Slot: Position (x2,y2) Size 48x48
>   ├─ Btn_Go
>   └─ Btn_Back
> ```
>
> 圆点的 `Anchors` 建议设成左上角 `(0,0)`，这样 `Position` 就是绝对像素，跨分辨率不会跑位。

> **`Btn_DropPointN` 实例自身必须是 `Visible`。**  
> 常见误改：把 marker 实例设成 `SelfHitTestInvisible`，以为"这样它就不会挡住别的控件"。  
> 结果这个实例的点击区域不可靠，点了没反应、也永远不会 `SetSelected`。  
> 要设 `Hit Test Invisible` 的是**子控件内部的装饰图**（`Img_SelectedIcon` 等），不是这个实例本身。
>
> **界面上的名字和内部枚举名不一样**，对照着看：
>
> | Widget Reflector / Inspector | Details 面板下拉                           | 显示 | 占位    | 能点 |
> | ---------------------------- | -------------------------------------- | -- | ----- | -- |
> | `Visible`                    | Visible                                | ✓  | ✓     | ✓  |
> | `Hidden`                     | Hidden                                 | ✗  | **✓** | ✗  |
> | `Collapsed`                  | Collapsed                              | ✗  | ✗     | ✗  |
> | `SelfHitTestInvisible`       | Not Hit-Testable (Self Only)           | ✓  | ✓     | ✗  |
> | `HitTestInvisible`           | Not Hit-Testable (Self & All Children) | ✓  | ✓     | ✗  |
>
> **要"能隐藏但位置一直留着"就用 `Hidden`**——它和 `Collapsed` 的唯一差别就是占不占位。  
> 想把图标区域的尺寸钉死，外面再套一个 `SizeBox`（**必须勾上 `Width/Height Override` 并填数值**，  
> 不勾的话 SizeBox 会去问内容的期望尺寸，内容一隐藏又塌回 0）；  
> 不要用 `Overlay`，它是叠放，不解决占位。
>
> 如果目的是"按钮还在、还能点，只是看不见底图"，**不要改 Visibility**——把 `Image` 的 `Brush`  
> 换成空或全透明贴图。`Hidden` 的控件不渲染也不接收点击。

> **复制实例后必须立刻改位置。** `Ctrl+D` 复制出来的新实例和原实例**位置完全相同**。  
> 两个圆点重叠时，只有层级面板里靠下的那个（更上层）能被点到，另一个像消失了一样。  
> 症状很迷惑：点哪个位置都命中同一个按钮。

**为什么用 `WBP_SelectableButton` 而不是普通按钮**：它有现成的选中态（`SetSelected` 会亮一个边框），  
不用自己做高亮。

### 2.5 第 2 步：`Event Construct` —— 把点击接进来

打开 **Graph** 面板：

1. 右键 → 搜 `Event Construct`（构造时跑一次）。
2. 从左侧变量/组件列表把 `Btn_DropPoint0` 拖进图表。
3. 从它拉出线 → 搜 **`Bind Event to On Button Clicked`**。
4. 在这个节点的 **`Event` 引脚上右键** → `Add Custom Event`，命名比如 `OnClick_DropPoint0`。
5. 在新建的 `OnClick_DropPoint0` 里 → 搜 `Preview Drop Point` → 参数填 `0`。
6. 对 `Btn_DropPoint1` 重复一遍，参数填 `1`。

最终长这样：

```text
Event Construct
   ├─ [Btn_DropPoint0].Set Normal        ← 先归零视觉，再绑事件
   ├─ [Btn_DropPoint1].Set Normal
   ├─ Bind Event to [Btn_DropPoint0].On Button Clicked
   │      Event -> OnClick_DropPoint0 -> Preview Drop Point (0)
   └─ Bind Event to [Btn_DropPoint1].On Button Clicked
          Event -> OnClick_DropPoint1 -> Preview Drop Point (1)
```

**为什么开头要有两句 `Set Normal`：**

`WBP_SelectableButton` 的视觉不是靠 `IsChecked` 之类的内置开关，而是靠 `SetNormal` / `SetSelected`  
**切换 Brush（贴图）**。Designer 里每个实例保存的 Brush 是什么，WBP 一构造出来就显示什么。

如果你复制了一个"选中态"的实例，或者资产创建时它的默认 Brush 就是选中那张图，  
那么一进页面**所有 marker 都会显示为已选中**——看起来像"全被选了"，其实是初始贴图没归零。

`Construct` 里统一 `Set Normal` 一遍，等于把初始状态钉死，不受实例默认值影响。  
这条白线要串行连（`Set Normal 0` → `Set Normal 1` → `Bind 0` → `Bind 1`），**不要并联**（见 §0）。

**为什么第 4 步要建自定义事件**：`Bind Event to` 的 `Event` 引脚要的是一个**事件**，  
不能连一个函数调用。所以在引脚上右键新建事件，把 `Preview Drop Point` 放进那个事件里。  
这是子控件绑定的标准套路，`WBP_SlimeMainMenu` 绑三个按钮用的也是它。

**为什么是 `Preview Drop Point` 不是 `Confirm Drop Point`**：点圆点只是"选"，还没确定。  
真要一步到位才用 `Confirm Drop Point`，但那样玩家点错就直接进游戏了。

### 2.6 第 3 步：`Event On Preparation Ready` —— 数量对不上就藏掉

**先判断这一步你到底需不需要做。** Designer 里的控件默认都是 `Visible`，所以：

| 你摆了几个 marker | 本关落点数 | 要做什么                       |
| ------------ | ----- | -------------------------- |
| 2            | 2     | **跳过这一步**，默认显示正好是对的        |
| 3            | 2     | 把第 3 个 `Collapsed`         |
| 2            | 3     | 少了，去加 marker + Bind + 预览分支 |

如果数量正好相等，这一段 `Less` / `Branch` 全部不用连——连了也不会错，只是白连。

**这个事件空着不会有任何问题**：`OnPreparationReady` 是 `BlueprintImplementableEvent`，  
C++ 只是通知一声，没有返回值、也不等你处理完。蓝图里不实现它，调用时就是空函数体——  
不报错、不阻塞、没有副作用。"空着"和"删掉这个事件"运行时完全一样。

**只有需要藏掉多余 marker 时，才继续往下看。**

Graph 里右键 → 搜 `Event On Preparation Ready`（参数 `DropPointCount`）。

对每个 marker 加一条：

```text
Event OnPreparationReady (DropPointCount)
   ├─ Less: A = 0, B = DropPointCount  -> Select(True=Visible, False=Collapsed) -> [Btn_DropPoint0] Set Visibility
   ├─ Less: A = 1, B = DropPointCount  -> Select(...)                          -> [Btn_DropPoint1] Set Visibility
   └─ Less: A = 2, B = DropPointCount  -> Select(...)                          -> [Btn_DropPoint2] Set Visibility（如果有）
```

读作「**第 0 个 小于 总数**」→ 这个圆点存在，就显示。

**`Less` 是小于号 `<`**（整数比较用 `Less (int)`）。  
用 `Greater` 反过来写也完全等价：`A = DropPointCount, B = 下标`，  
两者是同一件事的两面 —— `Count > i` ≡ `i < Count`。

**引脚顺序是这里唯一容易错的地方：**

| 节点        | A                | B                |
| --------- | ---------------- | ---------------- |
| `Less`    | 下标（0 / 1 / 2）    | `DropPointCount` |
| `Greater` | `DropPointCount` | 下标（0 / 1 / 2）    |

只换节点不换引脚，就会变成 `2 < 0`，永远为 false，**全部圆点被隐藏**。  
这时你会以为 `OnPreparationReady` 没触发，其实是比较写反了。

**还有一种更隐蔽的连反**：把 `DropPointCount` 接到 `A`、常数接到 `B`，写成 `DropPointCount < 2`。  
它判断的是「**落点总数少于 2 个**」，不是「第 3 个圆点是否存在」。

```text
错的：DropPointCount < 2     在只有 2 个落点时会给出正确答案，骗过人
对的：2 < DropPointCount     3 个落点的关卡才看得出区别
```

| Count | `Count < 2`（错） | `2 < Count`（对） |
| ----- | -------------- | -------------- |
| 1     | true → 显示 ✗    | false → 隐藏 ✓   |
| 2     | false → 隐藏 ✓   | false → 隐藏 ✓   |
| 3     | false → 隐藏 ✗   | true → 显示 ✓    |

只有 `Count = 2` 这一格两者答案相同，正好是"本关只有 2 个落点"的常见情况。  
所以这类错误在单关卡上测不出来，加第二关（3 个落点）才会暴露。

举例，本关配了 2 个落点（`DropPointCount = 2`）：

| marker           | 判定              | 结果        | 对不对   |
| ---------------- | --------------- | --------- | ----- |
| `Btn_DropPoint0` | `0 < 2` → true  | Visible   | ✓ 有落点 |
| `Btn_DropPoint1` | `1 < 2` → true  | Visible   | ✓ 有落点 |
| `Btn_DropPoint2` | `2 < 2` → false | Collapsed | ✓ 被藏掉 |


边界也顺带覆盖了：`DropPointCount = 0` 时三个都是 false，全部隐藏，不会留下一个点不动的圆点。

**为什么**：WBP 里可能为了将来的关卡预摆 3 个圆点，但这一关只有 2 个落点。  
不藏掉第 3 个，玩家会点到一个根本不存在的落点，然后什么都不会发生。

**为什么必须放在这个事件里**：`Construct` 执行时 C++ 还没缓存落点数量，  
那时调 `GetDropPointCount()` 读到的是 0。只有 `OnPreparationReady` 的参数是权威值。  
顺序是：`Construct` 绑点击 → `OnPreparationReady` 决定可见性。

> `Select` 节点不是 `Branch`。`Select` 按 bool 从两个值里挑一个，省掉 Branch + 两条 Set Visibility；  
> 用 `Branch` 也完全可以，只是节点更多。

**同时建议加一条自检**（下面 §2.9）。

### 2.7 第 4 步：`Event On Drop Point Previewed` —— 高亮跟着走

右键 → 搜 `Event On Drop Point Previewed`（参数 `DropPointIndex`）。

```text
Event OnDropPointPreviewed (DropPointIndex)
   ├─ Equal: A = DropPointIndex, B = 0 -> Branch
   │      True  -> [Btn_DropPoint0].Set Selected()   [Btn_DropPoint1].Set Normal()
   ├─ Equal: A = DropPointIndex, B = 1 -> Branch
   │      True  -> [Btn_DropPoint1].Set Selected()   [Btn_DropPoint0].Set Normal()
   └─ [Btn_Go].Set Is Enabled (true)
```

**为什么要手动把另一个 `Set Normal()`**：`SetSelected()` 只是给这个按钮点亮边框，  
它**不会**帮你把别的按钮熄灭。不手动清，点两个之后会两个同时亮着。

**为什么要 `Set Is Enabled(true)`**：一进来 `Btn_Go` 应该是灰的（还没选落点），  
选过之后才允许确认。这个禁用状态在 §2.8 里补。

### 2.8 第 5 步：确认按钮和返回按钮

```text
[Btn_Go] On Clicked   -> Confirm Previewed Drop Point
[Btn_Back] On Clicked -> Request Back To Main Menu
```

同时建议在 `Event Construct` 末尾加一句：

```text
[Btn_Go].Set Is Enabled (false)
```

> 也可以更省事：Designer 里选中 `Btn_Go` → Details → `Interaction` → 取消勾选 `Is Enabled`。  
> 默认值就是禁用，连 `Construct` 里那一句都不用写。

**为什么 `Btn_Go` 用裸 Button**：它不需要选中态，就是个一次性动作。  
所以它**有** `On Clicked`，直接用，不用 `Bind Event to`。

**为什么返回用 `Request Back To Main Menu` 而不是自己找子系统**：  
`USlimeUISubsystem::Get()` 是普通 C++ 静态函数，没标 `UFUNCTION`，蓝图里搜不到。  
所以转发放在落点页父类上。

### 2.9 第 6 步：自检清单

做完对着勾：

```text
[ ] WBP 里 marker 数量 == DA_SpawnLayout::DropPoints 的元素个数
[ ] 每个 marker 都在 Event Construct 里 Bind 了 On Button Clicked
[ ] 每个 Bind 的事件里，参数是硬编码的下标 0 / 1 / ...
[ ] OnPreparationReady 里有多余 marker 的 Collapsed 逻辑
[ ] OnDropPointPreviewed 里互斥切换（选中一个、另一个 SetNormal）
[ ] Btn_Go 连的是 Confirm Previewed Drop Point
[ ] Btn_Back 连的是 Request Back To Main Menu
[ ] Btn_Go 一进来是 disabled
```

**可选但强烈建议**——在 `OnPreparationReady` 里加一条数量比对：

```text
Branch: DropPointCount == <你实际摆的 marker 数>
   False -> Print String: "落点数量不匹配：DA 有 X 个，WBP 摆了 Y 个"
```

出现"点第 3 个没反应"时，这条字符串能省你半小时。

### 2.10 这份现在是什么状态

**已经接好了，本次改造不用动。** 依据：`UI实现/WBP_Blueprint_Wiring_Guide.md` §1.6 的接线盘点，  
`PreviewDropPoint` / `ConfirmPreviewedDropPoint` / `OnPreparationReady` / `OnDropPointPreviewed`  
四个都在用，`SetSelected` / `SetNormal` 也在调。

上面 6 步是**从零重建**或**新建一张关卡专用 WBP** 时照做的顺序。

### 2.11 每关一张 WBP 时怎么做

```text
WBP_SlimePreparation           -> CityPlaza 用，保持不动
WBP_SlimePreparation_Sandbox   -> 复制一份，换俯瞰图，按新落点数摆 marker
```

复制后如果落点数变了，**三样都要补**（漏一样那个 marker 就是死的）：

```text
1. 新增 Btn_DropPoint2 实例
2. Event Construct 里多一条 Bind Event to -> Preview Drop Point (2)
3. OnDropPointPreviewed 里多一个分支：Index == 2 -> Btn_DropPoint2.SetSelected + 其余 SetNormal
```

C++ 侧通过关卡定义里的 `Preparation Widget Class` 决定加载哪一份。

---

## 3. `WBP_SlimeLevelSelect`（关卡选择页）

### 3.1 这张页要达成什么

```text
玩家看到 3~4 个关卡节点 -> 点一个（选中）-> 点"进入"（加载地图）
```

和落点页的区别：**点关卡节点只是选中，不会直接进关**。进关是单独一个按钮的动作。

节点同样是 `WBP_SelectableButton` 的实例，命名 `Btn_Level0` / `Btn_Level1` / `Btn_Level2`  
（槽位 3 目前是预留，没有按钮）。

### 3.2 第 1 步：`Event Construct` —— 先归零视觉，再绑定点击

```text
Event Construct
   ├─ [Btn_Level0].Set Selected      ← 默认选中第 0 关（和 C++ 的初值 0 一致）
   ├─ [Btn_Level1].Set Normal
   ├─ [Btn_Level2].Set Normal
   ├─ Bind Event to [Btn_Level0].On Button Clicked -> Select Level (0)
   ├─ Bind Event to [Btn_Level1].On Button Clicked -> Select Level (1)
   └─ Bind Event to [Btn_Level2].On Button Clicked -> Select Level (2)
```

**为什么一开始要设选中态**：C++ 里 `SelectedLevelIndex` 默认是 `0`，但**进页面时不会主动发**  
`OnLevelSelectionChanged`（那只在玩家点了之后发）。所以第一屏的选中态得 WBP 自己刷一遍，  
否则会出现"一个都没选中"或者"好几个都选中"——取决于实例的默认 Brush。

做法和 §2.5 完全一样：拖出节点 → `Bind Event to On Button Clicked` → 在 `Event` 引脚右键建自定义事件 →  
在事件里调 `Select Level(i)`。

**为什么是 `Select Level` 不是 `Enter Selected Level`**：`SelectLevel` 只更新"选中了哪一关"，  
还会发一个 `OnLevelSelectionChanged` 让你们更新界面。`EnterSelectedLevel` 才是真的加载地图。

### 3.3 第 2 步：`Event On Level Selection Changed` —— 切换选中态

**这一步现在是空的**，所以点关卡不会变色。

```text
Event OnLevelSelectionChanged (LevelIndex, bAvailable)
   ├─ LevelIndex == 0 -> [Btn_Level0].Set Selected()   其余 Set Normal()
   ├─ LevelIndex == 1 -> [Btn_Level1].Set Selected()   其余 Set Normal()
   ├─ LevelIndex == 2 -> [Btn_Level2].Set Selected()   其余 Set Normal()
   ├─ bAvailable == false -> 该节点 Set Locked()，并显示"未开放"文本
   └─ [Btn_Enter].Set Is Enabled (bAvailable)
```

**为什么事件里同时给 `bAvailable`**：省得你再去问一次 `IsLevelAvailable`。  
`false` 表示这一关没配地图，UI 该显示为锁定、进入按钮该灰掉。

### 3.4 第 3 步：进入和返回

```text
[Btn_Enter] On Clicked -> Enter Selected Level
[Btn_Back]  On Clicked -> Return To Main Menu
```

**为什么返回现在变快了**：以前 `ReturnToMainMenu` 会**重载整张关卡地图**（黑屏一下），  
现在改成纯切屏。如果你 WBP 里有"返回时手动隐藏自己"的补救逻辑，可以删掉。

### 3.5 第 4 步（可选）：让标题和锁定态读数据

以前关卡名写死在 WBP 里。现在可以从关卡资产读：

```text
Event Construct（接在上面那三个 Bind 之后）
   ├─ [Text_Level0].Set Text (Get Level Display Name (0))
   ├─ [Text_Level1].Set Text (Get Level Display Name (1))
   ├─ [Text_Level2].Set Text (Get Level Display Name (2))
   └─ [Mask_Lock0].Set Visibility: Is Level Available (0) ? Collapsed : HitTestInvisible
      （其余同理）
```

**为什么值得做**：关卡名、开放状态改成策划在资产里填，不用改 WBP、不用重新编译。  
不接也能跑，只是文案写死。

**一个必须知道的区别**：

| 函数                          | 返回          | 什么时候用                            |
| --------------------------- | ----------- | -------------------------------- |
| `GetLevelCount()`           | 固定 **4**    | 你已经画了固定几个节点，循环它们                 |
| `GetConfiguredLevelCount()` | 实际配了几关（0~4） | 用 `ListView` 之类的**动态生成**节点时当循环上限 |

固定节点**不要**换成 `GetConfiguredLevelCount()`，否则未配置的槽位会直接消失，  
和"锁定但可见"的设计冲突。

### 3.6 第 5 步：自检清单

```text
[ ] 三个关卡节点都 Bind 了 On Button Clicked
[ ] Bind 里调的是 Select Level(i)，不是 Enter Selected Level
[ ] OnLevelSelectionChanged 里做了互斥的 Set Selected / Set Normal
[ ] bAvailable == false 的节点显示为锁定态
[ ] Btn_Enter 连的是 Enter Selected Level
[ ] Btn_Back 连的是 Return To Main Menu
```

---

## 4. `WBP_SlimeMainMenu`（主菜单）

### 4.1 结论

**不用改。** 三个按钮的连线保持原样：

```text
Btn_Start    On Button Clicked -> Start Game
Btn_Settings On Button Clicked -> Open Settings
Btn_Quit     On Button Clicked -> Quit Game
```

### 4.2 唯一的行为变化

```text
以前：点开始游戏 -> 直接加载 CityPlaza
现在：点开始游戏 -> 打开关卡选择页 -> 选关 -> 加载对应地图
```

WBP 不需要动。

### 4.3 一个不要做的事

**不要把"开始游戏"改接到自己 `CreateWidget` 关卡选择页。**  
关卡选择页由 `USlimeUISubsystem` 常驻创建，`StartGame()` 已经处理好了。  
自己创建会导致子系统里的那个实例拿不到显示，表现是"点了开始游戏没反应"。

### 4.4 启动时会看到什么

```text
启动 / PIE Play
    -> 主菜单
    -> 点"开始游戏"
    -> 关卡选择页
    -> 选一关 + 点"进入"
    -> 落点选择页（该关配置的 WBP）
    -> 点 marker + 点确认
    -> 投放演出 -> HUD -> 结算页
```

### 4.5 五种进入方式，第一屏分别是什么

| 场景                   | 第一屏                |
| -------------------- | ------------------ |
| 启动游戏 / PIE Play      | **主菜单**            |
| 主菜单 -> 选关 -> 进入      | 落点选择页              |
| 结算页/暂停页 -> **重试**    | 直接投放演出（**不经过**落点页） |
| 结算页/暂停页 -> **重新选落点** | 落点选择页              |
| 游戏中 -> 回主菜单          | **主菜单**            |

如果实际表现和这张表对不上，先去查 §9。

### 4.6 调试开关：跳过主菜单

```text
Project Settings -> Game -> Slime War -> UI|Editor -> Skip Main Menu In Pie
```

| 状态         | PIE 启动后     |
| ---------- | ----------- |
| **不勾（默认）** | 主菜单，和打包后一致  |
| 勾上         | 直接进当前地图的落点页 |

**只在调战斗或落点页时勾**，测菜单流程记得关回去——勾着的时候你测不到"开始游戏"这条链路。  
勾着也**不会**让"回主菜单"失效（代码里有独立的标志，优先级高于这个快捷方式）。

> 只影响 PIE，打包后不生效。改完重开 PIE 即可。

---

## 5. `WBP_SlimePause` / `WBP_SlimeResult`（暂停与结算）

### 5.1 暂停页

```text
Btn_Resume  -> Resume()
Btn_Retry   -> Request Retry()
Btn_Reselect-> Request Reselect Drop Point()
Btn_Quit    -> Request Quit()
```

已接好，不用动。

### 5.2 结算页

**这页目前基本是空的**（只有一个空的 `OnResultReady`，控件和按钮都没接）。

```text
Event On Result Ready (Result: FSlimeRunResult)
    -> 填充分数 / 目标分 / 击杀数 / 是否通过

Btn_Retry    -> Request Retry()
Btn_Reselect -> Request Reselect Drop Point()
Btn_Quit     -> Request Quit()
```

### 5.3 两个语义变了，文案要跟着改

**（1）"退出"不再是退出游戏。**

```text
暂停页 / 结算页 点"退出"  ->  回主菜单
主菜单 点"退出游戏"        ->  才真正关闭游戏
```

按钮上写"退出游戏"的话建议改成"返回主菜单"，否则玩家以为按了没反应。

**（2）"重新选落点"现在能正确回到落点页了。** 以前会跳回主菜单，现在修好了，文案不用改。

---

## 6. `WBP_SlimeHUD` / `WBP_SlimeSettings`

**都不用改。** 本次只删了一行 HUD 的调试日志，接口一个没动。

`WBP_SlimeHUD` 里 `OnScoreUpdated` 等事件签名保持原样，不需要重连。

---

## 7. 数据资产配置（一次性，之后加关卡照做）

> 用**界面操作**，不需要手写 ini。保存后 UE 自动写回 `Config/DefaultGame.ini`。

### 7.1 先确认三个前置资产

```text
Content/_SlimeWar/Core/Data/DA_RunConfig      已存在
Content/_SlimeWar/Core/Data/DA_SpawnLayout    已存在
Content/_SlimeWar/UI/WBP_SlimePreparation     已存在
```

CityPlaza 可以直接复用这三个。

### 7.2 新建关卡定义资产

1. 在 `Content/_SlimeWar/Core/Data/` 下新建目录 `Levels`。
2. 右键 → `Miscellaneous` → `Data Asset`。
3. 类选择框搜 `SlimeLevelDefinition`，命名 `DA_Level_CityPlaza`。
4. 填两组字段：

| 字段                         | 填什么                             |
| -------------------------- | ------------------------------- |
| `Level Id`                 | `CityPlaza`（**定了不要改**，会话靠它认关卡）  |
| `Display Name`             | `城市广场`（选关页显示的名字）                |
| `Map Asset`                | `L_Whitebox_CityPlaza`          |
| `Run Config`               | `DA_RunConfig`                  |
| `Preparation Widget Class` | `WBP_SlimePreparation`（留空则回退全局） |
| `Deploy Sequences`         | 每个落点一条；留空则回退全局                  |
| `Result Sequence`          | 结算镜头；留空则回退全局                    |

### 7.3 在项目设置里登记

1. `Edit` → `Project Settings`。
2. 左侧 `Game` 分类下找 **`Slime War`**。
3. 展开 `Levels` 数组。
4. 点 `+`，选 `DA_Level_CityPlaza`。
5. 关闭面板即保存（`DeveloperSettings` 会直接写回 ini）。
6. **重启编辑器**（`config=Game` 不热加载）。

**顺序就是槽位顺序**：数组第 0 项 = 槽位 0 = 界面上第一个节点。

### 7.4 加第二张图

重复 7.2 和 7.3：建 `DA_Level_Sandbox`（`Map Asset` 选 `L_Sandbox_OnePoint`），  
`Levels` 里再点一次 `+`。**不需要改代码、不需要编译。**

### 7.5 如果什么都不配会怎样

`Levels` 为空时有兜底：槽位 0 显示 `Current map`，点了重载当前地图。  
游戏不会进不去，但只有一关可选。所以这一步是"要不要多关"的选择，不是"能不能跑"的前提。

---

## 8. 以后可以优化的点

> 两条**都需要改 C++**，蓝图侧不用提前配合。列在这里是为了记住"为什么现在不做"。

### 8.1 落点数据从"下标对齐"升级为"带名字"

**现状**：`USlimeSpawnLayout::DropPoints` 是 `TArray<FVector>`，只有坐标没有名字。  
WBP 里的 marker 和数据**只靠下标对齐**——第 3 个圆点就是数组里的第 3 个坐标。

**问题**：数组换顺序、或 WBP 中间插了一个 marker，两边就错位，界面上完全看不出异常：  
玩家点"东侧"，人送到"西侧"。`GetDropPointCount()` 只能发现**数量**不一致，发现不了顺序不同。

**升级后**：每个落点变成结构体 `Location` / `DisplayName` / `Facing`，  
蓝图侧多一个 `GetDropPointDisplayName(int32)` 可以接到 marker 标题上。

**代价**：数组元素类型变了，`DA_SpawnLayout` 里已填的坐标会被 UE 丢掉，要重填。

**什么时候值得做**：出现"数量对得上但落点错位"，或者设计要让玩家看见落点名。

### 8.2 独立菜单地图（`MenuMap`）

**现状**：`ReturnToMainMenu()` 是"重载当前关卡地图，再把主菜单屏显示出来"。

**问题**：回主菜单的加载时间 = **这一整关的加载时间**。关卡变大后会黑屏几秒，体感像卡死。

**升级后**：加一张专门放主菜单的地图，回菜单秒进。

**代价**：多维护一张菜单地图；主菜单相关 WBP 要在两张图里都能工作。

**什么时候值得做**：单关加载体感超过 5~10 秒，或玩家反馈"退出要等很久"。

---

## 9. 排错速查

| 现象                   | 先看这里                                                                                                                |
| -------------------- | ------------------------------------------------------------------------------------------------------------------- |
| 点"开始游戏"没反应           | 关卡选择页是不是被自己 `CreateWidget` 了？应该由 `StartGame()` 走子系统                                                                 |
| 选关页节点全灰              | `Levels` 是不是空的？或者 `Map Asset` 没填？                                                                                   |
| 点关卡节点不变色             | `OnLevelSelectionChanged` 没实现（这是已知缺口，见 §3.3）                                                                        |
| marker 连高亮都不动        | `Event Construct` 里的 `Bind Event to On Button Clicked` 漏了或绑错实例（marker 是 `WBP_SelectableButton`，**没有** `On Clicked`） |
| 点任何圆点都命中同一个按钮        | 两个实例**位置重叠**（`Ctrl+D` 复制后没改 Position）。层级靠下的在上面，优先命中；另一个像消失了一样                                                       |
| 圆点怎么调 `Padding` 都对不齐 | 父面板是 `Overlay`，它的 Slot 只有 `Padding`/`Alignment`，没有 `Position`。换成 `Canvas Panel`（见 §2.4）                             |
| 某个圆点点了完全没反应，另一个正常    | 该实例的 `Visibility` 是 `SelfHitTestInvisible`，应该是 `Visible`（在 Widget Reflector / Inspector 里能直接看到）                     |
| 按一个圆点，两个圆点的事件都触发     | 两个 `Bind Event to` 的 `Target` 都指向了同一个实例（复制节点后没改 Target）。悬停 `Target` 引脚能看到绑的是谁                                       |
| 点哪个圆点都只触发同一个事件       | 两个 `Bind` 的 `Event` 引脚连到了同一个自定义事件，`Drop Point Index` 也就永远是同一个值                                                      |
| 点了某个落点没反应            | marker 数量 > `DropPoints` 元素数。越界点击会打 Warning 并忽略                                                                     |
| 落点页看不到圆点             | `OnPreparationReady` 没实现，或没按传入数量显示                                                                                  |
| 俯瞰图是上一关的             | 两关用了同一张 WBP，`Preparation Widget Class` 要指向各自的                                                                       |
| 部署演出时长不对             | 关卡 `Run Config` 里的 `Deploy Duration`，不是全局那份                                                                         |
| 按暂停键没反应              | 跟 WBP 无关：`BP_ThirdPersonCharacter` 的 `PauseAction` 是否指向 `IA_Pause`                                                  |
| 暂停页"退出"看起来没反应        | 它现在回主菜单，不是关游戏；检查文案                                                                                                  |

---

## 10. 一页速查：谁调谁

```text
WBP_SlimeMainMenu
    StartGame()      -> 子系统显示选关页
    OpenSettings()   -> 临时创建设置页
    QuitGame()       -> 退出游戏

WBP_SlimeLevelSelect
    SelectLevel(i)         -> 更新选中 + 触发 OnLevelSelectionChanged
    EnterSelectedLevel()   -> 记录关卡 ID + 加载地图
    ReturnToMainMenu()     -> 纯切屏回主菜单
    查询：GetLevelDisplayName(i) / IsLevelAvailable(i) / GetConfiguredLevelCount()

WBP_SlimePreparation
    marker 点击 -> PreviewDropPoint(i) -> OnDropPointPreviewed(i)
    Btn_Go      -> ConfirmPreviewedDropPoint()
    Btn_Back    -> RequestBackToMainMenu()
    查询：GetDropPointCount() / GetLevelId()

WBP_SlimePause / WBP_SlimeResult
    Resume() / RequestRetry() / RequestReselectDropPoint() / RequestQuit()
    退出 = 回主菜单
```
