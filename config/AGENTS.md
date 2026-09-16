<!--
  Canonical development guide for all HPM5361 projects.
  Distributed by: hpmdev sync-config
  Source of truth: <env-repo>/config/AGENTS.md
  Do NOT edit the distributed copy in a project; edit the env repo and re-run sync-config.
-->
# AGENTS.md - 嵌入式 C17 解耦架构开发指南

## 0. 本文件来源与环境约定

本文件由环境仓库统一分发，是**所有工程共享的开发规范**。项目本地如需追加约定，请写在本文件末尾的「项目附加约定」小节，升级时不会被覆盖。

环境仓库（`Alliance-HPM-Dev`）只负责：容器 / 工具链 / SDK / 脚本 / 工程模板 / 共享规范。
具体工程各自独立成库，统一放在工作区的 `projects/` 目录下开发，并复用环境变量指向的 SDK 与工具。

关键环境变量（由 `.envrc` 导出）：

| 变量 | 含义 |
| :--- | :--- |
| `HPMDEV_ROOT` | 环境仓库根目录 |
| `HPMDEV_SDK_DIR` | HPM SDK 路径（等价于 `HPM_SDK_BASE`） |
| `HPMDEV_TOOLS_DIR` | 工具/脚本根目录 |
| `HPMDEV_TEMPLATES_DIR` | 工程模板根目录 |
| `HPMDEV_SHARED_DIR` | 共享代码根目录（可选，见第 6 节） |
| `HPMDEV_PROJECTS_DIR` | 各独立工程存放目录 |

---

## 1. 核心设计哲学 (Core Philosophy)

本工程采用 **“物理隔离 + 契约驱动”** 模式。其核心目标是实现业务逻辑（App）与芯片外设（SDK/HAL）的深度解耦。

- **原子目标**：App 层代码与硬件外设深度解耦，仅通过 Interface 层访问。
- **现代化 C**：强制使用 **C17** 标准（匿名结构体、泛型选择 `_Generic`、静态断言 `static_assert`）。
- **高性能**：针对 HPM 系列 RISC-V 架构优化，强制执行 L1 Cache 对齐与 ILM (RAMFUNC) 部署。

---

## 2. 目录分层与依赖守则 (Strict Layering)

当前工程维持 **App 内部分层 + Interface 契约层 + Driver 适配层 + Board 板级层** 的结构，不额外引入独立 `Module/` 目录。

### 2.1 顶层分层

| 目录层级 | 职责 (Responsibility) | 允许包含的头文件 | 禁止项 (Hard Bans) |
| :--- | :--- | :--- | :--- |
| **1. App/** | 应用级代码总入口；内部再细分为 `Application/`、`Control/`、`Algorithm/`、`Platform/`、`Debug/` | `App/*`, `Interface/`, `<stdint.h>`, `<stdbool.h>`, `<stddef.h>` | 禁止直接包含任何 `hpm_*.h`；禁止跨层直接依赖 `Board/` 私有实现 |
| **2. Interface/** | **契约定义层**。定义硬件抽象对象 (HAL) 与跨层访问协议 | 纯 C 标准库头文件 | 禁止包含私有变量、静态函数或驱动实现 |
| **3. Driver/** | SDK 适配实现。将物理寄存器操作映射至 Interface 契约 | `hpm_sdk.h`, `board_pins.h`, `Interface/` | 禁止在此处编写业务逻辑、控制策略或状态机 |
| **4. Board/** | 硬件底表。定义原始引脚、时钟、板级资源与 IOCFG | `hpm_soc.h` (仅宏定义) | 禁止承载业务逻辑、控制算法或应用编排 |

### 2.2 App/ 内部分层

| 子目录 | 职责 | 允许依赖 | 禁止项 |
| :--- | :--- | :--- | :--- |
| **App/Application/** | 应用编排层；负责系统启动后的业务流程、状态机、模态切换、任务组织、上位机通讯编排 | `App/Control/`, `App/Platform/`, `App/Algorithm/`, `Interface/` | 禁止直接调用 `Driver/` 私有接口；禁止直接操作寄存器或 `hpm_*` API |
| **App/Control/** | 实际控制器层；负责闭环控制、保护策略、控制状态、调节器组合与控制输出决策 | `App/Algorithm/`, `App/Platform/`, `Interface/` | 禁止承担上位机协议编排；禁止直接依赖 `Driver/` 私有接口 |
| **App/Algorithm/** | 纯算法库；提供 PID、PLL、滤波、RMS、斜坡等可复用计算模块 | C 标准库、`<math.h>`、`<float.h>`、必要的通用类型头 | 禁止依赖 `Driver/`、`Board/`，尽量避免依赖具体业务状态机 |
| **App/Platform/** | 面向应用的硬件能力封装；把 `Interface/` 组合成可供 `Application/Control/` 使用的稳定能力 | `Interface/`, 必要的 App 公共类型头 | 禁止在此处写具体业务状态机；禁止把 SDK/HAL 细节向上泄漏 |
| **App/Debug/** | 调试、验证、自检、RTT 输出、临时 bring-up 入口 | `Interface/`, `App/Platform/`, `App/Control/` | 禁止成为正式业务默认路径；禁止让调试逻辑反向主导生产代码结构 |

### 2.3 推荐调用方向

```text
Application -> Control -> Platform -> Interface -> Driver -> Board
            \-> Platform -> Interface -> Driver -> Board
Control     -> Algorithm
Debug       -> Platform / Control / Interface
```

补充约束：

- `Application/` 负责“做什么、什么时候做、处于什么模式”。
- `Control/` 负责“控制怎么算、如何保护、输出什么控制量”。
- `Platform/` 负责“硬件能力如何被安全、稳定地提供给上层”。
- `Debug/` 仅作为验证辅助层，不能长期承载正式运行主流程。
- 若某个对象天然属于控制语义（如功率级控制器、采样同步控制器、保护控制器），优先落在 `Control/`，而不是额外拆出 `Module/`。

---

## 3. C17 编码与性能规范 (Coding Standards)

### 3.1 接口对象化 (Object-Oriented C17)

接口定义必须使用 **匿名结构体**。所有物理参数需在驱动层完成归一化（例如：占空比统一为 `float` [0.0-1.0]）。

```c
// 示例：Interface/intf_pwm.h
typedef struct {
    uint8_t instance_id;
    struct {
        status_t (*init)(void);
        status_t (*set_duty)(float duty);
    }; // 匿名结构体：允许对象通过 dev->set_duty() 直接调用
} pwm_if_t;
```

### 3.2 强制 C17

- 以 `-std=c17` 编译；`CMakeLists.txt` 中设置 `CMAKE_C_STANDARD 17` / `CMAKE_C_STANDARD_REQUIRED ON`。
- 允许：匿名结构体 / 联合体、`_Generic`、`_Static_assert`、指定初始化器。
- 禁止：GNU 扩展语法（除非 SDK 头文件强制要求）。

### 3.3 性能约定

- 中断热路径与高频调用函数使用 `ATTR_RAMFUNC` 放入 ILM，常量表使用 `ATTR_DLM` / 对应段属性放入 DLM。
- 频繁访问的全局状态放到 fast RAM，避免 AHB SRAM 访问延迟。
- 避免在热路径使用函数指针间接调用，必要时内联；`clampf` 之类的微函数标注 `always_inline`。
- 浮点域统一 `float`；仅在必要处使用 `double`（HPM5361 具备双精度 FPU，但 ISR 内优先单精度）。

---

## 4. 构建与调试

```bash
# 在具体工程目录下
make configure        # 生成构建系统
make build            # 编译（产物在 build/）
make artifacts        # 导出到 output/
make clean            # 清理
```

常用覆盖项：

```bash
make build BOARD=<board_name> CMAKE_BUILD_TYPE=Release HPM_BUILD_TYPE=flash_xip
```

- 排障先看 `build/last_build.log`。
- 调试信息路径重映射由 Makefile 的 `DEBUG_PREFIX_MAP` 控制；宿主机路径通过 `HPMDEV_HOST_WORKSPACE` 提供。
- 烧录：`make flash`（OpenOCD）或 `make flash-jlink`。

---

## 5. 配置分发与项目独立性

- `.clang-format` / `.clang-tidy` / `.clangd` / `.editorconfig` / `.cspell.json` / 本文件均由环境仓库 `config/` 分发。
- 修改规范请改环境仓库，然后运行 `hpmdev sync-config <project>`（或 `--all`）重新下发。
- 项目必须能仅依赖「环境仓库 + 自身」完成构建；不得记录对本工作区绝对路径的硬编码（一律走环境变量）。

---

## 6. 共享代码策略

- 旧的 `alliance_hpm_base_platform` 共享库已废弃，不再作为子模块引入。
- 可复用算法/驱动的传播途径有两条：
  1. **模板传播**：成熟的通用模块（如 `App/Algorithm`）由环境仓库的工程模板携带，新工程创建时自动获得。
  2. **环境变量共享**：需要跨工程共享的代码放到 `HPMDEV_SHARED_DIR` 指向的目录，工程通过该环境变量可选引用；脱离工作区时自动降级为「不启用」，不破坏独立构建。
- 任何被多个工程使用的独立想法，优先沉淀进模板或环境仓库，而不是在各工程间复制粘贴。

---

## 7. 模板内置共享模块（`templates/hpm5361-4layer`）

模板已内置以下经实战验证的模块，新工程创建后按实际硬件改配置即可，无需从零实现：

| 层 | 模块 | 说明 |
| :--- | :--- | :--- |
| Interface | `intf_adc / hrpwm / gptmr / gpio / can / trgm / synt / uart / clock / ws2812` | 契约层，含 `intf_default.c` 分发实现 |
| Driver | `drv_clock / adc / hrpwm / gptmr / trgm / gpio / mcan / synt / ws2812 / uart` | SDK 适配实现 |
| Driver | `IrqProfiler` / `WS2812` | 中断占用分析 / 灯带驱动 |
| Platform | `app_adc`（ADC16 PMT 采样链） / `app_analog_signal`（标定 + 多类型滤波） / `app_hrpwm`（含移相与软启动预热） / `app_gptmr` / `app_gpio` / `app_can` / `app_ws2812` / `app_buzzer` | 应用可用能力 |
| Debug | `app_debug` / `app_debug_adc` / `app_debug_hrpwm` / `app_debug_can` / `app_debug_profiler` / `app_debug_rtt` / `app_debug_trap` | RTT 输出与自检入口 |
| Algorithm | `algo_pid / algo_pll / algo_filter / algo_ramp / algo_rms / algo_hyst / algo_ffd` | 纯算法，零硬件依赖 |

命名约定（中性名，按实际拓扑改名即可）：

- ADC 通道：`ADC_CH_V_IN / I_IN / I_L / V_OUT / I_OUT / I_AUX`
- 模拟量项：与 ADC 通道一一对应
- HRPWM：`HRPWM_PAIR_A/B` → PWM0，`PAIR_C/D` → PWM1；实例 `HRPWM_INST_0/1`
- GPTMR：`APP_GPTMR_CH_0/1/2`

模板板级：`Board/user_board`（HPM5361，默认，`hpmdev new` 会改名）；`Board/hpm5301evklite_board` 为 HPM5301 参考板，无法构建 HPM5361 平台栈。

---

## 项目附加约定

<!-- 各项目在此追加本地约定，sync-config 不会覆盖本小节。 -->
