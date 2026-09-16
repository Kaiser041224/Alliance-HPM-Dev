# Alliance-HPM-Dev — 开发环境仓库

本仓库**只负责开发环境**：容器、工具链、SDK、脚本、工程模板、共享规范。具体工程各自独立成库，统一放在工作区 `projects/` 目录下开发，由本仓库提供统一的环境与工具。

```text
环境仓库（本仓库）          独立工程（projects/*，各自一个 git 仓库）
┌────────────────────┐      ┌───────────────────────────────┐
│ 容器 / 工具链 / SDK │ ───► │ HPM5361_SuperCap              │
│ 脚本 / 模板 / 规范  │      │ HPM5361_WirelessCharger       │
│ projects.yaml 清单  │      │ ...                           │
└────────────────────┘      └───────────────────────────────┘
```

---

## 1. 目录结构

```text
/workspace                             # 环境仓库根 = $HPMDEV_ROOT
├── config/                            # ★ 共享配置单一真源（由 hpmdev 下发）
│   ├── AGENTS.md                      #   开发规范（分层 / C17 / 性能 / 共享策略）
│   ├── clangd.tpl                     #   .clangd 模板（板级路径自动注入）
│   ├── clang-format / clang-tidy
│   ├── editorconfig / cspell.json
│   └── VERSION                        #   环境契约版本
├── templates/
│   └── hpm5361-4layer/                # ★ 工程模板（含 App/Algorithm 可复用算法库）
├── sdk/
│   └── hpm_sdk/                       # HPM 官方 SDK（子模块，pin 具体版本）
├── shared/                            # 跨工程共享代码（HPMDEV_SHARED_DIR，可选）
├── tools/
│   ├── bin/hpmdev                     # ★ 环境统一 CLI
│   └── scripts/                       # build_ui.sh / flash_target.sh / 安装脚本
├── projects/                          # ★ 各独立工程克隆于此（被 .gitignore 排除）
├── projects.yaml                      # 工程清单（只记 url/branch，不 pin commit）
├── .envrc                             # 环境入口（direnv 自动加载）
├── Dockerfile / .devcontainer*/       # 容器与 Dev Container 配置
└── README.md
```

**边界约定**：本仓库不跟踪任何工程内容；`git status` 应始终保持干净（`projects/` 整目录忽略）。

---

## 2. 初始化环境

推荐使用 Dev Container（`.devcontainer/`），或用 `direnv` 在本地加载：

```bash
cd /workspace
direnv allow
```

`.envrc` 会：加载 `hpm_sdk/env.sh`、加入工具链与 `tools/bin` 到 `PATH`、准备 Python venv、探测 OpenOCD / J-Link，并导出：

| 变量 | 含义 |
| :--- | :--- |
| `HPMDEV_ROOT` | 环境仓库根目录 |
| `HPMDEV_SDK_DIR` | SDK 路径（与 `HPM_SDK_BASE` 等价） |
| `HPMDEV_TOOLS_DIR` | 工具/脚本根目录 |
| `HPMDEV_TEMPLATES_DIR` | 工程模板根目录 |
| `HPMDEV_CONFIG_DIR` | 共享配置目录 |
| `HPMDEV_PROJECTS_DIR` | 工程存放目录 |
| `HPMDEV_SHARED_DIR` | 共享代码目录 |
| `HPMDEV_HOST_WORKSPACE` | 宿主机工作区路径（供 debug 路径重映射） |

自检：

```bash
hpmdev doctor     # 环境 / 工具链 / SDK / fileMode 体检
hpmdev env        # 打印解析后的所有路径
```

---

## 3. hpmdev 命令

```bash
hpmdev new <name> [--template <t>] [--board <b>]   # 从模板创建新工程
hpmdev clone [name]                                # 按 projects.yaml 克隆工程（缺省全部）
hpmdev list                                        # 列出工程及其 git 状态
hpmdev sync-config <name>|--all                    # 下发 config/ 到工程（或模板）
hpmdev doctor                                      # 环境体检
hpmdev env                                         # 打印环境路径
```

### 3.1 新建工程

```bash
hpmdev new my_motor_ctrl
cd projects/my_motor_ctrl
make build
```

自动完成：复制模板 → 重命名板级 `user_board` → 下发配置（`.clangd` / `.clang-format` / … / `AGENTS.md`）→ 生成 `.code-workspace`（相对引用 `sdk/hpm_sdk` 与 `shared`）→ `git init` 并提交初始脚手架。

### 3.2 接入既有工程

1. 在 `projects.yaml` 登记 `name / url / branch`（可选 `default_board`）。
2. 执行 `hpmdev clone`（或 `git clone` 到 `projects/<name>`）。
3. 执行 `hpmdev sync-config <name>` 下发共享配置。

`hpmdev list` 会显示清单内工程是否存在、分支与脏文件数。

### 3.3 更新共享配置

修改 `config/` 下的规范文件后下发：

```bash
hpmdev sync-config --all
```

`AGENTS.md` 的「项目附加约定」小节会被保留，不会被覆盖。

---

## 4. 工程构建与烧录

在工程目录内：

```bash
make configure        # 生成构建系统
make build            # 编译（日志 build/last_build.log）
make artifacts        # 导出产物到 output/
make clean
make flash            # OpenOCD 烧录（make flash-jlink 走 J-Link）
```

可覆盖：`make build BOARD=<board> CMAKE_BUILD_TYPE=Release HPM_BUILD_TYPE=flash_xip`。

工程 Makefile 全部通过环境变量定位 SDK 与脚本，并在缺省时回退到标准布局 `../../sdk/hpm_sdk`、`../../tools`，因此脱离 `.envrc` 也能按标准目录构建。

---

## 5. 共享代码策略

`alliance_hpm_base_platform` 共享库已废弃（见 `shared/README.md`），不再以子模块引入。可复用内容通过两条途径传播：

1. **模板传播**：两个实战工程沉淀的共享栈由 `templates/hpm5361-4layer` 携带，新工程自动获得——`App/Algorithm`（PID / PLL / 滤波 / RMS / 斜坡 / 迟滞 / 前馈）、`App/Platform`（ADC16 PMT / HRPWM / GPTMR / GPIO / CAN / WS2812 / 模拟量调理）、`App/Debug`（RTT 与自检）、完整 `Interface` + `Driver` 栈。
2. **环境变量共享**：需要跨工程共享但不想进模板的代码放到 `shared/`，工程通过 `$HPMDEV_SHARED_DIR` 可选引用；脱离工作区自动降级，不影响独立构建。

判断标准：只服务一个工程 → 留在工程内；多个工程都用 → 进模板或 `shared/`。

---

## 6. 环境版本与契约

- `config/VERSION` 记录环境契约版本（当前 `1.0.0`）。
- SDK 以子模块 pin 具体提交，`hpmdev doctor` 校验 `HPM_SDK_BASE` 与本仓库布局一致。
- 工程侧规范、工具链配置全部来自 `config/`，不要在工程内直接改（会被下次 `sync-config` 覆盖）。

---

## 7. 与旧布局的差异

| 旧 | 新 |
| :--- | :--- |
| `hpm_sdk/` | `sdk/hpm_sdk/` |
| `user_template/` | `templates/hpm5361-4layer/` |
| `alliance_hpm_base_platform/`（子模块） | 已废弃，能力由模板 / `shared/` 承接 |
| `HPM5361_*` 直接放在根目录 | `projects/HPM5361_*`（独立仓库，整目录忽略） |
| 根目录 `.clang-format` 等 | `config/`（单一真源，经 `hpmdev sync-config` 下发） |
| `tools/scripts/new_project` | `hpmdev new`（`new_project.sh` 保留为兼容包装） |
| `-fdebug-prefix-map` 硬编码宿主路径 | 读取 `HPMDEV_HOST_WORKSPACE`，映射到 `projects/<name>` 与 `sdk/hpm_sdk` |

---

## 8. 常见问题

**Q：`git submodule status` 报错？**
旧版本中 `HPM5361_SuperCap` 是未登记 gitlink。现已摘除，`.gitmodules` 只保留 `sdk/hpm_sdk`。

**Q：`git status` 出现大量 644→755 权限变化？**
已在本仓库与各工程设置 `core.fileMode=false`；新 clone 的工程可由 `hpmdev clone` 自动设置。

**Q：工程里 `make` 找不到 SDK？**
先 `direnv allow` 或 `hpmdev doctor`；工程 Makefile 也会回退到 `../../sdk/hpm_sdk`。
