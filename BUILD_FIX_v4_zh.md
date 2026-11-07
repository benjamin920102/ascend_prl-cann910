# CANN 9.1.0 GitHub Actions：Host 標頭路徑修正 v4

## 本次錯誤

- `pearl_kernel_loader.cpp: acl/acl.h: No such file or directory`
- `pearl_hlc2_host.cpp: kernel_tiling/kernel_tiling.h: No such file or directory`

由於 `libpearl_hlc2.so` 改成普通 C++ target，`find_package(ASC)` 與 Device 的 fatbin 編譯規則不一定會把 Host C++ 需要的 CANN include 路徑帶入。因此修改 `kernel/CMakeLists.txt`，新增 `cmake/ResolveCANNHostIncludes.cmake`，逐一核實並導入：

- `acl/acl.h` 的**父目錄**，例如 `$ASCEND_HOME_PATH/include`。
- `tiling/platform/platform_ascendc.h`、`tiling/tiling_api.h` 的**父目錄**。
- `kernel_tiling/kernel_tiling.h` 的**父目錄**，例如 `$ASCEND_HOME_PATH/include/ascendc/highlevel_api` 或 `$ASCEND_HOME_PATH/aarch64-linux/ascendc/include/highlevel_api`。

查找失敗時會在 CMake configure 階段直接報出缺少哪一個標頭與實際搜尋的 Toolkit 根目錄，避免模糊的 C++ `No such file or directory`。

## 使用

直接使用完整 ZIP，重新 GitHub Actions Build。或在 CANN 9.1 編譯機執行：

```bash
source /home/runner/Ascend/cann/set_env.sh
make RANK=128 kernel
```

如仍缺少某個標頭，請在編譯主機執行：

```bash
find "$ASCEND_HOME_PATH" -type f \( -name acl.h -o -name kernel_tiling.h -o -name platform_ascendc.h \) -print 2>/dev/null
```

本版僅修正已確認的 Host header include 問題，不表示已在 CANN 9.1 真正編譯或在 8.5 Runtime 實機驗證成功。
