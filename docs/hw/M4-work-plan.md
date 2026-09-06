# M4 工作计划（DSH 主导 + CCA/CCB 执行）

> **版本**：v0.1.0（2026-09-06 启动）
> **阶段**：M4 真机集成 + PoC
> **执行模式**：DSH 调度 + CCA（主力科研）+ CCB（辅助科研）双 CC 并行

---

## 一、M4 阶段总目标

从 0 到 1：建立**最小可编译的 ESP-IDF 项目骨架**，并实现 5 个核心组件的代码 + 测试规范。

**时间预估**：6 个任务批次 × 30-60 分钟/批 = 3-6 小时
**红线**：最多 2 个 `claude -p` 并发

---

## 二、6 批次任务矩阵

| 批次 | CCA 任务（硬件代码）| CCB 任务（文档 + 测试）| 依赖 |
|---|---|---|---|
| **T1** | ESP-IDF 项目脚手架 | M4 开发指南 | 无 |
| **T2** | p4c5_audio 组件 | p4c5_audio 测试规范 | T1 |
| **T3** | p4c5_pmic 组件 | AXP2101 14 寄存器核对报告 | T1 |
| **T4** | p4c5_4g 组件 | 4G 流量监控指南 | T1 |
| **T5** | dsh_client 协议层（复用 OMT）| 协议测试计划 | T1 |
| **T6** | p4c5_lvgl UI 框架 | UI 设计规范 | T1 |

---

## 三、详细任务

### T1: ESP-IDF 项目脚手架
**CCA 输出**：`hardware/p4c5-agent-terminal/`（20+ 文件）
- 顶层 `CMakeLists.txt` + `sdkconfig.defaults` + `partitions.csv`
- `main/CMakeLists.txt` + `idf_component.yml` + `app_main.c` + `config.h` + `Kconfig.projbuild`
- 5 个组件目录（占位 + TODO）
- `README.md`（编译步骤）
- `scripts/flash.sh`（一键烧录）

**CCB 输出**：`docs/hw/M4-development-guide.md`（2000-3500 字）
- M4 目标
- ESP-IDF 脚手架说明
- 10 个组件移植指南
- 真机验证清单
- 编译验证流程
- M4 → M5 过渡

### T2: 音频组件
**CCA 输出**：`p4c5_audio/p4c5_audio.{c,h}` + `Kconfig.projbuild`
- `init()` / `play()` / `record()` / `set_volume()` / `set_mute()` / `test_tone()` 6 个 API
- 集成 `espressif/esp_audio_codec` 库
- 基于 xiaozhi `box_audio_codec.cc` 移植

**CCB 输出**：`docs/hw/audio-test-spec.md`（1500-2500 字）
- ≥10 个测试用例
- ≥4 个性能测试
- 24h 长稳计划
- 失败排查指南

### T3: 电源管理组件
**CCA 输出**：`p4c5_pmic/p4c5_pmic.{c,h}`
- 集成 xiaozhi `pmic_axp2101` 组件
- DCDC1=3.3V / ALDO1=1.8V / ALDO3=3.3V / ALDO4=2.9V
- 14 个特殊寄存器配置（基于 xiaozhi）
- 库仑计 API（`get_battery_level()` / `is_charging()` / `is_discharging()`）

**CCB 输出**：`docs/hw/pmic-registers-audit.md`（1500-2500 字）
- 14 个寄存器（0x14/0x15/0x16/0x22/0x24/0x27/0x50/0x61/0x62/0x63/0x64/0x90/0x93）逐个核对
- 每个寄存器的：默认值 / xiaozhi 写入值 / 推测功能 / 规格书引用
- 风险标注（R2 优先级）

### T4: 4G 组件
**CCA 输出**：`p4c5_4g/p4c5_4g.{c,h}`
- 集成 `78/esp-ml307` v3.6.4
- 初始化 UART + POWER + DTR
- `dial()` / `send_tcp()` / `hangup()` API
- 关键风险标注：R4（2.9V 可能不够 4G）

**CCB 输出**：`docs/hw/4g-flow-guide.md`（1000-2000 字）
- 流量监控策略
- 定向流量包申请指南
- 4G vs WiFi failover 决策树
- 4G 拨号成功率测试

### T5: 协议层
**CCA 输出**：`components/dsh_client/dsh_client.{c,h}`
- 复用 OMT `hardware/tab5-adapter/` 的协议层
- 14 类帧格式（FRAME_HELLO / AUTH / TEXT / VOICE / IMAGE / ...）
- WebSocket Client
- 串口 Adapter（已 100% 复用）

**CCB 输出**：`docs/hw/protocol-test-plan.md`（1500-2500 字）
- 每类帧的测试用例
- 心跳（30s）测试
- 大数据传输测试（图片 / 文件）
- 错误恢复测试

### T6: UI 框架
**CCA 输出**：`p4c5_lvgl/p4c5_lvgl.{c,h}`
- 集成 xiaozhi `lvgl_st7102_display` 组件
- 主屏 UI（电量 + 信号 + 输入框 + 历史）
- 语音输入按钮
- 设置菜单

**CCB 输出**：`docs/hw/ui-design-spec.md`（2000-3000 字）
- UI 设计规范（4.3" 480×800）
- 配色方案
- 交互流程图
- 可访问性（字体大小 / 颜色对比）

---

## 四、依赖关系图

```
T1 (脚手架)
  ├── T2 (audio)
  ├── T3 (pmic)
  ├── T4 (4g)
  ├── T5 (protocol)
  └── T6 (lvgl)
```

所有 T2-T6 依赖 T1。T2-T6 之间**无依赖**（可任意顺序）。

---

## 五、调度策略

- **串行调度**：T1 必须先完成
- **并行调度**：T1 完成后，T2-T6 可分 3 批（T2+T3, T4+T5, T6）并行
- **约束**：每批最多 2 个 CC（红线 11）
- **预期总时长**：T1 = 30-60 分钟，T2-T6 每个 = 20-40 分钟

---

## 六、DSH 责任

| 责任 | 说明 |
|---|---|
| 派任务 | 写 prompt 文件 + `claude -p --resume` |
| 监控 | `bash .dsh-orchestration/bin/cc-orchestrator.sh status` |
| 提交 | `git add` + `git commit` + `git push`（用户授权后）|
| 派发间协调 | 处理 CC 反馈，调度下一批 |
| 风险追踪 | 累积每批风险，更新可信度报告 |

---

## 七、CC 反馈协议

每个任务结束后，CC 必须用以下格式回报：

```
✅ 任务: <一句话>
📁 产出: <文件路径> (<字数/行数>)
📝 关键 API / 章节: <列表>
⚠️ 风险: <列表，引用可信度报告 ID>
📌 建议: <下一步>
```

DSH 收到后：
1. 检查实际产出
2. commit
3. 派下一批

---

## 八、M4 完成的定义

- [ ] 5 个核心组件（audio / pmic / 4g / protocol / lvgl）有真实代码（不是 TODO）
- [ ] 10 份配套文档（开发指南 + 5 测试规范 + 1 寄存器核对 + 1 流量指南 + 1 协议计划 + 1 UI 规范）
- [ ] `idf.py set-target esp32p4` 通过
- [ ] 风险评估报告更新（R2/R3/R4 状态）
- [ ] 全部 commit 推送到本地（push 需用户授权）

---

## 九、M5 过渡

M4 完成后，进入 M5：
- 升级 ESP-IDF 到 5.5.5+（当前 5.4.2）
- 真机验证每个组件
- 24h 长稳测试
- 流量监控
- 低功耗优化

---

**作者**：DSH
**启动日期**：2026-09-06
**下次更新**：每批次完成后