# DSH 指挥官工作规范（v1.0）

> 创建于：2026-09-06
> 触发事件：M16 教训（未调研官方资料即派活，导致 CCA 走弯路）

## 🎯 DSH 核心职责

**DSH 是指挥官，不是主开发。** 但"指挥"≠"甩手"。每个 T 任务派发前必须：

### 步骤 1：调研官方资料（5-10 分钟）

按优先级查：

1. **ESP-IDF examples/**
   - `/Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup/esp-idf-v5.5.5/examples/`
   - 尤其关注：`peripherals/lcd/*`、`peripherals/mipi_dsi/*`、`system/esp_lvgl_adapter`

2. **esp_* 官方组件**
   - `components/esp_*`（直接内置）
   - `managed_components/espressif__*/`（自动拉取的）

3. **xiaozhi 参考实现**
   - `/tmp/p4c5_xiaozhi/`
   - 关键组件：`components/lvgl_st7102_display/`、`components/pmic_axp2101/`、`components/p4c5_audio/`

4. **OMT 参考实现**
   - `/Volumes/ZT-1T/项目开发/OMT/`
   - 仅当前三者都没有时参考

### 步骤 2：提取关键信息

调研结果要进派发 prompt：

- ✅ **API 名称**（如 `esp_lv_adapter_get_required_frame_buffer_count`）
- ✅ **配置参数**（如 tear_avoid_mode, rotation）
- ✅ **代码片段**（最简实现示例）
- ✅ **依赖声明**（如 `espressif/esp_lvgl_adapter: ^0.2.0`）
- ✅ **已知陷阱**（如"在 PSRAM 上用 1 FB 会 underrun"）

### 步骤 3：写完整 prompt

模板：

```markdown
# CCA 任务 T##：[目标]

## 🎯 调研结论（DSH 已查清）

[最重要的部分！先写调研结果，再写任务]

## 📚 必读资料

- 参考文件 1（关键代码）
- 参考文件 2（官方示例）
- 参考文件 3（spec）

## 📝 任务步骤

1. 步骤 1
2. 步骤 2
...

## 输出要求

[明确的交付物]

## 红线

[不能碰的东西]
```

### 步骤 4：派发 + 监控

```bash
claude -p --resume ${SID} < prompt.md
```

启动轮询脚本（已有 `.dsh-orchestration/bin/cc-poll.sh`）。

## 🚫 反模式（避免）

| ❌ 不要 | ✅ 应该 |
|---|---|
| "你自己查资料吧" | DSH 先调研，写进 prompt |
| "想办法优化一下" | "用 esp_lv_adapter，调用 X()，参考 Y 文件" |
| "性能不行，改改" | "MIPI DSI + PSRAM 带宽争用，用 esp_lv_adapter 双 FB" |
| 派活后就等 | 调研期间就写好 prompt，CC 启动就开干 |

## 📊 调研案例

### M14+15：LVGL 屏幕黑屏
- **错误做法**：让 CC 自己想办法（结果用自家 3-task 模型，DSI underrun）
- **正确做法**：DSH 调研 `/tmp/p4c5_xiaozhi/components/lvgl_st7102_display/ksdiy_lvgl_port.c`
- **发现**：`esp_lv_adapter` 自动管 FB 数 + tear-avoidance
- **结果**：5 分钟解决，CC 立即开干

### M12：ALDO4 电压错误
- **错误做法**：让 CC 直接照搬 xiaozhi 2.9V
- **正确做法**：DSH 派活前查 `ML307C 规格书 §3.1 要求 VBAT ≥ 3.4V`
- **发现**：必须升到 3.4V
- **结果**：避免 2 个迭代浪费

### M11：0x64 充电电压
- **错误做法**：让 CC 凭印象写寄存器
- **正确做法**：DSH 派活前查 `AXP2101 规格书 §6.13.2.62`
- **发现**：3-bit LUT，0x03 = 4.2V（写 0x2B 是 hardware mask 到 0x03）
- **结果**：避免假阳性 bug

## 🔄 持续改进

每次发现 DSH 派活失误，记录到 `docs/hw/DSH-MISTAKES.md`，下次避免。

## 🛡️ 红线（DSH 不能违反）

1. **不调研就派活** = 浪费 CCA/CCB 时间 = 浪费整个项目时间
2. **不指定具体 API/参数** = 让 CC 走弯路
3. **不引用官方资料** = 重复造轮子
4. **不验证参考资料存在** = 给出错的指引