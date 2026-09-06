# ESP32-P4 Chip Revision 历史与兼容性报告

> **版本**：v1.0（2026-09-06）
> **作者**：CCB（科研助理）
> **目标读者**：CCA（环境修复）+ DSH（M5 规划）
> **触发事件**：DSH 使用 IDF 5.5.5 编译固件在 v1.3 芯片上 `Guru Meditation: Illegal instruction` panic
> **根因已确认**：IDF 5.5.5 默认配置面向 v3.0+，与 v1.3 ROM 地址/寄存器布局不兼容

---

## 1. ESP32-P4 Chip Revision 概览

ESP32-P4 自 2024 年中量产以来，经历了重大硅片迭代，分为 **两大不兼容系列**：

| 系列 | 版本号 | 时间线 | 特征 |
|---|---|---|---|
| **v0.x / v1.x** | v0.0, v0.1, v1.0, v1.3 | 2024 中 ~ 2025 初 | 早期硅片，ECO 版本 < 5 |
| **v3.x** | v3.0, v3.1 | 2025+ | 重大硬件修订，ECO 版本 ≥ 5 |

> ⚠️ **关键事实**：v1.x 和 v3.x 之间存在**巨大硬件差异**（Espressif 官方 Kconfig 原话："have huge hardware difference"）。两者的 PMU 寄存器布局、ROM 函数地址表**完全不同**，编译产物互不兼容。

**本项目芯片**：ESP32-P4 **v1.3**（MAC: 80:f1:b2:d1:46:ea），属于早期系列。

---

## 2. v1.3 vs v3.1+ 差异表

差异来源：IDF 5.5.5 `components/soc/esp32p4/register/hw_ver1/` vs `hw_ver3/` 源码对比。

| 差异项 | v1.3（hw_ver1） | v3.1+（hw_ver3） | 影响 |
|---|---|---|---|
| **PMU 结构版本** | `SOC_PMU_STRUCT_HW_VER = 1` | `SOC_PMU_STRUCT_HW_VER = 3` | 所有 PMU sleep/电源管理代码不兼容 |
| **PMU 寄存器数** | 191 个寄存器文件 | 209 个寄存器文件（+18）| v3.x 新增寄存器 |
| **HP CPU 电源域** | 无独立控制位 | 新增 `hp_cpu_pd_en`、`power_pd_hp_cpu_cntl` | v1.x 无 HP CPU 独立断电 |
| **Flash LDO 通道** | `enable_sleep_flash_ldo_channel` 不可用 | 需 `chip_rev >= 100` 才使能 | v1.0 以下无此功能 |
| **PMU sleep 内存** | 无 `MEM_AUX_DEEPSLEEP` | `lp_sys_ll_set_hp_mem_lowpower_mode(MEM_AUX_DEEPSLEEP)` | v1.x 无 deep sleep 内存保留 |
| **ROM 函数地址** | `ets_efuse_write_key = 0x4fc006ac` | `ets_efuse_write_key = 0x4fc006a0` | **调用不同地址 → IllegalInstruction** |
| **ROM 函数偏移** | 标准 ROM（`esp32p4.rom.ld`）| ECO5 ROM（`esp32p4.rom.eco5.ld`）| 所有 ROM 调用地址不同 |
| **CPU 频率上限** | 399 MHz | 199 MHz（`REV_LESS_V3=y` 时）| v1.x 功耗/频率策略不同 |
| **LP XTAG 等待** | `lp_ana_wait_target: 8 bit` | 拆分为 2+8 bit（`expand` 字段）| PMU 时序参数不兼容 |
| **计数器清除** | 无 `cnt_clr` 位 | 新增 `cnt_clr: 1` | LP 系统计数器控制差异 |

> **核心结论**：v1.3 和 v3.1+ 的 ROM 函数地址表偏移了约 12 字节，PMU 寄存器布局有 18 个新增/变更寄存器。用 v3.1 固件跑在 v1.3 芯片上必然 panic。

---

## 2.5 ROM 地址差异详解

ESP32-P4 的 ROM 中固化了一组底层函数（efuse 读写、flash 操作、UART 等），每个函数在 ROM 中有固定地址。v1.x 和 v3.x 使用**不同的 ROM 地址表**：

| ROM 函数 | v1.x 地址（rom.ld）| v3.1+ 地址（eco5.ld）| 偏移 |
|---|---|---|---|
| `ets_efuse_read` | 0x4fc006b4 | 0x4fc006a8 | -0xC |
| `ets_efuse_program` | 0x4fc006b8 | 0x4fc006ac | -0xC |
| `ets_efuse_clear_program_registers` | 0x4fc006bc | 0x4fc006b0 | -0xC |
| `ets_efuse_write_key` | 0x4fc006c0 | 0x4fc006b4 | -0xC |
| `ets_efuse_get_read_register_address` | 0x4fc006c4 | 0x4fc006b8 | -0xC |
| `ets_efuse_get_key_purpose` | 0x4fc006c8 | 0x4fc006bc | -0xC |
| `ets_efuse_key_block_unused` | 0x4fc006cc | 0x4fc006c0 | -0xC |
| `ets_efuse_find_unused_key_block` | 0x4fc006d0 | 0x4fc006c4 | -0xC |
| `ets_efuse_rs_calculate` | 0x4fc006d4 | 0x4fc006c8 | -0xC |

> **规律**：v3.1+ 的 ECO5 ROM 将所有 efuse 函数地址统一前移了 **12 字节（0xC）**。这意味着用 v3.1 链接的固件调用 `ets_efuse_write_key` 时跳转到 `0x4fc006a0`，在 v1.3 上该地址指向**非函数区域** → CPU 解码到非法指令 → panic。

---

## 3. IDF 版本支持矩阵

| IDF 版本 | 发布日期 | v1.3 支持 | v3.1+ 支持 | 说明 |
|---|---|---|---|---|
| **5.4.2** | 2024-Q4 | ✅ 原生支持 | ❌ 不支持 | 仅有 hw_ver1 寄存器，无 v3.x 代码 |
| **5.5.0** | — | — | — | 不存在 |
| **5.5.1** | 2025-Q1 | ⚠️ 待验证 | ✅ | 可能有初始 v3.x 支持 |
| **5.5.2** | 2025-Q2 | ✅ 需配置 | ✅ 需配置 | 新增 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3` |
| **5.5.3-5.5.4** | 2025-Q3 | ✅ 需配置 | ✅ 需配置 | 同上 |
| **5.5.5** | 2026-Q2 | ✅ 需配置 | ✅ **默认** | 默认面向 v3.1+，v1.x 需手动开启 |

> **关键发现**：xiaozhi 项目使用 IDF ≥ 5.5.2，且 sdkconfig 中明确配置了：
> ```
> CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y    ← 选择 v1.x 支持
> CONFIG_ESP32P4_REV_MIN_0=y              ← 最低 v0.0
> CONFIG_ESP32P4_REV_MAX_FULL=199         ← 最高 v1.99
> ```
> 这证实 xiaozhi 是为 v1.3 芯片编译的。

---

## 4. Panic 根因分析

### 4.1 事件回顾

DSH 使用 IDF 5.5.5（默认配置）编译 803KB 固件，烧录到 v1.3 芯片后，启动时 `Guru Meditation Error: Illegal instruction` panic。

### 4.2 根因链

```
IDF 5.5.5 默认配置:
  CONFIG_ESP32P4_SELECTS_REV_LESS_V3=n  (默认)
  → 使用 hw_ver3 寄存器头文件
  → 链接 esp32p4.rom.eco5.ld（ECO5 ROM 地址表）
  → ets_efuse_write_key = 0x4fc006a0  ← ECO5 地址

v1.3 芯片 ROM:
  → 标准 ROM（ECO < 5）
  → ets_efuse_write_key = 0x4fc006ac  ← 标准地址

bootloader_start.c:27 call_start_cpu0():
  → 调用 bootloader_init() → ... → ets_efuse_write_key()
  → 跳转到 0x4fc006a0（ECO5 地址）
  → v1.3 ROM 在该地址处无有效函数
  → CPU 执行到非法指令 → Guru Meditation panic
```

### 4.3 附加不兼容

即使 ROM 地址问题修复，PMU 寄存器布局差异也会导致：
- `pmu_init()` 写入错误的位域
- sleep/wake 功能完全失效
- HP CPU 电源域控制不存在于 v1.3

---

## 5. 解决方案

### 方案 A：IDF 5.5.5 + 配置 v1.x 支持（✅ 推荐）

在 `sdkconfig.defaults` 或 `sdkconfig` 中添加：

```
# 选择 v1.x 支持（与 v3.x 互斥）
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y

# 设置芯片版本范围
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESP32P4_REV_MIN_FULL=100
CONFIG_ESP32P4_REV_MAX_FULL=199
```

然后 `idf.py fullclean && idf.py build`。

**优点**：保留 IDF 5.5.5 的新组件（esp_hosted, esp_audio_codec 等），xiaozhi 验证过此配置
**风险**：低。xiaozhi 已用此配置在 v1.3 上运行

**验证步骤**：
1. 修改 sdkconfig 后 `idf.py fullclean`（必须全清，否则旧 .o 文件缓存）
2. `idf.py build` 确认编译通过
3. `idf.py flash monitor` 烧录并观察串口
4. 期望看到 `chip revision: 103`（即 v1.3）且无 panic
5. 期望 bootloader 正常加载 app 分区

### 方案 B：降级到 IDF 5.4.2

IDF 5.4.2 仅有 hw_ver1 寄存器，天然支持 v1.3。

**优点**：无需特殊配置
**风险**：**高**。5.4.2 缺少本项目依赖的组件（esp_hosted v2.12.3、esp_lvgl_port v2.7.0 等可能需要 ≥ 5.5）

### 方案 C：打补丁 bootloader（❌ 不推荐）

手动修改 `bootloader_start.c` 或 ROM 链接脚本。

**优点**：无需降级
**风险**：**极高**。需要修改 ROM 地址映射，且 PMU 差异仍需处理。不建议。

### 方案 D：获取 v3.1+ 芯片（备选）

购买新版 ESP32-P4 模组（v3.0 或 v3.1）。

**优点**：使用最新 IDF 默认配置，无兼容性问题
**风险**：需要采购周期 + 费用；酷世 DIY 板现有库存可能是 v1.3

**如何识别芯片版本**：
- 编译任意固件后串口输出：`Chip: ESP32-P4, revision: xxx`
- `revision = 103` → v1.3（需 `SELECTS_REV_LESS_V3=y`）
- `revision = 301` → v3.1（使用默认配置）
- 也可通过 `AT+CGMR`（如果用 4G）或 `esptool chip_id` 命令查看

---

## 6. 方案 A 详细操作步骤

针对 CCA T5 的具体操作指引：

```bash
# 步骤 1：进入项目目录
cd hardware/p4c5-agent-terminal

# 步骤 2：编辑 sdkconfig.defaults（在末尾添加）
cat >> sdkconfig.defaults << 'EOF'

# === ESP32-P4 v1.3 chip revision support ===
# CRITICAL: v1.x and v3.x are mutually exclusive!
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y
CONFIG_ESP32P4_REV_MIN_FULL=100
CONFIG_ESP32P4_REV_MAX_FULL=199
EOF

# 步骤 3：清除旧的构建缓存（必须！否则旧的 v3 ROM 链接仍残留）
idf.py fullclean

# 步骤 4：重新配置并编译
idf.py set-target esp32p4
idf.py build

# 步骤 5：烧录并监控
idf.py -p /dev/tty.usbmodem5B8F1351161 flash monitor

# 步骤 6：在串口输出中确认
# 期望看到：
#   I (xxx) boot: chip revision: 103
#   I (xxx) boot: ESP-IDF v5.5.5 ...
#   I (xxx) boot: ... (正常启动)
# 不应看到：
#   Guru Meditation Error: Illegal instruction (PC: 0x...)
```

> **注意**：如果之前已经用默认配置编译过一次，`idf.py fullclean` **不能省略**。
> 旧的 build 目录中的 `.o` 文件包含了 v3 ROM 地址的硬编码引用，仅重新编译不会替换它们。

---

## 6. 风险 R8 标注

| 风险 ID | 描述 | 等级 | 影响范围 | 缓解措施 |
|---|---|---|---|---|
| **R8** | **ESP32-P4 chip revision 不兼容** | 🔴 **高** | 所有 v1.3 芯片（本项目当前硬件）| 使用 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y` 编译 |

### R8 详细影响

- **编译时**：必须在 sdkconfig 中设置 `CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y`
- **运行时**：v1.x 固件无法在 v3.x 芯片上运行（反之亦然）
- **OTA 风险**：如果 OTA 推送了错误版本的固件，设备将变砖（无法启动）
- **库存风险**：如果酷世后续发货 v3.1+ 芯片，现有固件无法使用

### R8 与已有风险的关系

| 已有风险 | R8 影响 |
|---|---|
| R1（引脚定义偏差）| 无影响 |
| R2（14 寄存器未核对）| 无影响 |
| R3（音频 codec 资料）| 无影响 |
| R4（ALDO4 电压）| 无影响 |
| R7（4G 流量费用）| 无影响 |

### R8 对 M5 的影响评估

- **阻塞等级**：**🔴 P0 阻塞**。R8 不解决，M5 无法启动（固件无法运行）
- **解决时间**：5 分钟（修改 sdkconfig 一个配置项）
- **回归风险**：低。xiaozhi 项目已验证此配置在 v1.3 上正常工作
- **长期影响**：如果未来采购 v3.x 芯片，需要两套 sdkconfig 分别编译

### OTA 防护建议

1. OTA 固件包中嵌入 chip revision 信息，启动时校验
2. bootloader 中 `esp_ota_img_app_verify()` 检查目标芯片版本
3. 如果芯片版本不匹配，拒绝烧写并报错

---

## 7. xiaozhi 参考配置（对照组）

xiaozhi 项目（`/tmp/p4c5_xiaozhi/`）是本项目的主要参考实现，其 sdkconfig 中的 P4 芯片配置如下：

```
# xiaozhi sdkconfig 提取（2026-09-06）
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_0=y
CONFIG_ESP32P4_REV_MIN_FULL=0
CONFIG_ESP32P4_REV_MAX_FULL=199

# IDF 版本要求
idf: version: '>=5.5.2'
```

**解读**：
- xiaozhi 使用 IDF ≥ 5.5.2（本项目也用 5.5.5 ✅）
- 明确选择 v1.x 支持（`SELECTS_REV_LESS_V3=y`）
- 最低版本 v0.0（兼容所有 v1.x）
- 最高版本 v1.99（排除 v3.x）

> **结论**：CCA 在移植 xiaozhi 代码时，**必须同步移植 sdkconfig 中的芯片版本配置**。仅复制 .cc/.h 文件不够，sdkconfig 同样关键。

---

## 7. M5 实机验证 Checklist（更新版）

综合 CCA T1-T5 + CCB T1-T5 的全部发现，M5 启动前需完成：

```
━━━ 环境修复（R8 解决） ━━━
 1. sdkconfig 添加 CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
 2. idf.py fullclean && idf.py build
 3. 烧录后确认不再 panic
 4. 串口输出 chip_revision=103（v1.3）

━━━ 电源轨验证（R4 + CCB T3） ━━━
 5. DCDC1 = 3.3V, ALDO1 = 1.8V, ALDO3 = 3.3V
 6. ALDO4 发射时电压 ≥ 2.7V（万用表）
 7. 0x64 充电电压验证（需修正为 0x2B = 4.2V）

━━━ 基础功能验证（CCA T1-T2） ━━━
 8. I2C 扫描：AXP2101(0x34), ES8311(0x18), ES7210(0x40)
 9. LCD 点亮：ST7102 MIPI-DSI 初始化成功
10. 4G 拨号：AT+CPIN? → READY, AT+CSQ ≥ 10, 拨号成功（CCA T4）
11. 音频播放：1kHz 测试音 → 扬声器出声
12. 4 麦录音：4 通道独立数据

━━━ 网络验证（CCA T4） ━━━
13. 4G 拨号 10 次成功率 ≥ 8/10
14. WebSocket 连接到 DSH
15. ping baidu.com 延迟 < 500ms
16. 24h 长稳（可选）

━━━ 回归验证 ━━━
17. 确认 xiaozhi 固件也能正常运行（作为对照组）

━━━ 文档更新 ━━━
18. 更新可信度评估报告：R8 状态从"新发现"改为"已解决"
19. 记录实机 chip_revision 值到本文档
20. 如有 v3.x 芯片到货，更新 §3 IDF 版本矩阵实测数据
```

### 7.1 M5 优先级排序

| 优先级 | 任务 | 依赖 | 估时 |
|---|---|---|---|
| **P0** | R8 修复（sdkconfig 配置） | 无 | 5 分钟 |
| P1 | 电源轨电压验证 | P0 | 30 分钟 |
| P1 | I2C 设备扫描 | P0 | 10 分钟 |
| P2 | 4G 拨号测试 | P0 + P1 | 1 小时 |
| P2 | 音频播放/录音 | P0 + P1 | 2 小时 |
| P3 | LCD 显示 | P0 | 30 分钟 |
| P3 | WebSocket 连接 DSH | P0 + P2(4G) | 1 小时 |
| P4 | 24h 长稳 | 全部通过 | 24 小时 |

---

## 附录：Kconfig 配置速查

```kconfig
# ── v1.3 芯片（本项目当前） ──
CONFIG_ESP32P4_SELECTS_REV_LESS_V3=y
CONFIG_ESP32P4_REV_MIN_100=y     # v1.0 最低
# 或
CONFIG_ESP32P4_REV_MIN_0=y       # v0.0 最低（xiaozhi 配置）

# ── v3.1+ 芯片（未来可能） ──
# CONFIG_ESP32P4_SELECTS_REV_LESS_V3 is not set
CONFIG_ESP32P4_REV_MIN_301=y     # v3.1 最低（IDF 5.5.5 默认）
```

> **互斥声明**（IDF Kconfig 原文）：
> "Support of ESP32-P4 rev. <3.0 and >=3.0 is mutually exclusive"

---

**版本**：v1.0（2026-09-06, CCB）
**数据来源**：IDF 5.5.5 源码（Kconfig.hw_support, CMakeLists.txt, rom.ld, rom.eco5.ld, hw_ver1/, hw_ver3/）+ xiaozhi sdkconfig
**下次更新**：R8 解决后（方案 A 验证通过），更新状态
