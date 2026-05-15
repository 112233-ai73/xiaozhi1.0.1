# 项目级中文变更说明 Skill 新增

## 背景

当前工作区相对上次提交 `19ea8db add_sr_handler` 没有已跟踪代码文件变更，主要新增了项目级 Skill 文件：

| 类型 | 文件 | 说明 |
|---|---|---|
| 新增 | `.codex/skills/gen-changelog/SKILL.md` | 项目级 Skill，用于生成中文技术变更说明文档 |
| 新增 | `.codex/skills/gen-changelog/output/current-change-note.md` | 本次生成并保存的中文说明文档 |

本次变更的目标是：在项目内沉淀一套固定的中文说明文档生成规则，让后续可以通过 `gen-changelog` 快速生成类似工程改造说明、方案说明、可删除代码分析和迁移文档。

## 推荐方案：项目级 `gen-changelog` Skill

新增 `.codex/skills/gen-changelog/SKILL.md`，将中文技术说明文档的写作流程固化到项目中。

该 Skill 主要覆盖以下场景：

| 场景 | 生成内容 |
|---|---|
| 当前代码改动说明 | 总结相对 `HEAD` 的文件变化和行为变化 |
| 改造方案说明 | 输出背景、推荐方案、详细设计、迁移步骤 |
| 可删除代码分析 | 用表格说明旧代码、所在文件、估算行数和删除原因 |
| 嵌入式状态机说明 | 描述状态流转、任务关系、队列/环形缓冲区数据流 |
| 评审材料整理 | 生成可直接放进项目记录的中文 Markdown |

## 为什么选这个方案

项目级 Skill 比单次提示词更稳定，适合当前这种反复生成工程说明文档的需求。

| 当前方式 | 新方案 | 说明 |
|---|---|---|
| 每次手动描述文档格式 | 使用 `gen-changelog` | 固定输出结构，减少重复说明 |
| 临时生成变更总结 | 项目内 Skill 规范化 | 后续在同一项目中可复用 |
| 文档结构依赖聊天上下文 | `.codex/skills` 持久保存 | 换会话后仍可通过 Skill 名称触发 |

## 详细设计

### 输入来源判断

Skill 中定义了三类输入来源：

1. 用户指定文件或模块时，优先分析指定范围。
2. 用户说“本次改动”“当前修改”“较上次提交”时，使用 Git 对比当前工作区和 `HEAD`。
3. 用户只给需求描述时，基于描述生成方案文档，并明确哪些内容是推断。

### Git 变更收集

Skill 规定了推荐使用的命令：

```powershell
git status --short
git diff --name-status HEAD
git diff --stat HEAD
git diff HEAD -- <path>
git ls-files --others --exclude-standard
```

其中 `git ls-files --others --exclude-standard` 用于覆盖未跟踪文件，避免只看 `git diff` 时漏掉新建文档或新建代码。

### ESP-IDF 项目关注点

文档特别加入了本项目相关的分析重点：

| 关注点 | 说明 |
|---|---|
| FreeRTOS task | 分析任务循环、任务间数据流 |
| queue / ringbuffer | 分析队列、环形缓冲区传递路径 |
| 全局状态 | 分析 `com_status`、语音识别状态等状态流转 |
| VAD / 语音识别 | 分析人声检测、命令识别、超时逻辑 |
| 串口 / 通信接口 | 分析外设交互和数据输出 |
| 内存和错误处理 | 检查 `malloc`、返回值、初始化失败路径 |

### 文档保存规则

如果用户要求生成并保存文档，Skill 默认将结果保存到：

```text
.codex/skills/gen-changelog/output/
```

文件名使用英文、小写、短横线，例如：

```text
current-change-note.md
voice-state-change.md
removable-code-analysis.md
```

## 文档输出结构

Skill 中预设了类似工程方案文档的中文结构：

```markdown
# <主题名称>

## 背景

## 推荐方案：<方案名>

## 为什么选这个方案

## 架构总览

## 详细设计

## ESP32 侧代码

## 可删除的代码

## 吞吐量/资源/风险评估

## 与当前方案的映射

## 迁移步骤

## 验证方式
```

这套结构适合写改造方案、代码迁移说明、旧逻辑清理报告。

## 可删除的代码

本次变更是新增 Skill 文档和输出文档，没有替换现有业务代码，因此当前没有可删除代码。

| 可删除 | 所在文件 | 行数 | 原因 |
|---|---|---:|---|
| 无 | 无 | `0 行` | 本次只新增 `.codex/skills/gen-changelog` 相关文档，未改动已有业务代码 |

## 风险评估

| 风险点 | 触发条件 | 影响 | 建议验证 |
|---|---|---|---|
| Skill 触发不稳定 | 请求中没有明确写 `gen-changelog` | Codex 可能不会加载该 Skill | 使用时显式写“使用 gen-changelog” |
| 中文显示乱码 | PowerShell 使用默认编码读取 UTF-8 文件 | 控制台看到乱码，但文件内容正常 | 使用 `Get-Content -Encoding UTF8` |
| 文档输出混入 Skill 目录 | 后续生成大量文档 | `.codex/skills` 目录可能变杂 | 统一放到 `output` 子目录 |

## 与当前方案的映射

| 当前实现 | 新方案 | 说明 |
|---|---|---|
| 手动描述“按截图格式生成文档” | `gen-changelog` Skill | 将格式要求沉淀到项目内 |
| 临时分析 Git diff | Skill 内固定 Git 分析流程 | 后续生成文档时流程更一致 |
| 普通变更总结 | 中文工程文档模板 | 增加背景、方案、设计、删除代码、迁移步骤等章节 |
| 只在聊天中输出 | 保存到 `output` 目录 | 生成结果可在项目内长期保留 |

## 使用方式

后续可以直接这样使用：

```text
使用 gen-changelog，帮我生成当前代码改动的中文说明文档，并保存到 skills 文件夹
```

针对指定文件：

```text
使用 gen-changelog，针对 main/audio/audio_sr.c 生成中文改造说明，并保存
```

生成可删除代码分析：

```text
使用 gen-changelog，分析当前改造后哪些代码可以删除，并保存成文档
```

## 验证方式

1. 执行 `git status --short`，确认 `.codex/skills/gen-changelog` 相关文件处于待提交状态。
2. 使用 `Get-Content .codex/skills/gen-changelog/SKILL.md -Encoding UTF8`，确认 Skill 中文内容正常。
3. 使用 `Get-Content .codex/skills/gen-changelog/output/current-change-note.md -Encoding UTF8`，确认生成文档中文内容正常。
4. 在后续请求中显式输入 `使用 gen-changelog`，观察输出是否按中文技术文档结构组织。
