# M7 双点生态关卡 · 编辑器与手动验收

> 自动资源循环、交付、再生、携货掉落、150 秒 TimeUp 和 Extinction 已通过。下面需要真实键鼠操作确认关卡流程与观感。

## 1. 打开工程

1. 关闭 Unreal Editor 后重新打开 `SlimeWar.uproject`。
2. 确认编译模块成功。
3. 从主菜单进入关卡选择，应出现 `Double Point Warehouse`。
4. 进入关卡后选择 D1 或 D2。

检查：

- [ ] 主菜单和关卡选择正常。
- [ ] 双点关卡可从 LevelSelect 进入。
- [ ] D1、D2 两个落点均可投放。
- [ ] 玩家落点没有卡墙或悬空。

## 2. 初始场景

控制台：

```text
SlimeRunTimeScale 0
SlimeEcologyResourceStatus
```

- [ ] A/B 各有 10 只体量 1。
- [ ] HUD 显示 A/B 站点卡片。
- [ ] WestNear 和 EastNear 库存 4。
- [ ] SouthShared 和 NorthShared 初始库存 5，之后恢复到 6。
- [ ] 地面尺寸和墙体布局符合 44×30 双点仓库。
- [ ] 四个矿点均可到达。

## 3. 采集与交付

开始运行：

```text
SlimeRunTimeScale 1
```

- [ ] 普通个体主动前往矿点。
- [ ] 到达矿点后停留约 2 秒。
- [ ] 矿点库存减少 1，个体携带 1 份货物。
- [ ] 携货个体返回同阵营站点。
- [ ] 到达交付半径后交付。
- [ ] 站点交付数 +1。
- [ ] 该阵营只新增 1 只体量 1。

可随时执行：

```text
SlimeEcologyResourceStatus
```

- [ ] 资源账本保持平衡。
- [ ] 体量账本保持平衡。

## 4. 矿点再生

1. 等待共享矿点被采集。
2. 观察 5 秒。

- [ ] SouthShared / NorthShared 每 5 秒恢复 1。
- [ ] 恢复到 6 后停止。
- [ ] WestNear / EastNear 不恢复。

## 5. 携货掉落

执行：

```text
SlimeEcologyCargoDropTest
```

- [ ] 出现一只携带 1 份货物的个体。
- [ ] 个体死亡后地面出现货物。
- [ ] 日志出现 `EcologyCargo: dropped 1`。
- [ ] 其他个体可拾取该货物。
- [ ] 地面货物显示独立的小型模型，不再只有日志。
- [ ] 携货个体身上/头顶显示对应货物模型。
- [ ] `ASlimeCargoVisual::CargoMeshAsset` 可在 Blueprint/DataAsset 中替换后续正式模型。

## 6. 交付专项

执行：

```text
SlimeEcologyDeliveryTest
```

- [ ] 1 秒内站点交付数增加 1。
- [ ] 携货清零。
- [ ] 新增一只体量 1。

## 7. 完整 150 秒回合

执行：

```text
SlimeRunTimeScale 10
```

- [ ] TIME 从 150 正常递减。
- [ ] 目标分达到 200 时回合不会提前结束。
- [ ] 时间归零后进入 Result/Ended。
- [ ] 运行中站点和资源 HUD 持续更新。
- [ ] 结算页可正常进入。

## 8. 灭绝

执行：

```text
SlimeClearEnemies
```

- [ ] 无存活、无待生成、无地面货物后立即结束。
- [ ] 结束原因为 `Extinction`，不是等待 150 秒。
- [ ] 调试清理后体量账本可解释。

## 9. 视觉与导航

- [ ] A/B 站点颜色和标签可辨认。
- [ ] 四个矿点标签和库存可辨认。
- [ ] 个体能绕过柱子和分隔墙前往矿点。
- [ ] 生成增员不会卡在墙内或与已有个体重叠。
- [ ] 相机遇墙收缩，不穿墙。
- [ ] 中性核心爆炸后，相邻中性核心受到伤害但不会全部递归爆炸。

## 10. 手动验收结论

- [x] 全部通过
- [ ] 存在问题，描述：

```text

```

用户已于 2026-10-19 完成手动验收，M7 标记为 `Passed`，下一模块为 M8。
