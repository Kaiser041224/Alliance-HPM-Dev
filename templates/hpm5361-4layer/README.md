# HPM5361 四层架构工程模板

HPM (HPMicro) 嵌入式开发工程模板，基于 **Board / Interface / Driver / App** 四层解耦架构，强制 **C17**。

本模板由环境仓库 `Alliance-HPM-Dev` 维护，已内置两个实战工程（SuperCap / WirelessCharger）沉淀下来的**共享平台栈**。新工程请通过 `hpmdev new` 派生，不要直接改本目录。

## 架构概览

```
┌──────────────────────────────────────────────────────────────┐
│  App/                                                        │
│    Logic/       业务入口骨架（app_init/app_run）              │
│    Algorithm/   ★ 纯算法库：PID / PLL / 滤波 / RMS / 斜坡 /    │
│                   迟滞 / 前馈（零硬件依赖）                    │
│    Platform/    ★ 外设能力封装：ADC(PMT) / HRPWM / GPTMR /     │
│                   GPIO / CAN / WS2812 / 模拟量调理 / 蜂鸣器    │
│    Debug/       ★ RTT / ADC / HRPWM / CAN 调试与中断占用分析    │
├──────────────────────────────────────────────────────────────┤
│  Interface/      契约层：intf_* 抽象接口 + intf_default 分发    │
├──────────────────────────────────────────────────────────────┤
│  Driver/         SDK 适配层（唯一可调用 hpm_* API 的地方）      │
│    hpm_impl/     drv_clock / adc / hrpwm / gptmr / trgm / ...  │
│    IrqProfiler/  中断占用分析                                  │
│    WS2812/       WS2812 灯带驱动                               │
├──────────────────────────────────────────────────────────────┤
│  Board/          板级 BSP（时钟 / 引脚 / flash 选项）           │
└──────────────────────────────────────────────────────────────┘
```

详细分层规范见 [AGENTS.md](AGENTS.md)。

## 目录结构

```
hpm5361-4layer/
├── CMakeLists.txt / Makefile
├── AGENTS.md / .clangd / .clang-format …   # 由环境仓库分发
├── Board/
│   ├── user_board/             # ★ HPM5361 通用板（默认；hpmdev new 会改名为 <project>_board）
│   └── hpm5301evklite_board/   # HPM5301 EVK 参考板（仅作 SDK 对照，见下方说明）
├── Interface/                  # 11 个 intf_*.h + intf_default.c
├── Driver/                     # hpm_impl / IrqProfiler / WS2812
├── App/
│   ├── main.c
│   ├── Logic/app_logic.c       # UART hello 骨架
│   ├── Algorithm/{Inc,Src}/    # 7 个可复用算法模块
│   ├── Platform/{Inc,Src}/     # 8 个平台模块
│   └── Debug/{Inc,Src}/        # 7 个调试模块
├── linkers/                    # gcc / iar / segger
└── README.md
```

## 默认板级与 SoC 说明

| 板级 | SoC | 说明 |
|------|-----|------|
| `user_board` | **HPM5361** | 默认板级。新工程由 `hpmdev new` 自动改名为 `<project>_board`，并在其中填写自己的引脚/时钟。 |
| `hpm5301evklite_board` | HPM5301 | SDK 官方 EVK，保留作对照。**平台栈（HRPWM/ADC16 PMT 等）面向 HPM5361，选择该板无法构建。** |

> 若你确实需要 HPM5301 工程，请自行按 5301 裁剪平台源文件清单（`CMakeLists.txt`）。

## 中性命名约定

平台栈中的信号/通道命名已统一为**中性名**，新工程按实际拓扑改名即可：

| 位置 | 约定 |
|------|------|
| ADC 通道 (`ADC_CH_*`) | `V_IN / I_IN / I_L / V_OUT / I_OUT / I_AUX` |
| 模拟量项 (`APP_ANALOG_SIGNAL_ITEM_*`) | 与 ADC 通道一一对应 |
| HRPWM 通道 (`HRPWM_PAIR_*`) | `PAIR_A/B` → PWM0，`PAIR_C/D` → PWM1；实例 `HRPWM_INST_0/1` |
| GPTMR 通道 (`APP_GPTMR_CH_*`) | `CH_0/1/2`，默认频率见 `app_gptmr.c` |

## 快速开始

依赖工作区环境（先 `direnv allow`，或确保 `$HPM_SDK_BASE` 已导出）：

```bash
make configure      # 生成构建系统（默认 BOARD=user_board）
make build          # 编译（日志 build/last_build.log，含内存占用可视化）
make artifacts      # 导出产物到 output/
make clean

make BOARD=user_board build   # 显式指定板级
make flash                    # OpenOCD 烧录
```

## 创建新工程（推荐）

```bash
hpmdev new my_motor_ctrl
```

自动完成：复制模板 → 板级改名 `<name>_board` 并同步内部标识 → 下发 `.clangd` / `.clang-format` / `.clang-tidy` / `.editorconfig` / `.cspell.json` / `AGENTS.md` → 生成 `.code-workspace`（相对引用 `hpm_sdk` 与 `shared`）→ `git init` 并提交。

## 新工程落地后的定制清单

1. `Board/<project>_board/*.yaml`：SoC / flash 容量 / OpenOCD 配置。
2. `Board/<project>_board/pinmux.c`：按实际硬件改引脚，`board.c` 的 flash 选项。
3. `App/Platform/Inc/app_adc.h` + `App/Platform/Src/app_adc.c`：ADC 通道与 PMT 触发映射。
4. `App/Platform/Src/app_hrpwm.c`：PWM 频率 / 死区 / 反相 / 移相配置。
5. `App/Platform/Src/app_gptmr.c`：定时器通道与频率。
6. `App/Platform/Src/app_gpio.h`：GPIO 引脚宏。
7. `App/Logic/app_logic.c`：替换为业务入口。

## 许可证

BSD-3-Clause
