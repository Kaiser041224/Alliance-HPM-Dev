# shared/ — 共享代码目录（环境变量驱动）

本目录用于存放**可跨多个工程复用、但又不适合放进模板**的代码与资源。

## 用法

环境仓库的 `.envrc` 会导出：

```bash
export HPMDEV_SHARED_DIR="$HPMDEV_ROOT/shared"
```

工程在 CMake 中可选引用，且必须做「存在性判断」，保证工程脱离工作区时仍能独立构建：

```cmake
# 示例：可选引入共享代码
if(DEFINED ENV{HPMDEV_SHARED_DIR} AND EXISTS "$ENV{HPMDEV_SHARED_DIR}/algo")
    sdk_app_inc($ENV{HPMDEV_SHARED_DIR}/algo/include)
    sdk_app_src($ENV{HPMDEV_SHARED_DIR}/algo/src/foo.c)
endif()
```

## 约定

- 这里放的是**跨工程**共享物：公共算法、通用驱动封装、通信协议、matlab 生成脚本等。
- 只服务单一工程的代码，请留在工程自身仓库内。
- 通用的、稳定的模块应优先沉淀进 `templates/`（新工程直接获得），`shared/` 用于「不想进模板但又想共享」的场景。
- 本目录默认不跟踪任何大体积第三方产物；第三方依赖优先走 SDK 或工具链。

## 关于已废弃的 `alliance_hpm_base_platform`

原 `alliance_hpm_base_platform` 子模块（GPIO/UART/SPI/CAN，C++）长期未更新且无工程引用，已从环境仓库移除，不再作为子模块维护。

- 上游仓库仍保留：<https://github.com/liguijia/alliance_hpm_base_platform>
- 如需历史参考：`git clone https://github.com/liguijia/alliance_hpm_base_platform`
- 后续通用能力请通过 **工程模板** 或 **本 `shared/` 目录** 传播，不要重新引入子模块。
