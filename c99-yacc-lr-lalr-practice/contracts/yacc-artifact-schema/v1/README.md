# YACC 可视化数据协议（v1）

## 1. 目标

该协议用于解耦：

- 核心算法层（`/src`）：负责生成 `artifacts/yacc/stepX/...` 原始文本产物。
- 可视化层（`/visualizer`）：只读取标准化 JSON，不依赖 `/src` 内部实现细节。

两者通过“文件协议”交互，不做代码级调用。

## 2. 协议版本

- 版本标识：`yacc-visualizer/v1`
- 元数据入口：`manifest.json`

## 3. 目录约定

适配后输出到：

```text
visualizer/public/data/v1/<case_id>/
  manifest.json
  step3/...
  step4/...
  step5/...
  step6/...
```

## 4. 关键文件

- `manifest.schema.json`：数据入口索引结构。
- `step-common.schema.json`：通用基础结构（summary/analysis）。
- `step3.schema.json`：符号表与产生式。
- `step4.schema.json`：增广文法与 lhs->productions 索引。
- `step5.schema.json`：First 集。
- `step6.schema.json`：LR(1) I0 / goto / lookahead 推导记录。

## 5. 兼容策略

1. 新增字段：允许，不破坏 v1 前端。
2. 字段语义变更：必须升级到 `v2`。
3. 适配层负责从旧产物格式转换到新协议。
