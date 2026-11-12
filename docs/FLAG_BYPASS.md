# Ascend 910B4 / 910C：GM Flag bypass DCache

## 修改範圍

1. Cube producer 和 Vector consumer 共用 `kernel/flag_transport.h`：`FlagStore`、`FlagLoad`、`FlagSpinGE`。
2. 固定使用 CANN 9.1.x 的 `AscendC::WriteGmByPassDCache<int32_t>` / `AscendC::ReadGmByPassDCache<int32_t>`；不再對 **producer / consumer flags** 逐次執行 `dcci`。
3. Vector 的 look-ahead 使用同一個 `FlagLoad`，不再刷新 producer flag 的 DCache line；若 look-ahead 已讀到正確的 monotonic generation 並預載第一筆 partial，下一輪直接使用，不再重複輪詢該 flag。
4. `dav-2201` 的 bypass store 對目標地址有額外限制：有效寫入地址必須落在每個 128-byte 區域的前 32 bytes。現在兩組 flag 的配置起始位址都強制向上對齊 128B，且 Flag 槽間距 512B，因此讀寫的 `int32_t` 均位於每個區域的起點。以 `g_prodFRaw` / `g_consFRaw` 保留原始指標供 `aclrtFree`。
5. Vector `producer flag -> dependent MTE2 DataCopy` 和 `consumer flag -> next cube Matmul` 新增 **一次性** S→MTE2 event fence。Vector 消費完 GM 最後一筆 `DataCopy` 後的 MTE2→S release fence 維持原狀。
6. 原本的 monotonic generation、cube one-band-late publish、BUFS=4、VEC2X=2、per-buffer back-pressure、host stream ordering 全部保留。非旗標的 `myHit` DCache 刷新仍然保留。

## 不能移除的等待

**Bypass DCache ≠ 無等待、無 HBM 延遲。** 它避免的是每次 flag 讀取都需要 DCache invalidate 的額外負擔，但每次讀取仍需要從 GM 記憶體路徑取得資料；如果對方尚未完成部分矩陣寫入或讀取，就仍需等待對應 flag。這是防止 ring slot 資料覆蓋的必要條件。官方亦提醒 bypass 方式可能比 DCache 方式慢，請以真機數據判定。

## 在 CANN 9.1.0 上編譯（無需新增參數）

```bash
# 原專案指定 CANN 9.1.0（scripts/build.sh）
source /usr/local/Ascend/cann-9.1.0/set_env.sh
export ASC_MODULES=/usr/local/Ascend/cann-9.1.0/aarch64-linux/tikcpp/ascendc_kernel_cmake/asc_modules
make kernel
```

或沿用原本的設定，例如 `make RANK=128 BACKOFF=32 kernel`。編譯完成的 Cube / Vector flags **一律**使用 bypass DCache；不需要、也不支援 `FLAG_BYPASS`、`PRL_FLAG_BYPASS` 或 `BYPASS_API_NEW` 等切換參數。程式固定採用 CANN 9.1 的 `ByPass` API 拼法；若日後更換不相容的新 SDK，應直接更新程式碼，而非用編譯開關變換路徑。

若需要與舊版比較效能，請使用修改前的原始專案作為獨立 baseline。`make kernel` 會覆寫前一次的 `kernel/build` / `build/libpearl_hlc2.so`，請避免覆蓋執行中的核心。`BACKOFF` 仍為原本的輪詢退避設定，不影響 bypass 與否。

## 驗證

1. 必須在實際 **910B4／910C**、實際 CANN 9.1.0 編譯，並做 small/large `nbands`、連續多 strip、跨 scan reset、帶/不帶 transcript、混合 mining 工作負載的 correctness 測試。
2. 對照原版與 bypass 版的結果、hang log（原來有 30 秒 timeout）、端到端 iter/s、flag 等待區段和 L2/GM/HBM 數據。
3. 若 bypass 比原版本慢，需回到獨立的舊版程式碼比較與調整；此版本不提供 DCache 模式切換。此補丁沒有宣稱已驗證 device 執行或一定會加速。

## 參考 API（CANN 9.1.X）

- https://www.hiascend.com/document/detail/en/CANNCommunityEdition/910/API/ascendcopapi/docs/en/api/SIMD-API/basic_api/scalar_compute/ReadGmByPassDCache_ISASI.md
- https://www.hiascend.com/document/detail/en/CANNCommunityEdition/910/API/ascendcopapi/docs/en/api/SIMD-API/basic_api/scalar_compute/WriteGmByPassDCache_ISASI.md
- https://www.hiascend.com/document/detail/zh/CANNCommunityEdition/910/API/ascendcopapi/docs/api/SIMD-API/%E5%9F%BA%E7%A1%80API/%E7%BC%93%E5%AD%98%E6%8E%A7%E5%88%B6/DataCacheCleanAndInvalid.md
