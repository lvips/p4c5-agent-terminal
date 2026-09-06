# Subagent 任务最佳实践（DSH 自用）

> **版本**：v1.0（2026-09-06）
> **背景**：M0-A 5 个调研 subagent 全部失败，本文档记录失败教训 + 修复方案
> **作用**：未来所有 subagent 任务（M0/M1/M2/M3 阶段）必须遵守

---

## 一、失败教训（M0-A 实际发生）

### 5 个调研 subagent 失败的共同模式

| 步骤 | 实际表现 |
|---|---|
| 1. 启动 | ✅ 接收任务 |
| 2. 读 p4c5-spec.md / p4c5-pins.csv | ✅ 成功（小文件）|
| 3. `pdftotext full.pdf` | ✅ 成功（但 dump 25k 行到 context）|
| 4. `sed -n 'A,Bp'` 翻页读 | ✅ 成功（但每条 sed 又 dump 40 行到 context）|
| 5. `grep "关键词"` | ✅ 成功（但反复 grep，反复 dump）|
| 6. 写报告 | ❌ **从未执行**（context 已满）|
| 7. git commit | ❌ **从未执行** |
| 8. report 回包 | ❌ **从未执行** |

**根因**：
- 🔴 **Context 爆炸**：25k+ 行 PDF 文本灌进 context
- 🔴 **方法论错误**：用 sed 翻页读而非 grep 模式搜
- 🔴 **写报告被延后**：先做调研再写，最后 context 不够
- 🟡 **图片 PDF 未 OCR**：`pdftotext` 对扫描件返回空

---

## 二、修复方案

### 2.1 DSH 端预处理（强烈推荐）

**派 subagent 前**，DSH 主动把 PDF 抽到临时目录：

```bash
# 标准化：所有 PDF 抽取到 /tmp/<project>_<topic>/
mkdir -p /tmp/p4c5_<topic>
for pdf in docs/hw/datasheets/<topic>/*.pdf; do
  name=$(basename "$pdf" | sed 's/\.\(pdf\|PDF\)$//')
  pdftotext "$pdf" /tmp/p4c5_${topic}/${name}.txt 2>/dev/null
done

# 标注图片 PDF
for f in /tmp/p4c5_<topic>/*.txt; do
  if [ ! -s "$f" ]; then
    echo "$(basename $f .txt) - 图片PDF, 需OCR" >> /tmp/p4c5_<topic>/IMAGES.txt
  fi
done
```

**优势**：
- subagent 收到极简 prompt（~500 tokens）
- 大量 PDF 抽取在 DSH 端完成（不进 subagent context）
- subagent 只需 `grep` 不要 `pdftotext`

### 2.2 Subagent 提示词新模板

**❌ 旧模板**（不要再用）：
```
必读资料（必须读全）：
1. docs/hw/datasheets/esp32-p4_datasheet_cn.pdf (1.9MB)
2. docs/hw/datasheets/esp32-c5_datasheet_cn.pdf (1.3MB)
3. docs/hw/schematic/原理图.pdf (1.6MB)

报告必须包含 5 个部分，每部分 ≤300 字，整个文档 ≤1500 字...
```

**✅ 新模板**（推荐）：
```
1. DSH 已预处理所有 PDF，文本在 /tmp/p4c5_<topic>/：
   - 用法: grep -A2 -B1 "关键词" /tmp/p4c5_<topic>/<file>.txt
   - **绝对不要 sed -n 全页读，不要 read 整本 .txt**
   - /tmp/p4c5_<topic>/IMAGES.txt 列出图片 PDF（pdftotext 无效）

2. 工作流（严格按此顺序）：
   a) **第一步** write 空骨架到目标文件（即使还没数据）
   b) **第二步** grep 填充
   c) **第三步** git add + commit
   d) **第四步** report 回包

3. 报告 ≤500 字，简洁。

4. 不要调用 bash 的 sandbox_permissions 参数（不需要）。
```

### 2.3 工作流纪律

**关键 4 步固定顺序**：

| 顺序 | 动作 | 目的 |
|---|---|---|
| 1 | `write` 写空骨架 | **先有 deliverable**（即使 0 内容也先建文件）|
| 2 | `grep` 填充数据 | 单条 grep 不超过 5 行输出 |
| 3 | `git add` + `commit` | 锁住成果 |
| 4 | `report` 回包 | 让 DSH 知道完成 |

**为什么先写骨架？** 即使 context 耗尽在第 3 步，文件也已经存在，DSH 可以手动补完。

### 2.4 图片 PDF 处理

| 情况 | 处理 |
|---|---|
| 文本 PDF | `pdftotext` 抽取 |
| 图片 PDF | 标注 "未 OCR" + 写 "M1 实测时人工补" |
| **有 tesseract** | `tesseract -l chi_sim+eng input.pdf out` |
| **没有 tesseract** | `brew install tesseract tesseract-lang` 后再 OCR |

**新硬件 P4C5 项目**：
- 原理图 PDF：图片型（Microsoft Print to PDF）
- ML307C 硬件规格书：图片型
- 其他 datasheet：文本型（pdftotext 可抽取）

### 2.5 Subagent 类型选择

| 场景 | 选择 | 原因 |
|---|---|---|
| 简单任务（单一文件）| `subagent` | 标准 subagent |
| 大量 context 消耗（PDF dump）| `subagent_fork` | 隔离 context |
| 多步流水线 | `subagent` × N（按步拆）| 简单可靠 |
| 不确定时 | `subagent` + 预抽取 | 兼容失败模式 |

### 2.6 Bash 工具注意事项

**Subagent #2 报错**：
```
invalid justification: expected a non-empty sentence
sandbox escalation to "danger-full-access" is not strictly wider...
```

**原因**：subagent 内部 bash 工具自动添加了 `sandbox_permissions` 参数，但当前 session 不需要（已 `danger-full-access`）。

**修复**：
- subagent 调 bash 时**不要**带 `sandbox_permissions` 参数
- 如果 subagent 报这个错，告诉它"去掉 sandbox_permissions 重试"

---

## 三、Subagent Prompt 模板（可直接复制）

### 模板 1：调研类任务

```markdown
# 任务：<一句话>

## DSH 已预处理
- 文本在 /tmp/p4c5_<topic>/，用法: `grep -A2 -B1 "关键词" <file>.txt`
- 图片 PDF 见 /tmp/p4c5_<topic>/IMAGES.txt

## 工作流（严格顺序）
1. write 写空骨架到 <output_path>（即使 0 内容也先建文件）
2. grep 填充数据
3. git add + commit
4. report 回包（用"输出要求"格式）

## 报告骨架（必填）
- §1 关键规格
- §2 引脚/接口
- §3 风险标注
- §4 M1 启动检查清单

## 红线
- ❌ 不要 sed -n 全页读
- ❌ 不要 read 整本 .txt
- ❌ 不要 pdftotext 二次抽取（DSH 已做）
- ❌ 不要调 bash 的 sandbox_permissions
- ❌ 不要改 CCA 专属文件 (p4c5-spec.md / p4c5-pins.csv / hardware/)
- ✅ 只写 <output_path>

## 输出要求（report 用此格式）
✅ 任务: <一句话>
📁 产出: <output_path> (<字数>)
📝 关键发现: 3-5 条
⚠️ 风险: 3-5 条
📌 建议: 2-3 条
```

### 模板 2：代码类任务

```markdown
# 任务：<一句话>

## 目标
<具体要做什么>

## 输入
- 文件 1: <path> - <说明>
- 文件 2: <path> - <说明>

## 输出
- 文件 1: <path> - <说明>
- 文件 2: <path> - <说明>

## 验收标准
- [ ] <可验证项 1>
- [ ] <可验证项 2>

## 红线
- ❌ 不要改 <不允许改的文件>
- ❌ 不要超过 <token 限额>
- ✅ commit 消息: <格式>
```

---

## 四、失败检测与恢复

### 4.1 如何发现 subagent 失败

| 信号 | 含义 |
|---|---|
| 关闭消息为空 | 失败在调研中途 |
| 关闭消息残缺（如 "现在去 AT 总览..."）| 失败在调研最后一步 |
| 没收到 `report` 回包 | 失败在写 deliverable 之前 |
| 关闭消息完整但 < 200 字 | 失败在最后总结 |

### 4.2 恢复策略

| 失败阶段 | DSH 端动作 |
|---|---|
| 调研中途 | DSH 自己接着调研（用我的工具）|
| 调研完成 + 写报告前 | 派一个**新** subagent，只给"写报告"任务（不带 PDF）|
| 写报告完成 + git commit 前 | DSH 自己 git commit |
| 全部完成 | 无需恢复 |

**关键**：5 个 subagent 失败时，**DSH 都用工具直接产出**了——这是兜底方案。

---

## 五、本项目的 subagent 调度约束

- **最大并发 2 个**（红线 11，与 OMT 相同）
- **文件边界**：CCA 写 `hardware/` + `docs/hw/`，CCB 写 `docs/research/` + 文档归档
- **commit 格式**：`type(scope): 中文描述`
- **不允许修改**：`multi-agent/`、`OMT/`、`~/.dsh/`、`/opt/homebrew/lib/node_modules/@deepseek-ai/`

---

## 六、版本

- v1.0 (2026-09-06): 初版，基于 M0-A 失败教训
