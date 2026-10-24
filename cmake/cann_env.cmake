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
# 等价于 cmake -Wno-dev，只影响“开发警告”，不影响真正的错误。
set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON CACHE BOOL "suppress CMake developer warnings" FORCE)
