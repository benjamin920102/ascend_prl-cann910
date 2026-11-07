# CANN 9.1.0 编译 → CANN 8.5.0 Runtime 移植版

> **状态：已直接改写工程源码，但尚未在 CANN 9.1.0 / Ascend 910B4 上编译及运行验证。不能保证 9.1 fatbin 被 8.5 Runtime 接受。**

## 改动

- `kernel/pearl_hlc2_cube_device.cpp`：从原 `.asc` 原样提取 Cube Device 实现，只改为 `extern "C"` 导出稳定入口名称。
- `kernel/pearl_hlc2_vec_device.cpp`：原样提取 Vector Device 实现，只改为 `extern "C"` 导出入口名称；原有 Vector64、DCache、旗标语义不动。
- `kernel/pearl_hlc2_host.cpp`：原 Host 资源、tiling、Queue、stream、event、内存、B repack 均保留；Cube `< < <...> > >` 改为显式 Launch。
- `kernel/pearl_kernel_loader.{h,cpp}`：在 `pearl_init` 之后、分配资源前显式加载 `pearl_cube.o` / `pearl_vec.o`，取得两个 Kernel function handle；通过 8.5 有的 `aclrtKernelArgsInit/Append/Finalize + aclrtLaunchKernelWithConfig` 投递异步任务；在所有 stream 同步之后卸载 Kernel。
- `kernel/pearl_build_config.h.in`：CMake 配置产生一个共享的 `pearl_build_config_generated.h`，确保各 RANK 的 Host/Device 编译参数完全一致。
- `kernel/CMakeLists.txt`：`ascendc_fatbin_library` 编 Device；`add_library(... SHARED)` 仅编普通 Host C++，阻止 9.1 编译器生成不兼容的注册函数依赖。
- `Makefile`：把两个 fatbin `.o` 与 `.so` 统一复制到 `build/`，不再只复制 `.so`。

## 在 CANN 9.1.0 编译机执行

```bash
source /usr/local/Ascend/cann-9.1.0/set_env.sh
export ASC_MODULES=/usr/local/Ascend/cann-9.1.0/aarch64-linux/tikcpp/ascendc_kernel_cmake/asc_modules
export ASCENDC_CMAKE_DIR=/usr/local/Ascend/cann-9.1.0/aarch64-linux/tikcpp/ascendc_kernel_cmake
make RANK=128 kernel
# 首先应看到三个文件：
ls -lh build/libpearl_hlc2.so build/pearl_cube.o build/pearl_vec.o
readelf -Ws build/libpearl_hlc2.so | grep -E 'rtRegisterFuncSymbol|rtRegisterVariable|aclrtGetFuncBySymbol|rtFunctionGetMetaInfoSize'
# 应无这四个符号
make RANK=128 proof miner
```

**注意**：如实际 toolkit 中 `ascendc.cmake` 不在上面的候选目录，请用 `find /usr/local/Ascend/cann-9.1.0 -name ascendc.cmake` 找到正确目录，设置 `ASCENDC_CMAKE_DIR`。

**CMake 注意**：移植包使用 `find_package(ASC)` 与 `ascendc_fatbin_library()` 组合。若实际 9.1 SDK 的 CMake 版本不允许组合，需要以 9.1 Toolkit 实机反馈调整 CMake 配置。本机没有 CANN 工具链，因此没有宣称编译成功。

## 部署到 Runtime 8.5.0 主机

```bash
# 将这三个文件放在同一个 build/ 目录，覆盖前备份老文件。
# build/libpearl_hlc2.so
# build/pearl_cube.o
# build/pearl_vec.o

source /usr/local/Ascend/cann-8.5.0/set_env.sh
bash scripts/check_runtime850.sh
# 输出不应包含 undefined symbol / not found
```

Kernel 默认从 `libpearl_hlc2.so` 所在目录自动找 `pearl_cube.o`、`pearl_vec.o`。如果在另外路径，设置 `PEARL_KERNEL_DIR=/absolute/kernel/path`。

## 关键风险和验收

1. **Host ELF**：`readelf -Ws` 不应包含四个 9.1 新 stub API；`ldd -r` 在 8.5 主机必须无 undefined。
2. **Device binary**：8.5 的 `aclrtBinaryLoadFromFile` 必须能加载 9.1 编译的 `.o`。如失败，Host 修改不能单独修正 Device ISA / binary metadata 不兼容。
3. **Kernel 入口**：`aclrtBinaryGetFunction(..., "pearl_hlc2_cube")`、`"pearl_hlc2_vec"` 必须成功。入口名通过 `extern "C"` 固定；如编译器仍重命名，需以 fatbin metadata 实查。
4. **参数 ABI**：Cube 带 `TCubeTiling` 完整 by-value 结构，按顺序 `Append`；不手工拼接 260 字节。特别要核对 8.5 解析 9.1 kernel 的 metadata 后能提供正确布局。
5. **计算一致**：必须对比 9.0 Golden Binary 的已知输入/输出或完整矿池 nonce、BLAKE3 Proof；再测 throughput。
6. **性能**：每次 submission 新建参数 handle 可能增大 CPU Launch 开销；先测正确性，再考虑缓存与参数更新。
7. **未验证**：本文不表示已运行 NPU/编译测试，工程仍属于待部署验证的源码移植。

## 官方依据

- CANN 9.1，Custom Operator Loading，非混合编程：https://www.hiascend.com/doc_center/source/en/CANNCommunityEdition/910/others/acldevg/runtime_doc_dev_0019.html
- CANN 8.5，Runtime Kernel Loading API：https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/850/API/appdevgapi/aclcppdevg_03_1825.html
- CANN 9.1 `ascendc_fatbin_library`：https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910beta2/programug/Ascendcopdevg/docs/guide/%E7%BC%96%E7%A8%8B%E6%8C%87%E5%8D%97/%E9%99%84%E5%BD%95/%E7%AE%80%E6%98%93%E8%87%AA%E5%AE%9A%E4%B9%89%E7%AE%97%E5%AD%90%E5%B7%A5%E7%A8%8B.md
