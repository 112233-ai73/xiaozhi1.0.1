---
name: gen-changelog
description: Generate Chinese technical change documents in a structured design-note style. Use when the user asks to generate a Chinese explanation document, changelog, migration note, refactor report, design proposal, removable-code analysis, or code-change summary using sections like background, recommended solution, detailed design, code-side changes, removable code, effort estimate, current-solution mapping, and migration steps.
---

# 中文技术变更说明生成

## 目标

根据当前代码、Git diff、用户描述或指定文件，生成一份中文技术说明文档。文档风格参考工程改造方案说明：标题清晰、目录完整、重点突出，适合给自己、同事或评审人员快速理解“改了什么、为什么改、怎么迁移、哪些代码可以删”。

## 工作流程

1. 先确认输入来源：
   - 如果用户指定文件或模块，优先分析指定范围。
   - 如果用户说“本次改动”“当前修改”“较上次提交”，使用 Git 对比当前工作区和 `HEAD`。
   - 如果用户只给需求描述，基于描述生成方案文档，并明确哪些内容是推断。

2. 收集变更信息：

```powershell
git status --short
git diff --name-status HEAD
git diff --stat HEAD
git diff HEAD -- <path>
git ls-files --others --exclude-standard
```

3. 阅读关键代码：
   - 优先读状态机、任务循环、初始化入口、公共头文件、通信接口、回调函数。
   - 对嵌入式/ESP-IDF 项目，重点关注 FreeRTOS task、queue、ringbuffer、全局状态、VAD/语音识别状态、串口/网络通信、内存分配和错误返回。
   - 对未跟踪文件，要直接读取文件内容，不能只看 `git diff`。

4. 生成中文文档：
   - 使用中文标题和中文说明。
   - 代码符号、函数名、宏名、文件名保持原样，用反引号包裹。
   - 结论先行，每节只写对理解改造有帮助的信息。
   - 可以使用表格承载“文件、行数、原因、影响、状态”等结构化内容。

## 推荐文档结构

根据用户需求选择下面的章节，不需要每次全部使用。大型改造优先使用完整结构；小改动保留核心章节即可。

```markdown
# <主题名称>

## 背景

## 推荐方案：<方案名>

## 为什么选这个方案

## 架构总览

## 详细设计

### <模块一>

### <模块二>

### <模块三>

## ESP32 侧代码

### <功能点一>

### <功能点二>

## 可删除的代码

| 可删除 | 所在文件 | 行数 | 原因 |
|---|---|---:|---|
| `<函数/变量/机制>` | `<file.c>` | `~N 行` | <原因> |

## 吞吐量/资源/风险评估

## 与当前方案的映射

| 当前实现 | 新方案 | 说明 |
|---|---|---|
| `<旧机制>` | `<新机制>` | <迁移关系> |

## 迁移步骤

1. <步骤一>
2. <步骤二>
3. <步骤三>

## 验证方式
```

## 表格模板

### 可删除代码表

用于说明改造后哪些旧代码、机制、宏、任务或状态可以删除。

```markdown
| 可删除 | 所在文件 | 行数 | 原因 |
|---|---|---:|---|
| `<name>` | `<path>` | `~10 行` | <为什么新方案不再需要> |
```

### 当前方案映射表

用于说明旧实现和新实现之间的对应关系。

```markdown
| 当前方案 | 新方案 | 影响 |
|---|---|---|
| <旧逻辑> | <新逻辑> | <行为变化或兼容性说明> |
```

### 风险评估表

用于说明潜在风险、触发条件和验证手段。

```markdown
| 风险点 | 触发条件 | 影响 | 建议验证 |
|---|---|---|---|
| <风险> | <条件> | <影响> | <测试方法> |
```

## 写作规则

- 用“背景 -> 方案 -> 设计 -> 代码影响 -> 删除/迁移 -> 验证”的顺序组织内容。
- 标题要像工程文档，不要写成聊天记录。
- 避免空泛描述，例如“优化代码结构”；要写清楚优化了哪个模块、删掉了什么机制、替换成什么机制。
- 对不确定内容使用“推测”“建议”“需要确认”，不要写成已发生事实。
- 如果引用文件，尽量给出可点击文件链接和行号。
- 如果估算行数，用 `~40 行` 这种近似写法，不要假装精确。
- 如果涉及状态机，必须写清状态流转，例如 `IDLE -> WORKING -> LISTENING`。
- 如果涉及任务/线程，必须说明任务之间的数据流和同步方式。
- 如果涉及协议迁移，必须说明旧协议职责、新协议职责、保活/重连/消息完整性由谁负责。

## 输出风格

文档应当像一份可以直接放进项目记录或评审材料里的中文 Markdown。语气简洁、工程化、明确，不写寒暄。

优先使用：

- 二级/三级标题
- Markdown 表格
- 简短段落
- 代码符号反引号
- 编号迁移步骤

避免使用：

- 过长段落
- 无依据的性能结论
- 只列文件名但不解释影响
- 与代码无关的泛泛建议

## 保存规则

如果用户要求“生成文档”“保存文档”“存到 skills 文件夹”或类似表达，直接把生成结果保存为 Markdown 文件：

```text
.codex/skills/gen-changelog/output/<主题名>.md
```

命名规则：

- 文件名使用英文、小写、短横线，例如 `current-change-note.md`。
- 如果用户指定名称，优先使用用户指定名称。
- 如果没有指定名称，按内容选择名称，例如 `voice-state-change.md`、`current-change-note.md`、`removable-code-analysis.md`。
- 保存后在回复中给出文件路径，并简要说明文档主题。
