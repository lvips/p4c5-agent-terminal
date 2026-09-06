# docs 总索引

> **版本**：v1（2026-09-06，DSH Day 0 初始化）
> **作者**：DSH + CCA + CCB
> **触发**：项目初始化

---

## 一、目录分类

| 目录 | 内容 | 归属 |
|---|---|---|
| `hw/` | 硬件资料（规格书 / 原理图 / 引脚 / datasheets） | CCA 维护 |
| `cc-orchestration/` | CC 工作流文档（README / RUNBOOK / ARCHITECTURE / CHANGELOG） | CCB 维护 |
| `handovers/` | HANDOFF 历史与当前 | DSH 维护 |
| `research/` | 调研报告（PDF 抽取 / 规格对比 / 第三方资料汇总） | CCB 维护 |
| `hardware/` *(未来)* | 固件开发文档（架构 / API / 调试） | CCA 维护 |

---

## 二、按主题速查

| 想看什么 | 跳到 |
|---|---|
| 这个开发板是什么 | `hw/README.md` → `hw/p4c5-spec.md` |
| 引脚怎么接 | `hw/p4c5-pins.csv` |
| 跟 OMT Tab5 差在哪 | `hw/00-新硬件差异矩阵.md` |
| **xiaozhi vs p4c5 电源对比** | **`hw/xiaozhi-power-comparison.md`** |
| IC 规格书在哪 | `hw/datasheets/`（15 份 PDF）|
| **IC 规格速查** | **`hw/datasheets-summary/README.md`** → pmic-axp2101.md / ml307c.md / peripherals.md / AXP2101-REG-MAP.md |
| CC 怎么用 | `cc-orchestration/README.md` |
| 当前进度到哪 | `handovers/HANDOFF.md` |
| 历史踩坑 | `cc-orchestration/CHANGELOG.md` |
| 项目根目录 | `../README.md` |

---

## 三、文档维护规则

- **新建文档**：先在所属主题目录下加 `.md`，再在本 README 的「按主题速查」加跳转
- **大文档改动**：超过 50% 内容替换时，版本号升 minor（如 v1.0 → v1.1）
- **跨仓引用**：本项目可能引用 OMT（`/Volumes/ZT-1T/项目开发/OMT/`）和 macs（`/Volumes/ZT-1T/项目开发/multi-agent-collab-system/`），统一用相对路径
- **文档归档**：过期文档移至 `<主题>/archive/` 并加 `ARCHIVED-YYYY-MM-DD` 前缀

---

## 四、与 OMT 文档结构对照

| OMT 路径 | 本项目路径 | 关系 |
|---|---|---|
| `OMT/README.md` | `README.md`（本项目根）| 沿用结构 |
| `OMT/docs/hardware/00-用户需求与产品定位.md` | （沿用，不重复） | 100% 一致 |
| `OMT/docs/hardware/实施计划.md` | （未来）`docs/hardware/实施计划.md` | 70% 沿用 |
| `OMT/docs/hardware/M5Stack-Tab5-4G-终端-定型方案.md` | `docs/hw/p4c5-spec.md` | 替换 |
| `OMT/docs/hardware/hw/*.md` | `docs/hw/datasheets-summary/*.md` | 替换 |