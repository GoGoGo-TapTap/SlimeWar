# SlimeWar 版本控制与协作规范

> 适用对象：所有参与 SlimeWar 开发的成员（程序 / 策划 / 美术）。
> 目标：**不让任何一个 .uasset / .umap 被静默覆盖，不让任何一次推送因为本地环境差异产生整文件假 diff。**
> 仓库托管：**GitHub**（这项很关键，见第 4 节）。

---

## 0. TL;DR（老手只看这段）

```powershell
git lfs install                                  # 每台机器一次，必做
git clone <repo-url>
cd SlimeWar
git config core.autocrlf input                   # 或者不设，靠 .gitattributes 的 eol=lf

# 日常：改资产前先在群里认领，改完立刻推
git pull --rebase
git add Content
git commit -m "feat(art): 更新角色移动动画"
git push
```

三条铁律：

1. **改 `.uasset` / `.umap` 前先在群里说一声**（GitHub 不支持锁，只能靠这个）。
2. **不要在不拉取的情况下改同一个资产**。`.uasset` 是二进制序列化数据，冲突无法自动合并，解决成本极高。
3. **`Content/` / `Config/` / `Source/` / `*.uproject` 必须入库；`Binaries/` / `Intermediate/` / `Saved/` / `DerivedDataCache/` / `.vs/` 绝不入库。**

---

## 1. 新人 onboarding（按顺序做，别跳）

### 1.1 装工具

| 工具 | 版本要求 | 说明 |
|---|---|---|
| Git for Windows | 任意较新版本 | 自带 Git Credential Manager |
| **Git LFS** | ≥ 3.0（推荐 3.7+） | **必须装。不装会导致资产变成几百字节的文本指针** |
| Unreal Engine | 5.5（见 `SlimeWar.uproject` 的 `EngineAssociation`） | |
| Visual Studio 2022 | 17.8+，含 `Microsoft.VisualStudio.Workload.NativeGame` | 组件清单见仓库根的 `.vsconfig`，VS 安装器可直接导入 |

### 1.2 前置检查

```powershell
git --version          # 例：git version 2.47.0.windows.1
git lfs version        # 例：git-lfs/3.7.1 (GitHub; windows amd64)
```

`git lfs version` 报 "not a git command" 就说明 LFS 没装好，**先解决这个再 clone**。

### 1.3 安装 LFS 并 clone

```powershell
git lfs install                       # 写入全局 filter + 当前仓库 hook
git clone <repo-url>
cd SlimeWar
```

### 1.4 验证 clone 是否正确（重要）

```powershell
git lfs ls-files | Measure-Object -Line     # 应该是一个较大的数字，不是 0
```

**如果结果是 0，或者 `Content/` 下的 `.uasset` 只有 100 多字节**，说明 clone 时 LFS 没工作：

```powershell
git lfs install
git lfs pull
```

### 1.5 生成工程文件并首次编译

右键 `SlimeWar.uproject` → *Generate Visual Studio project files* → 打开 `SlimeWar.sln` → 编译 `Development Editor` 配置。
（`SlimeWar.sln` 不入库，每人本机生成。）

### 1.6 设置行尾模式

```powershell
git config core.autocrlf input
```

仓库的 `.gitattributes` 里每个文本类型都写死了 `eol=lf` / `eol=crlf`，这条只是兜底，防止未列出的文本类型被 Windows 默认的 `autocrlf=true` 翻来翻去。

---

## 2. 仓库结构：什么该入库，什么不该

### 2.1 必须入库

| 路径 | 原因 |
|---|---|
| `Content/**` | 所有 UE 资产，游戏本体 |
| `Config/**/*.ini` | 输入映射、渲染、GameplayTag 等共享设置 |
| `Source/**` | C++ 源码 |
| `Plugins/**/Content/`、`Plugins/**/*.uplugin`、`Plugins/**/Source/` | Lyra 系插件与 Wwise 的资产和源码，**游戏运行依赖，缺了直接挂** |
| `SlimeWar.uproject` | 工程定义（模块、插件启用列表） |
| `.gitignore` / `.gitattributes` / `.ignore` / `.vsconfig` | 协作基础设施本身 |
| `Docs/**` | 本目录 |

### 2.2 绝不入库（已由 `.gitignore` 覆盖）

`Binaries/` `Build/` `Intermediate/` `Saved/` `DerivedDataCache/` `.vs/` `.idea/` `.vscode/` `*.sln` `Plugins/*/{Binaries,Intermediate,Saved,DerivedDataCache}/`，以及 `Thumbs.db` `Desktop.ini` `.DS_Store`。

### 2.3 常见误区

- ❌ **"把 `Config/` 也忽略掉吧，里面都是 ini"** —— 不行。`Config/DefaultInput.ini`、`DefaultGame.ini` 是团队共享配置，忽略掉所有人拿到的都是 UE 默认值，输入映射直接丢失。只忽略 `Saved/Config/`（已被 `Saved/` 覆盖）。
- ❌ **"把 `Plugins/` 整个忽略，队友自己下"** —— 不行，`Plugins/*/Content/` 里有游戏依赖的资产。
- ⚠️ `SlimeWar.uproject` 的 `EngineAssociation` 字段包含本机引擎标识。如果队友装了不同版本/不同来源的引擎，这个字段会产生小冲突。**解决方式是手动改回团队统一版本，不要提交自己的本机值。**

---

## 3. Git LFS 与 `.gitattributes` 规则说明

`Content/` 下的资产由 LFS 存储（GitHub 单文件 > 100 MB 会被拒，LFS 是唯一出路）。规则**按扩展名**而不是按路径，写在仓库根的 `.gitattributes`。

### 3.1 走 LFS 的类型

- **UE 资产**：`uasset` `umap` `ubulk` `uexp` `uptnl` `upk` `udk` `t3d`
- **DCC 源文件**：`fbx` `obj` `blend` `max` `mb` `ma` `abc`
- **图片**：`psd` `tga` `png` `jpg` `jpeg` `gif` `bmp` `tif` `tiff` `exr` `hdr` `svg`
- **音视频**：`wav` `wem` `bnk` `ogg` `mp3` `flac` `aiff` `mp4` `mov`

每个条目都是 `filter=lfs diff=lfs merge=lfs -text`，含义：

- `filter=lfs`：内容存 LFS，仓库里只放指针
- `-text`：禁止任何行尾转换（二进制必须如此，否则文件会被损坏）
- `diff=lfs`：`git diff` 显示 "LFS object <oid>" 而不是尝试当文本 diff
- `merge=lfs`：**冲突时不会尝试逐行合并**，而是保留两边的对象，让你显式二选一

### 3.2 不走 LFS 的文本类型

`ini` `uproject` `uplugin` `json` `cs` `h` `hpp` `cpp` `c` `txt` `md` `sh` 显式声明 `text eol=lf`；
`bat` `cmd` `ps1` 声明 `text eol=crlf`（Windows 脚本必须 CRLF）。
顶部 `* text=auto eol=lf` 是兜底规则。

### 3.3 新增二进制文件类型时

如果你要往 `Content/` 里引入一种当前规则没覆盖的扩展名（例如 `.USD`、`.vdb`、某种中间件格式），**先在 `.gitattributes` 里加一行再提交**：

```
*.usd filter=lfs diff=lfs merge=lfs -text
```

**顺序不能反。** LFS 的 filter 只在 `git add` 那一刻生效：先 `git add` 再补规则，文件已经以完整二进制写进 Git 对象库和永久历史，事后只能 `git lfs migrate import --include="*.usd"` 重写历史（全员必须重新 clone）。

### 3.4 GitHub LFS 配额（要知道会花钱）

GitHub 免费额度的 LFS 存储和月流量都很小，UE 项目很容易超。超了之后 push 会被拒。
在 GitHub 仓库页 **Settings → Billing** 里能看到用量；必要时购买 data pack。
**不要把 `Binaries/`、`DerivedDataCache/`、`Saved/` 入库**——这是最常见的额度杀手。

---

## 4. ⚠️ GitHub 不支持文件锁：这是本项目的最大协作约束

### 4.1 结论

| 项 | 状态 |
|---|---|
| Git LFS 存储（大文件） | ✅ 支持 |
| **Git LFS 文件锁（Locking API）** | ❌ **不支持** |
| `.gitattributes` 里的 `lockable` 属性 | ⚠️ 在 GitHub 上**空转**，不报错但不起作用 |

`.gitattributes` 里 `*.uasset` / `*.umap` 保留了 `lockable`，这是**有意为之**：

1. 在 GitHub 上它无害——服务端不支持 Locking 时，git-lfs 客户端不会启用锁定检查，push 不会因此失败。
2. 它把"这些文件应该被独占编辑"这个**意图**固化在仓库里，将来团队搬到 GitLab / 自建 Gitea / GitHub Enterprise 时，锁立刻可用，无需迁移。
3. 它也是给新人的一个提示：看到 `lockable` 就该意识到"这文件不能两个人同时改"。

**但绝对不要开启 `lfs.locksverify`**：

```powershell
# 不要执行这条！
git config --global lfs.locksverify true
```

开启后 git-lfs 会在每次 push 时校验锁定状态，而 GitHub 在收到 Locking API 请求时返回 404，**结果是每次 push 都报错**，而不是安静降级。同理不要开 `lfs.setlockablereadonly`。

如果之前的机器上已经开过，撤销：

```powershell
git config --global --unset lfs.locksverify
git config --global --unset lfs.setlockablereadonly
```

### 4.2 想确认？一条命令验证

```powershell
git lfs lock Content/SomeAsset.uasset
```

- 返回锁信息（含 owner、locked_at）→ 服务端支持，可按 4.3 起用锁定流程
- 返回 `Not implemented` 或 404 → 不支持，本项目的现状，按 4.4 的约定走
- 无论结果如何，用完记得 `git lfs unlock Content/SomeAsset.uasset`

### 4.3 如果将来换了支持锁的托管（GitLab / Gitea / GHE）

```powershell
git config --global lfs.locksverify true
git config --global lfs.setlockablereadonly true   # 未锁定的 lockable 文件本地置只读

git lfs lock Content/Characters/Hero.uasset        # 编辑前
# ... 编辑、提交、推送 ...
git lfs unlock Content/Characters/Hero.uasset      # 编辑后
git lfs locks                                      # 查看谁锁着什么
```

### 4.4 GitHub 现状下的替代方案：约定 + 检测

既然没有强制锁，就得靠**流程**。这是本项目协作规范里最重要的部分。

**编辑资产前（强制）**

1. `git pull --rebase` —— 保证自己基于最新版本。
2. 在团队群里发一条认领消息：`认领 Content/Characters/Hero.uasset，预计 30 分钟`。
3. 看到别人认领了同一个文件，**等**，或者找对方商量分工。
4. 通过 **GitHub Issues** 也可以：给"资产认领"打一个 label，把文件名写进 issue，比群消息更可追溯、跨时区也不会丢。

**编辑资产后（强制）**

1. **尽快提交并推送**，不要让修改在本地过夜。修改在本地放越久，撞车概率越高。
2. 一次提交只动一组相关资产，不要攒一个"改了 200 个文件"的巨型提交——那会让冲突排查变得不可能。

**推荐的工作方式：切片**

`.umap` 是冲突重灾区。TPS 项目里两人同时往同一张关卡拖东西，冲突几乎无法人工解决。所以：

- 关卡尽量**一人负责一张**，或者按 `Levels/` 子目录切分职责（光照 / 玩法 / 美术分不同的子关卡）。
- 用 **World Partition + 外部 Actor（`__ExternalActors__`）** 时，一个 actor 一个文件，天然降低冲突面积。如果项目还没启用，值得认真考虑。
- 大改关卡前，先 `git pull`，改完立刻推，避免长时间分歧。

### 4.5 万一已经撞车了怎么办

`.uasset` 冲突的表现是 `git status` 显示 `both modified`，文件里是两段 LFS 指针 + 冲突标记，**UE 打不开**。

```powershell
git status                                  # 确认哪些文件冲突
git lfs checkout                            # 先把 LFS 指针对应的真实内容取回工作区

# 二选一：保留自己的 or 保留对方的
git checkout --ours   Content/Characters/Hero.uasset
git checkout --theirs Content/Characters/Hero.uasset

# 或者更常见的做法：放弃本地改动，重新基于远端做
git checkout --theirs Content/Characters/Hero.uasset
git add Content/Characters/Hero.uasset
git commit
```

**关键：`--ours` / `--theirs` 在 rebase 和 merge 下含义相反**（rebase 时 `ours` 是远端、`theirs` 是你的提交），选之前先 `git status` 看清当前处于哪个操作中。拿不准就：

```powershell
git merge --abort        # 或 git rebase --abort
git pull --rebase
```

然后在群里同步一下，**一定要确认被丢弃的那一版是否能从对方机器或之前的提交里找回**（`git reflog` 通常能救回来）。

---

## 5. 日常协作流程

### 5.1 标准循环

```powershell
git pull --rebase                    # 先同步
# ... 在 UE 编辑器里工作，注意及时 Saved ...
git status                           # 确认改动符合预期
git add <具体文件>                    # 尽量别用 git add -A
git commit -m "feat(combat): 增加换弹动画状态机"
git push
```

### 5.2 提交信息约定

用 `<type>(<scope>): <描述>`：

| type | 用途 |
|---|---|
| `feat` | 新功能 / 新资产 |
| `fix` | 修 bug |
| `art` | 纯美术资产 |
| `refactor` | 重构 |
| `chore` | 工具、配置、忽略规则 |
| `docs` | 文档 |

例：`art(environment): 导入废弃工厂场景模块化资产`、`fix(input): 修正手柄右摇杆灵敏度曲线`。

### 5.3 分支策略（小队建议）

- `main` 始终保持可开、可编、可跑。
- 每个人一个短命分支：`feat/aim-down-sights`、`art/level-industrial`。
- **资产类改动尽量直接提交到 `main`**（短平快），源码/系统类改动走分支 + PR。原因：分支上的 `.uasset` 会在合并时造成二选一，分支活得越久，损失越大。
- 合并 PR 前，作者必须自己先 `git pull --rebase origin main` 解决冲突。

### 5.4 不要提交的临时东西

- 本地调试用的临时关卡：建在 `Content/Developers/<你的名字>/` 下，或者干脆别提交。
- 引擎自动生成的 `Content/Collections/`、`Content/Developers/` 视团队约定，一般不入库。
- 如果 `.gitignore` 里有你觉得该忽略但没忽略的，**提 PR 改 `.gitignore`**，不要用 `git add -f` 绕过。

---

## 6. 合规自检清单

每次推送前过一遍：

- [ ] `git status` 里没有 `Binaries/`、`Intermediate/`、`Saved/`、`DerivedDataCache/`、`.vs/`、`*.sln` 的改动
- [ ] 新增的二进制扩展名已经先写进 `.gitattributes`
- [ ] 没有 `git add -f` 强制添加被忽略的文件
- [ ] 提交里没有本机专属路径、密钥、账号
- [ ] `git lfs ls-files` 的结果里包含你这次提交的资产（说明它真的走了 LFS）

加入资产后的验证：

```powershell
git lfs ls-files | Select-String "你的资产名"
```

如果这里查不到，但 `git status` 显示它被提交了，**说明它以完整二进制进了普通 Git**，需要立刻处理（见第 8 节）。

---

## 7. 常见报错速查

| 报错 / 现象 | 原因 | 处理 |
|---|---|---|
| `This repository is over its data quota` | GitHub LFS 额度用尽 | GitHub Settings → Billing 购买 data pack；同时检查是不是误提交了 `Binaries/`/`Saved/` |
| `batch response: Repository or object not found` | LFS 对象缺失，常见于 clone 时没装 LFS | `git lfs install && git lfs pull` |
| `Encountered N files that should have been pointers, but weren't` | 文件以普通 Git 提交，且被 LFS 规则匹配 | `git lfs migrate import --include="<pattern>"`（见第 8 节） |
| `.uasset` 只有 130 字节，UE 打不开 | 拿到的是 LFS 指针文本，不是内容 | `git lfs pull` |
| `Git LFS is not installed` | 客户端没装 LFS | 装 Git LFS，然后 `git lfs install` |
| 每次 push 都报锁定相关错误 | 误开了 `lfs.locksverify`，而 GitHub 不支持 Locking | `git config --global --unset lfs.locksverify`（见 4.1） |
| `File ... is 130.00 MB; this exceeds GitHub's file size limit of 100.00 MB` | 该扩展名不在 `.gitattributes` 的 LFS 规则里 | 先补规则，再 `git lfs migrate import` |
| 队友说"你的提交里全是行尾变化" | `core.autocrlf` 不一致 | 双方执行 `git config core.autocrlf input`；`.gitattributes` 的 `eol=lf` 是权威 |

---

## 8. 事后补救：文件已经以二进制进了历史

**前提：已经提交并推送过，且其他成员可能已经 clone。**

先装 LFS 迁移工具（一次性）：

```powershell
git lfs migrate info --everything          # 看看哪些类型占用最大
```

改写历史（**会改变所有 commit hash，必须全员配合**）：

```powershell
git lfs migrate import --include="*.uasset,*.umap" --everything
git push --force
```

⚠️ 后果与要求：

1. **所有 commit hash 变化**，其他人必须重新 `git clone`，或用 `git fetch && git reset --hard origin/main`（本地未提交的工作会丢，先让他们备份）。
2. 如果有人基于旧历史开了分支，那些分支也需要重写。
3. 强制推送前**必须**在群里通知并暂停所有人的推送。
4. 操作前先 `git branch backup-before-migrate` 留个后路。

**更省事的做法**：如果资产是刚导入不久、历史很短，直接删掉重新导入 + 在 `.gitattributes` 里先加好规则，再重新提交，通常比迁移历史更快也更安全。

**最好的做法**：永远先写 `.gitattributes`，再 `git add` 资产。这也是本仓库配置文件的既定顺序。

---

## 9. 维护这份配置的人

修改 `.gitignore` / `.gitattributes` / `.ignore` 时注意：

| 文件 | 谁读它 | 作用 |
|---|---|---|
| `.gitignore` | **Git** | 决定什么不入库。新人最容易在这里犯错 |
| `.gitattributes` | **Git + Git LFS** | 决定文件怎么存、怎么 diff、怎么合并、走不走 LFS |
| `.ignore` | **ripgrep / VS Code 搜索 / The Silver Searcher** | **Git 完全不读这个文件**，它只影响代码搜索时排除哪些目录 |

`.ignore` 里排除了 `/Content`，所以**在 VS Code / `rg` 里搜不到资产名**。这是刻意的（否则搜代码会被成千上万条资产命中），但新人容易误判"资产不存在"。如果你需要搜资产，用 UE 编辑器里的 Content Browser，或临时 `rg --no-ignore`。

改动这三个文件后，如果涉及已入库文件的属性变化（例如某个扩展名新增了 LFS 规则），需要重新规范化：

```powershell
git add --renormalize .
git status                 # 检查变化是否符合预期
git commit -m "chore: 更新 .gitattributes 并重新规范化"
```
