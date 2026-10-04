# SlimeWar 协作规范

> 托管：**GitHub**。三条铁律先记住：
> 1. **改 `.uasset` / `.umap` 前，先在群里说一声**（GitHub 不支持文件锁，只能靠这个）。
> 2. **`Content/` `Config/` `Source/` `*.uproject` 入库；`Binaries/` `Intermediate/` `Saved/` `DerivedDataCache/` `.vs/` `*.sln` 绝不入库。**
> 3. **改完尽快推，别让修改在本地过夜。**

---

## 1. 新人装环境

```powershell
git lfs install        # 每台机器一次，必做！不装的话资产会变成 100 多字节的文本指针，UE 打不开
git clone <repo-url>
cd SlimeWar
git lfs ls-files       # 结果要是几百上千，不是 0
```

如果 `git lfs ls-files` 是 0，或 `Content/` 下的 `.uasset` 只有 100 多字节：

```powershell
git lfs install
git lfs pull
```

然后再：右键 `SlimeWar.uproject` → *Generate Visual Studio project files* → 编译。（`SlimeWar.sln` 每人本机生成，不入库。）

顺手设一下行尾，避免整文件假 diff：

```powershell
git config core.autocrlf input
```

---

## 2. 什么入库，什么不入库

| 入库 | 不入库（`.gitignore` 已处理） |
|---|---|
| `Content/` 全部资产 | `Binaries/` `Build/` `Intermediate/` `Saved/` `DerivedDataCache/` |
| `Config/*.ini`（输入映射等共享设置） | `Plugins/*/{Binaries,Intermediate,Saved,DerivedDataCache}/` |
| `Source/` C++ 源码 | `.vs/` `.idea/` `.vscode/` `*.sln` `*.user` |
| `Plugins/*/Content/` 和 `*.uplugin`（**游戏运行依赖**） | `Thumbs.db` `Desktop.ini` `.DS_Store` |
| `SlimeWar.uproject`、`Docs/`、`.gitattributes` | |

⚠️ 两个别踩的坑：

- **别忽略 `Config/`**：`DefaultInput.ini` 是团队共享输入映射，忽略掉所有人拿到的都是引擎默认值。
- **别忽略整个 `Plugins/`**：里面有 Lyra 系插件的资产，缺了工程直接挂。只想让搜索跳过它们的话，写 `.ignore`，不是 `.gitignore`。

---

## 3. LFS 规则

大文件走 LFS，规则按扩展名写在 [.gitattributes](.gitattributes)。已覆盖：`uasset umap ubulk uexp upk t3d`、`fbx obj blend max mb ma abc`、`psd tga png jpg jpeg gif bmp tif exr hdr svg`、`wav wem bnk ogg mp3 flac mp4 mov`。

**引入新扩展名（比如 `.usd`）时，先在 `.gitattributes` 加一行再提交，顺序不能反：**

```
*.usd filter=lfs diff=lfs merge=lfs -text
```

反了的话，文件会以完整二进制进永久历史，之后只能重写历史 + 全员重新 clone。

⚠️ GitHub 的 LFS 有配额（存储 + 月流量），UE 项目容易超，超了 push 直接被拒。**这是别把 `Binaries/`、`Saved/` 入库的另一个原因。**

---

## 4. GitHub 不支持文件锁 ⚠️

`.gitattributes` 里 `*.uasset` / `*.umap` 带 `lockable`，但 **GitHub 没有 Locking API，所以它不起作用**（`git lfs lock` 会返回 404）。保留它只是为了以后搬到 GitLab / Gitea 时能直接用。

**绝对不要开 `lfs.locksverify`**，开了之后每次 push 都会报错而不是安静降级：

```powershell
git config --global --unset lfs.locksverify          # 如果之前开过
git config --global --unset lfs.setlockablereadonly
```

既然没有强制锁，就靠**约定**：

- 改资产前，群里发一条：`认领 Content/Characters/Hero.uasset，30 分钟`。
- 别人认领了同一个文件 → 等，或者商量分工。
- **关卡按人切片**：一人一张，或按 `Levels/` 子目录分职责。TPS 项目里两人同改一张 `.umap` 基本无解。
- 有条件就启用 **World Partition + 外部 Actor（`__ExternalActors__`）**，一个 actor 一个文件，冲突面积从"整张关卡"降到"单个 actor"。

**万一撞车了**（`.uasset` 冲突时 UE 打不开，因为文件里是两段冲突标记）：

```powershell
git lfs checkout                                    # 先取回真实内容
git checkout --theirs Content/Characters/Hero.uasset # 二选一
git add Content/Characters/Hero.uasset
git commit
```

`--ours` / `--theirs` 在 merge 和 rebase 下含义相反，选之前先 `git status` 看清状态；拿不准就 `git merge --abort` 然后 `git pull --rebase`。丢掉的版本通常能用 `git reflog` 找回。

---

## 5. 日常流程

```powershell
git pull --rebase
# ... 在 UE 里干活 ...
git add <具体文件>            # 尽量别 git add -A
git commit -m "art(environment): 导入废弃工厂模块化资产"
git push
```

**提交信息**用 `<type>(<scope>): <描述>`，type 取 `feat` / `fix` / `art` / `refactor` / `chore` / `docs`。

**分支**：`main` 永远保持可开可编。源码/系统改动走短命分支 + PR；**资产改动直接进 `main`**，因为分支养得越久，合并时那次 `.uasset` 二选一损失越大。

**临时东西**放 `Content/Developers/<你的名字>/`，或者干脆别提交。想让 `.gitignore` 忽略什么，提 PR 改 `.gitignore`，不要 `git add -f` 绕过。

**推送前自查**：

- [ ] `git status` 里没有 `Binaries/` `Intermediate/` `Saved/` `DerivedDataCache/` `.vs/` `*.sln`
- [ ] 新加的二进制类型已经先写进 `.gitattributes`
- [ ] `git lfs ls-files | Select-String "刚提交的资产"` 能查到（说明真的走了 LFS）

---

## 6. 报错速查

| 报错 / 现象 | 处理 |
|---|---|
| `This repository is over its data quota` | GitHub Settings → Billing 买 data pack；顺便查是不是误提交了 `Binaries/`、`Saved/` |
| `.uasset` 只有 130 字节，UE 打不开 | 拿到的是指针不是内容 → `git lfs install && git lfs pull` |
| `batch response: Repository or object not found` | 同上，LFS 对象没拉下来 |
| `Encountered N files that should have been pointers` | 文件以普通 Git 提交了 → 见第 7 节 |
| `File ... exceeds GitHub's file size limit of 100 MB` | 该扩展名不在 LFS 规则里 → 先补 `.gitattributes` 再迁移 |
| 每次 push 都报锁定错误 | 误开了 `lfs.locksverify` → 见第 4 节 |
| 队友说"你提交里全是行尾变化" | 双方执行 `git config core.autocrlf input` |

---

## 7. 已经提交错了，怎么救

**如果已经推送、别人也 clone 过了**，改写历史会改变所有 commit hash，必须全员配合：

```powershell
git branch backup-before-migrate                      # 先留后路
git lfs migrate import --include="*.uasset,*.umap" --everything
git push --force
```

强制推送前**必须**在群里通知并让所有人停止推送；之后其他人要重新 clone，或 `git fetch && git reset --hard origin/main`（本地未提交的工作会丢）。

> 历史很短的话，直接删掉重新导入、并先加好 `.gitattributes` 规则，通常比迁移更省事。**最好的做法永远是先写规则再 `git add`。**

---

## 附：三个配置文件谁读谁

| 文件 | 谁读 | 作用 |
|---|---|---|
| `.gitignore` | **Git** | 什么不入库 |
| `.gitattributes` | **Git + LFS** | 怎么存、怎么 diff、怎么合并、走不走 LFS |
| `.ignore` | **ripgrep / VS Code 搜索** | **Git 完全不读**，只影响代码搜索 |

`.ignore` 里排除了 `/Content`，所以 VS Code / `rg` 搜不到资产名。这是刻意的（否则搜代码会被资产淹没），搜资产请用 UE 的 Content Browser。

---

## 8. 仓库内路径必须是 ASCII ⚠️（UE 构建硬约束）

**目录名和文件名只能用小写字母、数字、下划线、短横线。中文名和空格会让 UE 构建直接崩。**

不是风格问题，是 UnrealBuildTool 的坑：它用 `git status` 的结果算"改动工作集"，
而 git 默认会把非 ASCII / 含空格的路径转义成 `"Docs/\347\250\213..."` 这种带引号的形式，
UBT 解析时抛出：

```
System.ArgumentException: Path fragment '"Docs/\347\250\213..."' contains invalid directory separators.
```

**所以：**

- 文档、源码、资产一律用 ASCII 路径（如 `Docs/ProgramTaskList.md`、`L_Sandbox_OnePoint.umap`）。
- 内容可以是中文，路径不行。
- 已经踩过的坑：把 `策划案/` 直接搬进仓库 → 编辑器/命令行编译全挂。
  正确做法是目录改名 `Docs/GameDesign/`，文件名改 `DesignDoc.md`、`Image1.png`。

> 顺带一条：`git config core.quotePath false` 能绕过这个问题，但那是**每台机器**的本地配置，
> 不能指望队友也设。改路径才是根治。
