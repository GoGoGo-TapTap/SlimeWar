# R3 资源行为语义自动测试

- 日期：2026-10-19
- 状态：自动 soak 通过，手动 Checklist 待执行

## 测试

```text
地图：L_Ecology_DoublePoint
命令：SlimeRunStart,SlimeEcologyResourceSoakTest 20
日志：Saved/Logs/R3-Resource.log
截图：HighresScreenshot00099.png
```

## 账本结果

```text
resource OK: 0 stock + 12 carried + 0 ground + 8 delivered = 18 initial + 2 regen
mass OK: 20 initial + 8 delivered = 28 live + 0 removed
```

站点：

- A：12 存活、14 体量、4 交付、2 核心。
- B：12 存活、14 体量、4 交付、2 核心。

## 已实现行为

- 初始采集休息随机 0.1–1.2 秒。
- 资源目标保留 2 秒。
- 矿点拥挤代价 1.8 米/可见同族。
- 拾取动作距离 100 cm、结算容差保留 110 cm。
- 有资源记忆时探索速度 90 cm/s。

## 结论

持续采集、交付、1:1 增员和账本守恒通过。目标保留和拥挤选择的个体级表现仍需手动观察。
