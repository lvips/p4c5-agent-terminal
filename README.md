# p4c5-agent-terminal — 便携 AI Agent 远程终端

> **一句话定位**：便携版 DSH —— 让你在户外、路上、沙发上，能用语音 + 触屏指挥家里 PC 上的 DSH Agent 干活。
>
> **硬件平台**：ESP32-P4C5 酷世DIY 开发板（ESP32-P4 主控 + ESP32-C5 WiFi 协处理器 + ST7102 480×800 MIPI-DSI 屏 + ML307C 4G）
>
> **理念**：重启智序，镜化人生 —— 重启人工智能的新秩序，镜像化数字人，为你而生。
>
> **公司**：序镜科技 / 项目代号 `lvips`

---

## 一、项目是什么

p4c5-agent-terminal 是 **OMT（Order Mirror Terminal / 序镜终端）项目的硬件 v2.0 版本**，核心产品定位 100% 不变，仅更换硬件平台：

```
OMT (v1)              OMT (v2) ← 本项目
├─ ESP32-P4 (主)       ├─ ESP32-P4 (主)        ← 共用
├─ ESP32-C6 (WiFi6)   ├─ ESP32-C5 (WiFi6 双频) ← 升级
├─ ILI9881C 5寸屏    ├─ ST7102 4.3寸屏        ← 缩小便携
├─ HU006 4G模组      ├─ ML307C 4G模组         ← 中移OneMO
├─ ES8311 + ES7210   ├─ ES8311 + ES7210       ← 共用
├─ NP-F 电池+ADC    ├─ AXP2101 PMIC          ← 完整电源管理
└─ BMI270 IMU        └─ LSM6DS3TR-C IMU      ← 替换
```

**核心闭环**：DSH（DeepSeek Harness，本会话）作为科研主任，指挥 CCA（科研主管）+ CCB（科研助理）双 CC 长驻工作流，开发本项目的硬件固件 + 软件协议栈。

---

## 二、目录结构

```
p4c5-agent-terminal/
├── README.md                          # 本文件
├── LICENSE                            # Apache 2.0
├── .gitignore                         # 忽略运行时状态
├── docs/                              # 项目文档
│   ├── README.md                      # docs 总索引
│   ├── hw/                            # 硬件资料（已建立）
│   │   ├── README.md                  # 5 分钟速查
│   │   ├── p4c5-spec.md               # 完整规格书
│   │   ├── p4c5-pins.csv              # 引脚定义表
│   │   ├── 00-新硬件差异矩阵.md        # OMT vs P4C5
│   │   ├── datasheets/                # 15 份 IC 规格书 PDF
│   │   ├── datasheets-summary/        # M0-A 调研产出
│   │   ├── schematic/                 # 原理图 + .step
│   │   ├── sdkconfig-references/      # xiaozhi 配置参考
│   │   ├── reference-codes/           # xiaozhi 代码导读
│   │   ├── tools/                     # 开发工具索引
│   │   └── shell/                     # 3D 外壳
│   ├── cc-orchestration/              # CC 编排文档
│   ├── handovers/                     # HANDOFF 历史
│   └── research/                      # 调研报告（CCB 写）
├── hardware/                          # 固件目录（待 M1 启动后建）
│   └── p4c5-agent-terminal/           # 主固件
└── .dsh-orchestration/                # ★ CC 工作流运行时（gitignore）
    ├── bin/                           # 6 个 shell 脚本
    ├── templates/                     # 3 份提示词模板
    ├── cordis-plugin/                 # DSH Cordis 集成
    ├── state/                         # PID / session-id（gitignore）
    ├── logs/                          # 日志（gitignore）
    └── locks/                         # 文件级锁（gitignore）
```

---

## 三、技术栈

| 层 | 技术 |
|---|---|
| 固件 | ESP-IDF **5.5.5+** 或 6.2 + FreeRTOS + LVGL **9.4.0** + C/C++ |
| 主控 | ESP32-P4（HP 双核 400MHz + LP 单核 40MHz） |
| WiFi 协处理器 | ESP32-C5（双频 WiFi6 + BLE5 + 802.15.4） |
| WiFi 协议栈 | esp_hosted v2.12+ |
| 显示屏 | ST7102 MIPI-DSI 480×800（2 lane, 820 Mbps） |
| 触摸 | ST7123（I2C 共享总线） |
| 音频 codec | ES8311（输出）+ ES7210（输入）+ NS4150B（PA） |
| 4G 模组 | ML307C-DC-CN（Cat.1）via esp-ml307 v3.6.4 |
| PMIC | AXP2101（DCDC1+ALDO1-4 多路输出） |
| USB-UART | CH343P（USB CDC-ACM，免驱） |
| Adapter | Node.js + @anthropic-ai/claude-agent-sdk + ws（**沿用 OMT**） |
| 通信协议 | WebSocket + 14 类帧（**沿用 OMT**） |
| 协作工具 | Claude Code (claude -p) + DSH + 双 CC 长驻 |
| 模型网关 | coding.dashscope（阿里百炼）|

---

## 四、与 OMT 项目的复用关系

| 维度 | 复用度 | 说明 |
|---|---|---|
| **产品定位** | 🟢 100% | 便携版 DSH |
| **MVP 验收清单** | 🟢 100% | 6 项不变 |
| **PC 端 Adapter** | 🟢 100% | `hardware/tab5-adapter/` 是 OS 无关的，直接复制改名 |
| **通信协议** | 🟢 100% | dsh_client 14 类帧格式不动 |
| **协作方法论** | 🟢 100% | DSH + CC + 双脑分离 + 心跳 + HANDOFF |
| **多 CC 编排** | 🟡 90% | OMT 已有 skill，直接套用 CCA/CCB 角色分工 |
| **硬件资料** | 🔴 10% | 全套重做（详见 `docs/hw/00-新硬件差异矩阵.md`）|
| **HAL 驱动** | 🔴 100% 重写 | 引脚 / 芯片全变 |
| **示例工程** | ⭐ 大利好 | xiaozhi-esp32 已完整适配本硬件 |

---

## 五、快速上手

### 5.1 构建固件（待 M1 完成后）

```bash
cd hardware/p4c5-agent-terminal
unset IDF_PYTHON_ENV_PATH IDF_PYTHON VIRTUAL_ENV PYTHONWARNINGS
export IDF_PATH=/Volumes/ZT-1T/项目开发/TLA/01-esp-idf-setup/esp-idf-v5.5.5
export IDF_PYTHON_ENV_PATH=/Users/mac-air-lvips/.espressif/python_env/idf5.5_py3.9_env
idf.py build
idf.py -p /dev/cu.usbmodem1401 flash
```

### 5.2 运行 Adapter（沿用 OMT）

```bash
cd hardware/p4c5-adapter
zsh -c 'source ~/.zshrc 2>/dev/null; node adapter.js'
```

### 5.3 启动 CC 工作流

```bash
# 启动 daemon（后台）
nohup bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/bin/cc-orchestrator.sh start &

# 查看状态
bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/.dsh-orchestration/bin/cc-orchestrator.sh status

# 在 DSH 主对话内通过 tool_agent_cca / tool_agent_ccb 派发任务
```

### 5.4 加载 DSH Cordis 插件

```bash
bash /Volumes/ZT-1T/项目开发/ESP32-P4C5/run-cordis-overlay.sh
```

---

## 六、当前状态与研发进度

**已打通**（Day 0 完成）：
- ✅ Git 仓库初始化 + remote 接入 `lvips/p4c5-agent-terminal`
- ✅ 目录骨架建立（`.dsh-orchestration/` + `docs/`）
- ✅ 15 份 IC 规格书 PDF 整理入库
- ✅ 原理图 + 3D 外壳入库
- ✅ README + LICENSE + .gitignore

**进行中**（M0.5）：
- 🔄 6 个 CC 编排脚本
- 🔄 3 份提示词模板
- 🔄 cca + ccb 长驻测试

**待启动**：
- ⏳ M0-A 5 个并行 subagent 资料采集
- ⏳ M1 Cordis 插件集成
- ⏳ M2 HANDOFF 机制
- ⏳ M3 试运行 + 项目集成

---

## 七、关键路径速查

| 项 | 路径 |
|---|---|
| 项目总览 | `README.md`（本文件） |
| 硬件资料总索引 | `docs/hw/README.md` |
| 完整规格书 | `docs/hw/p4c5-spec.md` |
| 引脚定义表 | `docs/hw/p4c5-pins.csv` |
| OMT 差异矩阵 | `docs/hw/00-新硬件差异矩阵.md` |
| CC 工作流说明 | `docs/cc-orchestration/README.md` |
| 当前 HANDOFF | `docs/handovers/HANDOFF.md` |
| 实践日志 | `multi-agent/实践日志.md`（待建）|
| Cordis 插件 | `.dsh-orchestration/cordis-plugin/code.host.js` |
| CC 启动脚本 | `.dsh-orchestration/bin/cc-orchestrator.sh` |

---

## 八、接手新环境的第一件事

1. 读本 README（项目全景）
2. 读 `docs/hw/README.md`（硬件资料总览）
3. 读 `docs/hw/p4c5-spec.md`（完整规格）
4. 读 `docs/hw/p4c5-pins.csv`（引脚定义）
5. 读 `docs/hw/00-新硬件差异矩阵.md`（vs OMT Tab5）
6. 读 `docs/handovers/HANDOFF.md`（当前进度）
7. 读 `docs/cc-orchestration/README.md`（工作流说明）
8. `bash .dsh-orchestration/bin/cc-orchestrator.sh start`

---

## 九、开发约定（接手必读）

1. **接力棒**：CC 端自动心跳（dsh-orchestration/bin/heartbeat），DSH 端手动心跳 + HANDOFF.md
2. **实践日志**：每次踩坑先记 `docs/cc-orchestration/CHANGELOG.md`，再提炼进 skill
3. **commit 规范**：`type(scope): 中文描述`（如 `feat(hw): ...`、`fix(audio): ...`）
4. **文件边界**：CCA 写 `hardware/` + `docs/hw/`；CCB 写 `docs/research/` + `docs/cc-orchestration/` + 文档归档
5. **敏感信息**：`.env*` 绝不提交（已 gitignore）
6. **串行并发**：同一时刻最多 2 个 claude -p（红线 11）

---

**License**: Apache 2.0
**Repo**: https://github.com/lvips/p4c5-agent-terminal
**作者**：序镜科技 / DSH + CCA + CCB 协作