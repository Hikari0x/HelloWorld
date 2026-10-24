# CANN 9.0 自定义算子手册（Ubuntu 24.04 · arm64）

> 目标：全新虚机 → 命令行立骨架 → 编译出算子包 → 转 CLion 远程开发 → 上云跑通 `y = x + 1`
> 环境：Apple Silicon Mac + Ubuntu 24.04 虚机（aarch64），**无 NPU**，只做编码与编译
> 实测：Ubuntu 24.04.5 · CANN 9.0.0 · CMake 3.28.3（系统）/ 4.0.2（CLion 内置）· GCC 13.3 / gcc-12 · 4 核 5G
> 约定：示例用户 `lht`、工程 `~/projects/HelloWorld`，按你的实际情况替换

## 0. 怎么用这份手册

| 阶段 | 在哪做 | 做什么 | 完成标志 |
|---|---|---|---|
| 1 立骨架 | 命令行 | 装 CANN、`msopgen` 生成、写三个源文件 | `bash build.sh` 出 `.run` |
| 2 转 IDE | CLion 远程 | 接一次远程连接，改**生成器**，重置缓存 | CMake 面板 `Configuring done` |
| 3 日常 | CLion + 终端 | 编辑、构建、提交 | 两边都能构建，`git status` 干净 |

顺序不能反：**先用终端跑通，再进 IDE**。IDE 里出不来的问题，终端一次就能定位。
每节末尾都有验收命令，按顺序执行即可复现。全文命令实测于 2026-10 的干净虚机。

## 1. 前置条件

| 项 | 要求 | 说明 |
|---|---|---|
| 虚机 | Ubuntu 24.04 / aarch64 | `uname -m` 必须是 `aarch64` |
| 磁盘 | **≥ 30G** | 安装包 3.5G + CANN 安装后 8.6G + CLion 后端 5.7G + 产物 ~1G |
| 内存 | 5G 可跑，8G 更稳 | 实测 4 核 5G 可完成全流程 |
| 网络 | 虚机需能出网 | 下 CANN；CLion 首次要从 `download.jetbrains.com` 拉后端 |
| 客户端 | Mac 上的 CLion 2025.2 | 远程开发由它发起，版本会自动匹配后端 |

```bash
uname -m && lsb_release -d && df -h $HOME | tail -1 && free -g | head -2
```

## 2. 虚机初始化

```bash
sudo apt update && sudo apt install -y openssh-server
sudo systemctl enable --now ssh
ip -4 addr show | grep inet        # 记下 IP，后面 SSH / CLion 都要用
```

Mac 上验证一次（后面两种终端都靠它）：

```bash
ssh lht@192.168.x.x 'echo ok && uname -m'
```

## 3. 装依赖

```bash
sudo apt update
sudo apt install -y build-essential cmake unzip wget gcc-12 g++-12 \
                    python3 python3-dev python3-pip libssl-dev zlib1g-dev pkg-config
```

## 4. 安装 CANN（安装包约 3.5G）

```bash
mkdir -p ~/cann_pkg && cd ~/cann_pkg
wget -c "https://ascend-cann-open.obs.cn-north-4.myhuaweicloud.com/CANN/CANN%209.0.0/Ascend-cann-toolkit_9.0.0_linux-aarch64.run"
wget -c "https://ascend-repo.obs.cn-east-2.myhuaweicloud.com/CANN/CANN%209.0.0/Ascend-cann-910b-ops_9.0.0_linux-aarch64.run"

# 顺序不能反：先 toolkit，再 ops
bash Ascend-cann-toolkit_9.0.0_linux-aarch64.run --install --install-path=$HOME/Ascend
bash Ascend-cann-910b-ops_9.0.0_linux-aarch64.run   --install --install-path=$HOME/Ascend

du -sh ~/Ascend        # 期望约 8.6G
```

## 5. 终端环境（外部 SSH 与 IDE 内置终端通用）

```bash
source ~/Ascend/cann-9.0.0/set_env.sh
```

写进 `~/.bashrc` 时**必须加交互式判断**；已有的那行 `source` 要替换掉，不要追加第二行：

```bash
sed -i '\#source $HOME/Ascend/cann-9.0.0/set_env.sh#d' ~/.bashrc
cat >> ~/.bashrc <<'EOF'

# CANN 只在交互式 shell 生效，避免污染 scp / 非交互 ssh
if [[ $- == *i* ]]; then
    source "$HOME/Ascend/cann-9.0.0/set_env.sh" >/dev/null 2>&1
fi
EOF
```

为什么要判断：`set_env.sh` 会往 stdout 输出，无条件 source 会把 `scp`、`git` 的远程命令搞脏。

这一步只管终端。**CLion 后端不读 `.bashrc`**，IDE 那边靠第 7.4 节的兜底文件解决。

## 6. 验证安装

必须在 `source` 之后、同一个 shell 里：

```bash
echo $ASCEND_HOME_PATH                        # .../Ascend/cann-9.0.0
ls $ASCEND_HOME_PATH/include | head -3        # acl aclnn ...
python3 -c "import acl; print('acl ok')"      # acl ok
which msopgen                                 # $ASCEND_HOME_PATH/bin/msopgen
cat ~/Ascend/ascend_cann_install.info         # 9.0.0 没有 version.cfg，别去找
```

## 7. 生成工程骨架（命令行）

### 7.1 算子描述文件

```bash
mkdir -p ~/projects && cd ~/projects
vim hello_world.json && chmod 600 hello_world.json
```

```json
[
  {
    "op": "HelloWorld",
    "input_desc":  [ { "name": "x", "param_type": "required", "format": ["ND"], "type": ["float16"] } ],
    "output_desc": [ { "name": "y", "param_type": "required", "format": ["ND"], "type": ["float16"] } ],
    "attr": []
  }
]
```

### 7.2 生成

```bash
msopgen gen -i hello_world.json -c ai_core-ascend910b -lan cpp -out HelloWorld
cd HelloWorld && ls
```

> `msopgen` 有安全检查：工作目录不能对 group/other 可写，否则报
> `The path ... should not be written by user group or others`。别在 `/tmp`、共享目录、Windows 挂载盘里生成。

```text
HelloWorld/
├── build.sh              # 一键构建（source 环境 → cmake --preset → binary + package）
├── CMakeLists.txt
├── CMakePresets.json
├── framework/tf_plugin/  # 模板自带，不用动
├── op_host/hello_world.cpp
└── op_kernel/{hello_world.cpp, hello_world_tiling.h}
```

### 7.3 三处对齐（最容易踩坑的一步）

`msopgen 9.0.0` 不管 `-c` 传什么，`CMakePresets.json` 和代码模板都写 `ascend910`。910B 要用 `ascend910b`，
**presets 和代码必须一致**，否则构建时找不到 `aic-<unit>-ops-info.ini`（BUG 2）。
顺带把 presets 里写死的 CANN 路径换成环境变量，工程才能跨机器（BUG 8）。

```bash
# ① 代码与 presets 的 compute unit 对齐
sed -i 's/"value": "ascend910"/"value": "ascend910b"/' CMakePresets.json
sed -i 's/AddConfig("ascend910")/AddConfig("ascend910b")/' op_host/hello_world.cpp

# ② presets 版本提升 + 路径跟随环境变量（需要 CMake ≥ 3.21，本机 3.28 满足）
python3 - <<'PY'
import json
p = 'CMakePresets.json'
d = json.load(open(p))
d['version'] = 3
d['configurePresets'][0]['cacheVariables']['ASCEND_CANN_PACKAGE_PATH']['value'] = '$env{ASCEND_HOME_PATH}'
json.dump(d, open(p, 'w'), indent=4)
PY

# ③ 自检：两条输出必须是同一个值；路径必须是 $env{ASCEND_HOME_PATH}
grep -A2 ASCEND_COMPUTE_UNIT CMakePresets.json | grep value
grep -n AddConfig op_host/hello_world.cpp
grep -A2 ASCEND_CANN_PACKAGE_PATH CMakePresets.json | grep value
```

### 7.4 加环境兜底（IDE 能用的关键）

CLion 后端不读 `.bashrc`，`find_package(ASC)` 会直接失败。把"找 CANN"写进工程即可，`CMakeLists.txt` 加一行
（位置必须在 `find_package(ASC)` **之前**）：

```cmake
cmake_minimum_required(VERSION 3.16.0)
project(opp)

# CANN 环境兜底：让没 source set_env.sh 的构建（CLion / 新终端）也能 configure
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/cann_env.cmake)

find_package(ASC REQUIRED)
```

```bash
mkdir -p cmake
cat > cmake/cann_env.cmake <<'EOF'
# ---------------------------------------------------------------------------
# CANN 环境兜底
#
# 目的：让「没有 source set_env.sh」的构建（CLion 远程开发、新终端、CI）也能
#      正常 configure。已 source 过时这段什么都不做，只补齐缺失的变量，不覆盖已有值。
#
# 生效范围：configure 阶段（opbuild / python 生成脚本）。
# 注意：build 阶段（make / asc_opc / ccec）用的是 make 进程自己的环境，
#      所以编译 kernel、打 .run 仍然要在 source 过 set_env.sh 的 shell 里跑。
# ---------------------------------------------------------------------------
if(NOT ASCEND_CANN_PACKAGE_PATH)
    if(DEFINED ENV{ASCEND_HOME_PATH} AND EXISTS "$ENV{ASCEND_HOME_PATH}")
        set(ASCEND_CANN_PACKAGE_PATH "$ENV{ASCEND_HOME_PATH}" CACHE PATH "CANN install path" FORCE)
    elseif(EXISTS "$ENV{HOME}/Ascend/cann-9.0.0")
        set(ASCEND_CANN_PACKAGE_PATH "$ENV{HOME}/Ascend/cann-9.0.0" CACHE PATH "CANN install path" FORCE)
    else()
        message(FATAL_ERROR
            "找不到 CANN 安装目录。请先 source $HOME/Ascend/cann-9.0.0/set_env.sh，"
            "或者用 -DASCEND_CANN_PACKAGE_PATH=<CANN 目录> 指定。")
    endif()
    message(STATUS "CANN 环境兜底：ASCEND_CANN_PACKAGE_PATH=${ASCEND_CANN_PACKAGE_PATH}")
endif()

if(NOT DEFINED ENV{ASCEND_HOME_PATH})
    set(ENV{ASCEND_HOME_PATH} "${ASCEND_CANN_PACKAGE_PATH}")
endif()

list(APPEND CMAKE_PREFIX_PATH
    "${ASCEND_CANN_PACKAGE_PATH}/lib64/cmake"
    "${ASCEND_CANN_PACKAGE_PATH}/toolkit/tools/tikicpulib/lib/cmake")

if(NOT DEFINED ENV{LD_LIBRARY_PATH})
    set(ENV{LD_LIBRARY_PATH} "${ASCEND_CANN_PACKAGE_PATH}/lib64")
endif()
if(NOT DEFINED ENV{PYTHONPATH})
    set(ENV{PYTHONPATH} "${ASCEND_CANN_PACKAGE_PATH}/python/site-packages")
endif()
if(NOT "$ENV{PATH}" MATCHES "${ASCEND_CANN_PACKAGE_PATH}/bin")
    if(DEFINED ENV{PATH})
        set(ENV{PATH} "$ENV{PATH}:${ASCEND_CANN_PACKAGE_PATH}/bin:${ASCEND_CANN_PACKAGE_PATH}/tools/ccec_compiler/bin")
    else()
        set(ENV{PATH} "${ASCEND_CANN_PACKAGE_PATH}/bin:${ASCEND_CANN_PACKAGE_PATH}/tools/ccec_compiler/bin")
    endif()
endif()

if(NOT DEFINED ASCEND_COMPUTE_UNIT)
    set(ASCEND_COMPUTE_UNIT "ascend910b" CACHE STRING "compute unit，必须与 op_host 里的 AddConfig 一致" FORCE)
endif()
if(NOT DEFINED vendor_name)
    set(vendor_name "customize" CACHE STRING "vendor name" FORCE)
endif()
if(NOT DEFINED ENABLE_SOURCE_PACKAGE)
    set(ENABLE_SOURCE_PACKAGE ON CACHE BOOL "build source package" FORCE)
endif()
if(NOT DEFINED ENABLE_BINARY_PACKAGE)
    set(ENABLE_BINARY_PACKAGE ON CACHE BOOL "build binary package" FORCE)
endif()

# CMake 3.31+ 下 CANN 9.0 的 install() 会刷一堆 CMP0177 开发警告（无害，只是噪音）。
# 等价于 cmake -Wno-dev，只影响"开发警告"，不影响真正的错误。
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "suppress CMake developer warnings" FORCE)
EOF
```

`cann_env.cmake` 做五件事，全部**只补缺、不覆盖**：

| # | 做什么 | 解决 |
|---|---|---|
| 1 | `ASCEND_CANN_PACKAGE_PATH` 空时按 `$ASCEND_HOME_PATH` → `$HOME/Ascend/cann-9.0.0` 兜底，都没有就报人话错误 | IDE 里没环境变量 |
| 2 | 补 `ASCEND_HOME_PATH` / `LD_LIBRARY_PATH` / `PYTHONPATH` / `PATH` | `libgraph.so` 找不到 |
| 3 | `<CANN>/lib64/cmake` 追加到 `CMAKE_PREFIX_PATH` | `Could not find ... "ASC"` |
| 4 | 给 `ASCEND_COMPUTE_UNIT=ascend910b`、`vendor_name=customize` 默认值 | 手点 IDE 时少配 |
| 5 | 关掉 `CMP0177` 开发警告 | CMake 4.x 刷屏 |

> 重新跑 `msopgen gen` 会覆盖 `CMakeLists.txt`，到时候把上面那行 `include` 补回去。
> 兜底只覆盖 configure 阶段；编 kernel 的 `asc_opc` / `ccec` 属于 build 阶段，仍要 source 过环境。

### 7.5 收图纸

```bash
mkdir -p op_desc && cp ../hello_world.json op_desc/
```

骨架随时可重新生成，描述文件是"图纸"，必须进仓库。

## 8. 写三个文件

### 8.1 `op_kernel/hello_world_tiling.h`

```cpp
#ifndef HELLO_WORLD_TILING_H
#define HELLO_WORLD_TILING_H
#include <cstdint>

struct HelloWorldTilingData {
    uint32_t totalLength;   // 输入元素总数
    uint32_t tileNum;       // 每核切几块
};
#endif
```

### 8.2 `op_host/hello_world.cpp`

```cpp
#include "../op_kernel/hello_world_tiling.h"
#include "register/op_def_registry.h"

namespace optiling {
const uint32_t BLOCK_DIM = 8;   // 用 8 个 AI Core
const uint32_t TILE_NUM = 8;    // 每核再切 8 块

static ge::graphStatus TilingFunc(gert::TilingContext* context)
{
    HelloWorldTilingData* tiling = context->GetTilingData<HelloWorldTilingData>();
    uint32_t totalLength =
        static_cast<uint32_t>(context->GetInputShape(0)->GetOriginShape().GetShapeSize());

    tiling->totalLength = totalLength;
    tiling->tileNum = TILE_NUM;

    // 不设长度，kernel 会收到 0 字节 tiling，输出全是 0
    context->GetRawTilingData()->SetDataSize(sizeof(HelloWorldTilingData));

    context->SetBlockDim(BLOCK_DIM);
    context->GetWorkspaceSizes(1)[0] = 0;
    return ge::GRAPH_SUCCESS;
}
}

namespace ge {
static ge::graphStatus InferShape(gert::InferShapeContext* context)
{
    *context->GetOutputShape(0) = *context->GetInputShape(0);
    return GRAPH_SUCCESS;
}

static ge::graphStatus InferDataType(gert::InferDataTypeContext* context)
{
    context->SetOutputDataType(0, context->GetInputDataType(0));
    return GRAPH_SUCCESS;
}
}

namespace ops {
class HelloWorld : public OpDef {
public:
    explicit HelloWorld(const char* name) : OpDef(name)
    {
        this->Input("x")
            .ParamType(REQUIRED).DataType({ge::DT_FLOAT16})
            .Format({ge::FORMAT_ND}).UnknownShapeFormat({ge::FORMAT_ND});
        this->Output("y")
            .ParamType(REQUIRED).DataType({ge::DT_FLOAT16})
            .Format({ge::FORMAT_ND}).UnknownShapeFormat({ge::FORMAT_ND});

        this->SetInferShape(ge::InferShape).SetInferDataType(ge::InferDataType);
        this->AICore().SetTiling(optiling::TilingFunc);
        this->AICore().AddConfig("ascend910b");   // 必须与 CMakePresets 的 ASCEND_COMPUTE_UNIT 一致
    }
};
OP_ADD(HelloWorld);
}
```

### 8.3 `op_kernel/hello_world.cpp`

```cpp
#include "kernel_operator.h"
#include "hello_world_tiling.h"

constexpr int32_t BUFFER_NUM = 2;

class KernelHelloWorld {
public:
    __aicore__ inline KernelHelloWorld() {}

    __aicore__ inline void Init(GM_ADDR x, GM_ADDR y, uint32_t totalLength, uint32_t tileNum)
    {
        this->blockLength = totalLength / AscendC::GetBlockNum();
        this->tileNum = tileNum;
        this->tileLength = this->blockLength / tileNum / BUFFER_NUM;

        xGm.SetGlobalBuffer((__gm__ DTYPE_X*)x + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);
        yGm.SetGlobalBuffer((__gm__ DTYPE_Y*)y + this->blockLength * AscendC::GetBlockIdx(), this->blockLength);

        pipe.InitBuffer(inQueueX, BUFFER_NUM, this->tileLength * sizeof(DTYPE_X));
        pipe.InitBuffer(outQueueY, BUFFER_NUM, this->tileLength * sizeof(DTYPE_Y));
        pipe.InitBuffer(tmpBuf, this->tileLength * sizeof(float));
    }

    __aicore__ inline void Process()
    {
        int32_t loopCount = this->tileNum * BUFFER_NUM;
        for (int32_t i = 0; i < loopCount; i++) {
            CopyIn(i); Compute(i); CopyOut(i);
        }
    }

private:
    __aicore__ inline void CopyIn(int32_t progress)
    {
        AscendC::LocalTensor<DTYPE_X> xLocal = inQueueX.AllocTensor<DTYPE_X>();
        AscendC::DataCopy(xLocal, xGm[progress * this->tileLength], this->tileLength);
        inQueueX.EnQue(xLocal);
    }

    __aicore__ inline void Compute(int32_t progress)
    {
        AscendC::LocalTensor<DTYPE_X> xLocal = inQueueX.DeQue<DTYPE_X>();
        AscendC::LocalTensor<DTYPE_Y> yLocal = outQueueY.AllocTensor<DTYPE_Y>();
        AscendC::LocalTensor<float> fp = tmpBuf.Get<float>();

        // fp16 -> fp32 -> +1 -> fp16，避免 fp16 精度问题
        AscendC::Cast(fp, xLocal, AscendC::RoundMode::CAST_NONE, this->tileLength);
        AscendC::Adds(fp, fp, static_cast<float>(1.0), this->tileLength);
        AscendC::Cast(yLocal, fp, AscendC::RoundMode::CAST_RINT, this->tileLength);

        outQueueY.EnQue(yLocal);
        inQueueX.FreeTensor(xLocal);
    }

    __aicore__ inline void CopyOut(int32_t progress)
    {
        AscendC::LocalTensor<DTYPE_Y> yLocal = outQueueY.DeQue<DTYPE_Y>();
        AscendC::DataCopy(yGm[progress * this->tileLength], yLocal, this->tileLength);
        outQueueY.FreeTensor(yLocal);
    }

private:
    AscendC::TPipe pipe;
    AscendC::TQue<AscendC::QuePosition::VECIN, BUFFER_NUM> inQueueX;
    AscendC::TQue<AscendC::QuePosition::VECOUT, BUFFER_NUM> outQueueY;
    AscendC::TBuf<AscendC::QuePosition::VECCALC> tmpBuf;
    AscendC::GlobalTensor<DTYPE_X> xGm;
    AscendC::GlobalTensor<DTYPE_Y> yGm;
    uint32_t blockLength, tileNum, tileLength;
};

extern "C" __global__ __aicore__ void hello_world(GM_ADDR x, GM_ADDR y, GM_ADDR workspace, GM_ADDR tiling)
{
    REGISTER_TILING_DEFAULT(HelloWorldTilingData);
    GET_TILING_DATA(tilingData, tiling);

    KernelHelloWorld op;
    op.Init(x, y, tilingData.totalLength, tilingData.tileNum);
    op.Process();
}
```

## 9. 命令行构建（先跑通这一步）

```bash
cd ~/projects/HelloWorld
source ~/Ascend/cann-9.0.0/set_env.sh
bash build.sh
```

期望结尾：

```text
Self-extractable archive "custom_opp_ubuntu_aarch64.run" successfully created.
```

产物 `build_out/custom_opp_ubuntu_aarch64.run`（实测约 370K，4 秒左右，含 kernel 编译）。
云端老系统加载失败时改用 GCC 12：`CC=gcc-12 CXX=g++-12 bash build.sh`。

`build.sh` 做的事：检查 `ASCEND_HOME_PATH` → `cmake --preset=default -B build_out` → `cmake --build build_out --target binary package`。

| 目录 | 谁生成 | 内容 |
|---|---|---|
| `build_out/` | `build.sh` | 完整构建 + `.run` |
| `cmake-build-debug/` | CLion | host 编译产物、`clion-Debug-log.txt` |

两个都进 `.gitignore`。

## 10. 转 CLion 远程开发

### 10.1 首次接入

1. Mac 上的 CLion → 欢迎页 **远程开发** → **SSH** → `+` 新建连接：主机 IP、用户 `lht`、密码/密钥。
2. 首次会自动下载后端到虚机 `~/.cache/JetBrains/RemoteDev/dist/`（约 5.7G，网络差时 10 分钟以上，失败会重试 3 次）。
3. 连上后选择项目路径 `~/projects/HelloWorld` → 打开。

卡在 `Downloading IDE Backend` 时，先在虚机上确认出网：

```bash
curl -I https://download.jetbrains.com | head -1     # 期望 200/302
```

### 10.2 只改生成器

CLion 用**自己的 CMake profile** 重新 configure，不读 `CMakePresets.json`；环境变量已由 7.4 的兜底解决，
所以只剩一处必须手动改。

`⌘,` 打开设置 → `构建、执行、部署` → `CMake` → 选中 `Debug` → **生成器** 改为 `Unix Makefiles`。
（英文：Settings → Build, Execution, Deployment → CMake → Generator。）

原因：CANN 的 cmake 模块写死 `--build-tool=$(MAKE)`，Ninja 解析报 `bad $-escape`（BUG 5）。

### 10.3 重置缓存

`工具` → `CMake` → **重置缓存并重新加载项目**（Tools → CMake → Reset CMake Cache and Reload Project）。
找不到菜单就 `⌘⇧A` 查找操作，输入 `重置缓存`。等价做法：

```bash
rm -rf ~/projects/HelloWorld/cmake-build-debug
```

### 10.4 成功标志

```text
.../bin/cmake -DCMAKE_BUILD_TYPE=Debug -G "Unix Makefiles" -S .../HelloWorld -B .../cmake-build-debug
-- CANN 环境兜底：ASCEND_CANN_PACKAGE_PATH=/home/lht/Ascend/cann-9.0.0   # 只有没配环境变量时才出现
-- Opbuild generating sources - done
-- Configuring done (0.6s)
-- Generating done (0.0s)
```

```bash
grep -c "CMake Error" ~/projects/HelloWorld/cmake-build-debug/CMakeFiles/clion-Debug-log.txt   # 0
grep "ASCEND_CANN_PACKAGE_PATH" ~/projects/HelloWorld/cmake-build-debug/CMakeCache.txt          # 有真实路径
grep "CMAKE_GENERATOR:INTERNAL" ~/projects/HelloWorld/cmake-build-debug/CMakeCache.txt          # Unix Makefiles
```

### 10.5 补全范围

| 文件 | 补全/跳转 | 原因 |
|---|---|---|
| `op_host/*.cpp` | 正常 | 参与 CMake 编译，命令里有 `<CANN>/include` |
| `framework/*.cc` | 正常 | 同上 |
| `op_kernel/*.cpp` | **没有** | kernel 由 `ccec` 编译，编译命令在 build 阶段现生成，CMake 里没有 |

kernel 的红波浪线不要管。为什么救不回来：用 g++ 加上全部 include 解析同一份文件，实测报 **2709 个 error**
（`__ubuf__`、`__inout_pipe__`、`half`、`PIPE_V` 等 ccec 语言扩展），CLion 需要同一套头文件，结论一样。
kernel 里查 API 用这几条：

```bash
T=$ASCEND_HOME_PATH/compiler/tikcpp/tikcfw/interface
grep -n "class TQue"            $T/kernel_tpipe.h                # 162
grep -n "class GlobalTensor"    $T/kernel_tensor.h               # 234
grep -n "inline void DataCopy(" $T/kernel_operator_data_copy_intf.h | head -3
```

### 10.6 两种终端都能用

| | 位置 | 环境 | 用法 |
|---|---|---|---|
| 内置 Terminal | IDE 底部 `终端` | 交互式 shell，`.bashrc` 生效 | 直接 `bash build.sh`、`git`、`msopgen` |
| 外部 SSH | 自己的终端 | `ssh lht@<ip>` 后同样加载 `.bashrc` | 同上 |

进任何一个终端先自检：

```bash
echo $ASCEND_HOME_PATH && which msopgen && which git
```

为空说明 `.bashrc` 没生效，临时补 `source ~/Ascend/cann-9.0.0/set_env.sh`。

### 10.7 三条红线

1. 生成器别用 Ninja（BUG 5）。
2. 别在 IDE 里构建 `package`：CPack 在该环境下报 `preinstall` 失败，出包统一走 `build.sh`。
3. 别在 IDE 里构建 `binary`：缺 build 阶段 `PATH`，报 `asc_opc: command not found`（BUG 7）。

## 11. 日常循环

1. IDE 改代码（`op_host` / `op_kernel` 三个文件）。
2. 快速查语法：`⌘F9` 构建（`all` 目标，只编 host，秒级）。
3. 真验证（含 kernel、出包）：内置终端 `bash build.sh`。
4. 上云：`git push` → 服务器 `git pull` + `build.sh` + 装包（第 13 节）。

## 12. Git：只提交源码

### 12.1 `.gitignore`

```gitignore
# 构建产物（build.sh 用 build_out，CLion 用 cmake-build-*）
build_out/
cmake-build-*/
_CPack_Packages/

# IDE 工程文件（远程开发自动生成，路径因机器而异）
.idea/
*.iml

# 编译/打包产物与临时文件
*.run
*.run.json
*.log
*.o
*.so
__pycache__/
.DS_Store
```

### 12.2 进 / 不进

| 进仓库（16 个） | 说明 |
|---|---|
| `README.md` `build.sh` `CMakeLists.txt` `CMakePresets.json` `.gitignore` | 工程与构建定义 |
| `cmake/cann_env.cmake` | 环境兜底，IDE 与服务器都要用 |
| `op_host/{CMakeLists.txt, hello_world.cpp}` | host 侧 |
| `op_kernel/{CMakeLists.txt, hello_world.cpp, hello_world_tiling.h}` | kernel 侧 |
| `framework/CMakeLists.txt` + `framework/tf_plugin/*` | 模板自带 |
| `op_desc/hello_world.json` | 算子描述文件（图纸） |
| `test/main.cpp` | aclnn 调用示例（上云验证） |

| 不进仓库 | 原因 |
|---|---|
| `build_out/` `cmake-build-*/` `_CPack_Packages/` | 构建产物，可重新生成 |
| `.idea/` `*.iml` | IDE 本地状态，每台机器不同 |
| `*.run` `*.run.json` `*.log` `*.o` `*.so` `__pycache__/` | 产物与临时文件 |

已进过仓库的产物要清索引（不删文件）：`git rm -r --cached build_out cmake-build-debug .idea`。

### 12.3 初始化与校验

```bash
cd ~/projects/HelloWorld
git init
git add -A
git status --short          # 只应出现上述 16 个文件
git commit -m "HelloWorld: first ascend c operator"
git branch -M main
git remote add origin git@github.com:<账号>/HelloWorld.git   # 或 https + PAT
git push -u origin main
```

干净标准：`git status --short` 除正在改的源码外无输出；`git status --ignored --short` 只看到
`.idea/`、`build_out/`、`cmake-build-debug/`。

### 12.4 换机器不用改路径

7.3 已经写成 `"version": 3` + `"$env{ASCEND_HOME_PATH}"`，服务器 `source set_env.sh` 后即可构建：

```bash
grep -A2 ASCEND_CANN_PACKAGE_PATH CMakePresets.json   # 应看到 $env{ASCEND_HOME_PATH}
```

旧仓库若写死本机路径，到服务器要改，否则报 `register/op_def_registry.h: No such file`（BUG 8）：

```bash
sed -i "s#/home/lht/Ascend/cann-9.0.0#/usr/local/Ascend/cann-9.0.0#" CMakePresets.json
```

## 13. 服务器部署与运行

```bash
git clone https://github.com/<账号>/HelloWorld.git && cd HelloWorld
source /usr/local/Ascend/cann-9.0.0/set_env.sh
bash build.sh
cd build_out && chmod +x custom_opp_*.run && ./custom_opp_*.run     # 装进 $ASCEND_OPP_PATH/vendors

ls $ASCEND_HOME_PATH/opp/vendors/customize/op_api/include/aclnn_hello_world.h   # 装好了
```

最小 aclnn 调用程序在 `test/main.cpp`（输入 fp16 全 1.0，期望输出全 2.0）：

```bash
g++ -std=c++17 test/main.cpp -o test/hello \
  -I$ASCEND_HOME_PATH/include \
  -I$ASCEND_HOME_PATH/opp/vendors/customize/op_api/include \
  -L$ASCEND_HOME_PATH/lib64 \
  -L$ASCEND_HOME_PATH/opp/vendors/customize/op_api/lib \
  -lcust_opapi -lascendcl -lnnopbase -lpthread

export LD_LIBRARY_PATH=$ASCEND_HOME_PATH/lib64:$ASCEND_HOME_PATH/opp/vendors/customize/op_api/lib:$LD_LIBRARY_PATH
./test/hello      # 期望 PASS
```

`-lnnopbase` 不能漏：`aclCreateTensor` 在这个库里（BUG 9）。

## 14. BUG 档案

| # | 现象 | 根因 | 修法 | 节 |
|---|---|---|---|---|
| 1 | `msopgen` 报 `should not be written by user group or others` | CANN 安全检查：目录对 group 可写 | 工作目录 `chmod 700`、文件 `chmod 600`，别放 `/tmp` | 7.2 |
| 2 | `Ops info ini ... missing` / `Target path not found: .../binary/ascend910` | `ASCEND_COMPUTE_UNIT` 与 `AddConfig` 不一致，ini 生成在 910b、构建找 910 | 两处统一 `ascend910b` 并自检 | 7.3 |
| 3 | `Could not find a package configuration file provided by "ASC"` | IDE 后端没有 `CMAKE_PREFIX_PATH` | `cmake/cann_env.cmake` 兜底 | 7.4 |
| 4 | `ERROR: ASCEND_HOME_PATH environment variable is not set!` | `asc-config.cmake` 强制要求该变量 | 同上 | 7.4 |
| 5 | `ninja: error: build.ninja:NNN: bad $-escape` | CANN 用了 `--build-tool=$(MAKE)`，Ninja 不支持 | 生成器改 `Unix Makefiles` | 10.2 |
| 6 | `opbuild ops error: ... libgraph.so: cannot open shared object file` | configure 阶段缺 `LD_LIBRARY_PATH` | 兜底补上（或配 Environment） | 7.4 |
| 7 | `asc_opc: command not found` | 在没 source 的环境编 `binary` | 用 `bash build.sh` | 10.7 |
| 8 | `register/op_def_registry.h: No such file` | presets 写死了别的机器的 CANN 路径 | 改 `$env{ASCEND_HOME_PATH}` 或 `sed` | 12.4 |
| 9 | 链接报 `undefined reference to aclCreateTensor` | 少了 `-lnnopbase` | 编译命令补上 | 13 |
| 10 | `CMake Warning (dev) ... CMP0177 is not set` | CMake 3.31+ 对 CANN 的 `install()` 报开发警告 | 兜底已关 | 7.4 |
| 11 | `scp` / `git` 远程命令偶发失败、输出混入 CANN 信息 | `.bashrc` 无条件 source | 加交互式判断 | 5 |
| 12 | 运行输出全 0 | tiling 长度没设 | `SetDataSize` 不能漏 | 8.2 |
| 13 | 只有一部分数据正确 | 多核偏移错 | 检查 `GetBlockIdx()` 与 `blockLength` | 8.3 |
| 14 | `op_kernel` 里"找不到要转到的声明" | kernel 由 `ccec` 编，g++/CLion 解析该头文件报 2709 错 | 结构性限制；`build.sh` 验语法、`grep` 查声明 | 10.5 |

排查顺序：configure 阶段看 CMake 输出 → 构建阶段看 `make` 输出 → 运行阶段看 `./test/hello`。

## 15. 全新虚机验收清单

按顺序执行，每行都能独立判定。全部通过即说明环境可复现。

| # | 命令 | 期望 |
|---|---|---|
| 1 | `uname -m` | `aarch64` |
| 2 | `df -h $HOME \| tail -1` | 可用 ≥ 20G |
| 3 | `systemctl is-active ssh` | `active` |
| 4 | `source ~/Ascend/cann-9.0.0/set_env.sh && echo $ASCEND_HOME_PATH` | 非空 |
| 5 | `python3 -c "import acl; print('acl ok')"` | `acl ok` |
| 6 | `which msopgen` | 有输出 |
| 7 | `cd ~/projects/HelloWorld && bash build.sh` | 结尾 `successfully created` |
| 8 | `ls build_out/*.run` | 存在 |
| 9 | `grep -c "CMake Error" cmake-build-debug/CMakeFiles/clion-Debug-log.txt` | `0` |
| 10 | `grep -c "ASCEND_CANN_PACKAGE_PATH" cmake-build-debug/CMakeCache.txt` | `2` |
| 11 | `grep CMAKE_GENERATOR:INTERNAL cmake-build-debug/CMakeCache.txt` | `Unix Makefiles` |
| 12 | `git status --short` | 只有你正在改的源码 |
| 13 | 服务器上 `./test/hello` | `PASS` |

一次跑完前 11 项：

```bash
cd ~/projects/HelloWorld && source ~/Ascend/cann-9.0.0/set_env.sh
echo "arch=$(uname -m) free=$(df -h $HOME | awk 'NR==2{print $4}') ssh=$(systemctl is-active ssh)"
echo "cann=$ASCEND_HOME_PATH acl=$(python3 -c 'import acl;print("ok")' 2>&1 | tail -1) msopgen=$(command -v msopgen)"
bash build.sh >/tmp/b.log 2>&1 && tail -1 /tmp/b.log
errs=$(grep -c 'CMake Error' cmake-build-debug/CMakeFiles/clion-Debug-log.txt 2>/dev/null || true)
cann=$(grep -c 'ASCEND_CANN_PACKAGE_PATH' cmake-build-debug/CMakeCache.txt 2>/dev/null || true)
gen=$(grep -o 'Unix Makefiles' cmake-build-debug/CMakeCache.txt | head -1)
echo "errs=$errs cann_cache=$cann gen=$gen"
```

## 16. 附录

环境变量（`set_env.sh` 设置，缺一不可）：

| 变量 | 作用 | 缺了会怎样 |
|---|---|---|
| `ASCEND_HOME_PATH` | CANN 根目录 | `asc-config.cmake` 直接 `FATAL_ERROR` |
| `CMAKE_PREFIX_PATH` | 找 `asc-config.cmake` | `Could not find ... "ASC"` |
| `LD_LIBRARY_PATH` | opbuild 等二进制运行 | `libgraph.so` 找不到 |
| `PATH` | `msopgen` / `asc_opc` / `ccec` | 工具 not found |
| `PYTHONPATH` | CANN python 模块 | `import acl` 失败 |
| `ASCEND_OPP_PATH` | 算子包装载目录 | 包装错位置 |

常用命令：

```bash
source ~/Ascend/cann-9.0.0/set_env.sh          # 终端环境
bash build.sh                                 # 构建 + 出包
grep -c "CMake Error" cmake-build-debug/CMakeFiles/clion-Debug-log.txt
git status --short                            # 干净目录检查
```

备注：

- 本机没有 NPU，`npu-smi`、`acl.get_soc_name()`、`run.sh` 都会失败，属正常，跑算子上云。
- 24.04 若提示系统不支持，安装命令后加 `--force` 重试。
- `gcc-12` 只在云端老系统加载失败时才是必须的；本机默认 GCC 13 能编过。
- CLion 内置 CMake 4.0.2、系统 CMake 3.28.3 都能用（`cmake_minimum_required(3.16)`）。
- 想要 kernel 的类型补全只能换 VS Code + CANN 的 `c_cpp_properties.json` 方案，CLion 模拟不了 ccec。
