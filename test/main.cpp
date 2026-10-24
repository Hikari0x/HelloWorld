// 最小 aclnn 调用：y = x + 1，输入 fp16 全 1.0，期望输出全 2.0（0x4000）
// 只在装了算子包的 NPU 机器上运行；本地只能编译，不能跑
#include <cstdio>
#include <cstdint>
#include <vector>
#include "acl/acl.h"
#include "aclnn_hello_world.h"

#define CHECK(expr)                                                     \
    do {                                                                \
        auto _r = (expr);                                               \
        if (_r != 0) { printf("FAIL %s ret=%d\n", #expr, (int)_r); return 1; } \
    } while (0)

int main()
{
    CHECK(aclInit(nullptr));
    CHECK(aclrtSetDevice(0));

    aclrtStream stream = nullptr;
    CHECK(aclrtCreateStream(&stream));

    int64_t shape[2] = {8, 2048};
    int64_t total = 16384;
    size_t bytes = total * sizeof(uint16_t);

    std::vector<uint16_t> hostIn(total, 0x3C00);   // fp16 的 1.0
    std::vector<uint16_t> hostOut(total, 0);

    void* devIn = nullptr;
    void* devOut = nullptr;
    CHECK(aclrtMalloc(&devIn, bytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK(aclrtMalloc(&devOut, bytes, ACL_MEM_MALLOC_HUGE_FIRST));
    CHECK(aclrtMemcpy(devIn, bytes, hostIn.data(), bytes, ACL_MEMCPY_HOST_TO_DEVICE));

    auto xDesc = aclCreateTensor(shape, 2, ACL_FLOAT16, nullptr, 0, ACL_FORMAT_ND, shape, 2, devIn);
    auto yDesc = aclCreateTensor(shape, 2, ACL_FLOAT16, nullptr, 0, ACL_FORMAT_ND, shape, 2, devOut);

    uint64_t wsSize = 0;
    aclOpExecutor* executor = nullptr;
    CHECK(aclnnHelloWorldGetWorkspaceSize(xDesc, yDesc, &wsSize, &executor));

    void* ws = nullptr;
    if (wsSize > 0) {
        CHECK(aclrtMalloc(&ws, wsSize, ACL_MEM_MALLOC_HUGE_FIRST));
    }

    CHECK(aclnnHelloWorld(ws, wsSize, executor, stream));
    CHECK(aclrtSynchronizeStream(stream));
    CHECK(aclrtMemcpy(hostOut.data(), bytes, devOut, bytes, ACL_MEMCPY_DEVICE_TO_HOST));

    int bad = 0;
    for (int64_t i = 0; i < total; i++) {
        if (hostOut[i] != 0x4000) { bad++; }
    }
    printf("total=%lld bad=%d out[0]=0x%04x\n", (long long)total, bad, hostOut[0]);
    printf("%s\n", bad == 0 ? "PASS" : "FAIL");

    aclrtFree(ws);
    aclrtFree(devIn);
    aclrtFree(devOut);
    aclrtDestroyStream(stream);
    aclrtResetDevice(0);
    aclFinalize();
    return bad == 0 ? 0 : 1;
}
