# YACC 全流程实施文档

## 文档状态

- 版本：`v0.9`
- 当前完成范围：已完成 YACC 全流程中的第 `1`、`2`、`3`、`4`、`5`、`6`、`7`、`8`、`9`、`10` 步。
- 依据文件：
- `c99.y`
- `docs/编译原理专题实践：全周期详细进度计划.md`
- `docs/编译原理课程实践 2026.pptx`

---

## 一、阶段目标（当前版本）

本版本完成十件事：

1. 固化 Yacc 输入范围与第一版支持边界（第 1 步）。
2. 固化 Yacc 核心内部数据结构设计（第 2 步）。
3. 完成 Yacc 输入文件解析器实现（第 3 步）。
4. 完成文法预处理与增广文法构造校验（第 4 步）。
5. 完成 First 集与符号串 First 计算（第 5 步）。
6. 完成 LR(1) 项、closure(I0)、goto(I0, X)（第 6 步）。
7. 完成 LR(1) 项目集规范族（全状态）与状态转移图（第 7 步）。
8. 完成 Action/Goto 分析表构造、冲突检测与冲突消解日志（第 8 步）。
9. 完成 LR 总控程序（查表驱动移进-归约执行）与解析日志导出（第 9 步）。
10. 完成 LR(1) 到 LALR(1) 状态合并、LALR 表构造与差异对比（第 10 步）。

当前版本已完成从输入解析到 LR/LALR 表和运行时解析的主链路，后续可直接进入第 11 步词法联调与语义接口阶段。

---

## 二、第 1 步交付：输入范围与支持边界（已完成）

## 2.1 输入文件结构约定

Yacc 输入文件采用三段结构：

1. Definitions 区：第一个 `%%` 之前。
2. Rules 区：两个 `%%` 之间。
3. User Subroutines 区：第二个 `%%` 之后。

`c99.y` 当前符合此结构。

## 2.2 第一版必须支持的语法元素

第一版解析器必须支持以下元素，且能正确解析 `c99.y`：

1. `%token` 声明（同一行可多个 token）。
2. `%start` 声明（单一开始符）。
3. 规则段中的产生式定义：
- 左部非终结符
- `:`
- 备选分支 `|`
- 规则结束符 `;`
4. 右部符号类型：
- 命名 token（如 `IDENTIFIER`、`IF`）
- 命名非终结符（如 `translation_unit`）
- 单字符字面量终结符（如 `'('`、`';'`、`'+'`）
5. 空白与换行分隔。
6. User Subroutines 区按原文保留，不参与分析表构造。

## 2.3 第一版不做的能力（明确边界）

以下能力暂不作为第 1 版的必做项：

1. `%left`、`%right`、`%nonassoc` 优先级与结合性声明。
2. `%union`、`%type`、`%prec` 的完整语义支持。
3. 规则中复杂 C 动作代码执行。
4. include 链接、多文件语法拼接、条件编译。

说明：`c99.y` 当前主要使用 `%token` 与 `%start`，因此以上边界不影响第一版完成“可解析并建模”。

## 2.4 动作（Action）策略

第一版动作策略定为“解析并保存，不执行”：

1. 若规则无动作，存储为空动作。
2. 若规则有 `{ ... }` 动作块，按原文字符串保存。
3. 分析阶段不运行动作代码，只为后续语义阶段预留挂载点。

## 2.5 第 1 步验收标准

满足以下条件即判定第 1 步完成：

1. 能说明支持项和不支持项（本节已固定）。
2. 能明确回答“当前版本可以读取 `c99.y` 的哪一部分以及如何处理”。
3. 后续开发不再反复讨论输入范围边界。

结论：第 1 步已完成。

---

## 三、第 2 步交付：内部数据结构设计（已完成）

## 3.1 设计原则

1. 全链路复用：同一份结构要服务于解析、First、Closure/Goto、表构造、总控程序。
2. 稳定编号：符号和产生式必须有稳定整数 ID，便于表索引。
3. 可追踪：每条产生式保留来源行号，便于报错和调试。
4. 可扩展：支持后续加入优先级、语义值类型、动作执行器。

## 3.2 核心实体定义

建议实现语言可为 C++/Java/Python，字段语义保持一致。

```text
enum SymbolKind {
  TERMINAL,
  NONTERMINAL,
  SPECIAL
}

struct Symbol {
  int id;                  // 全局唯一，稳定编号
  string name;             // 符号名，如 IDENTIFIER / expression / '('
  SymbolKind kind;         // 终结符 / 非终结符 / 特殊符号
  bool is_literal_char;    // 是否为单字符字面量终结符
}

struct ActionBlock {
  bool present;            // 该候选式是否有动作
  string raw_code;         // 原始动作代码，不执行，仅保存
}

struct Production {
  int id;                  // 产生式编号
  int lhs_symbol_id;       // 左部非终结符
  vector<int> rhs_symbol_ids; // 右部符号序列；空序列表示 epsilon
  ActionBlock action;      // 语义动作占位
  int source_line;         // 在 .y 文件中的起始行
}

struct Grammar {
  string source_path;                  // 输入文件路径
  int start_symbol_id;                 // %start 指定的符号
  int augmented_start_symbol_id;       // 增广开始符号 S'
  int eof_symbol_id;                   // 结束符 $
  int epsilon_symbol_id;               // 内部 epsilon（可选）
  vector<Symbol> symbols;              // 全部符号表
  vector<int> terminal_ids;            // 终结符 ID 列表
  vector<int> nonterminal_ids;         // 非终结符 ID 列表
  vector<Production> productions;      // 全部产生式（含增广产生式）
  unordered_map<string, int> symbol_id_by_name; // 名称到 ID
  unordered_map<int, vector<int>> prod_ids_by_lhs; // lhs -> 产生式ID列表
  string user_subroutines_raw;         // 第二个 %% 后原文
}
```

## 3.3 必要辅助结构（为第 3-8 步预留）

```text
struct LR1Item {
  int production_id;
  int dot_pos;
  int lookahead_symbol_id;
}

struct ParserTable {
  // Action: (state, terminal) -> Shift/Reduce/Accept/Error
  // Goto:   (state, nonterminal) -> next_state
}
```

说明：第 2 步只定结构，不实现算法。

## 3.4 编号与保留符号规则

1. 先注册所有 `%token` 与规则中出现的字面量终结符。
2. 再注册所有非终结符。
3. 最后注册特殊符号：`$`、`epsilon`、`S'`。
4. `Production.id` 按解析顺序递增；增广产生式固定放在第 `0` 条。

## 3.5 解析阶段接口定义（第 3 步将实现）

```text
Grammar parseYaccFile(string path);
```

接口输入输出约定：

1. 输入：`.y` 文件路径。
2. 输出：填充完成的 `Grammar`。
3. 错误：返回“行号 + 列号 + 附近文本 + 错误类型”。

## 3.6 第 2 步验收标准

满足以下条件即判定第 2 步完成：

1. 已给出可编码的数据结构，非概念性描述。
2. 覆盖了符号、产生式、动作、开始符号、索引映射。
3. 可直接支撑 First、Closure/Goto、Action/Goto 表的后续实现。

结论：第 2 步已完成。

---

## 四、第 3 步交付：Yacc 输入文件解析器实现（已完成）

## 4.1 代码落地位置

第 3 步实现使用 C++，代码放在 `/src` 下，按模块拆分：

1. `src/yacc/model/grammar.h`：文法核心结构定义。
2. `src/yacc/parser/yacc_parser.h`：解析器接口与错误类型。
3. `src/yacc/parser/yacc_parser.cpp`：解析器实现。
4. `src/tools/yacc_parse_main.cpp`：命令行验证入口。
5. `CMakeLists.txt` + `src/CMakeLists.txt`：构建配置。

## 4.2 已实现能力

解析器接口：

```text
Grammar parse_yacc_file(const std::string& path);
```

已实现功能：

1. 读取 `.y` 文件并按两个 `%%` 切分三段。
2. 解析 Definitions 段中的 `%token` 和 `%start`。
3. 解析 Rules 段中的：
- 产生式左部
- `:`
- `|`
- `;`
- 命名符号与字符字面量符号
- 动作块 `{...}`（保存原文，不执行）
4. 保留 User Subroutines 段原文到 `Grammar.user_subroutines_raw`。
5. 构建增广产生式 `S' -> start_symbol`。
6. 构建 `lhs -> 产生式ID列表` 索引。
7. 提供带行列号的解析错误信息 `ParseError(line, column, message)`。
8. 支持结构化导出与结果分析（见 4.5）。

## 4.3 第一版边界在代码中的体现

1. 支持 `%token`、`%start`。
2. 暂不支持 `%left`/`%right`/`%union`/`%prec` 等高级特性。
3. 遇到第一版未支持规则元素会在解析期报错，不静默忽略。

## 4.4 第 3 步验收标准

满足以下条件即判定第 3 步完成：

1. 能成功解析 `c99.y` 并生成 `Grammar`。
2. 可输出符号数、终结符数、非终结符数、产生式数。
3. 规则语法错误可定位到行列号并给出错误信息。

结论：第 3 步已完成。

## 4.5 导出与分析（结构化防散落）

为避免后续阶段输出文件散落，解析结果统一导出到目录树：

```text
artifacts/
  yacc/
    step3/
      <input_stem>/
        summary.txt
        raw/
          symbols.tsv
          productions.txt
          user_subroutines.c
        analysis/
          report.txt
```

命令行用法：

1. 默认解析与校验：`./build/src/yacc_parse_tool c99.y`
2. 导出到默认目录：`./build/src/yacc_parse_tool c99.y --export`
3. 导出到自定义目录：`./build/src/yacc_parse_tool c99.y --export-dir artifacts/yacc/step3/c99_custom`
4. 终端展示明细：`--dump-symbols`、`--dump-productions`

当前分析项：

1. `unused_terminals`：未在任何 RHS 中出现的终结符。
2. `unreachable_nonterminals`：从开始符号不可达的非终结符。
3. `productions_with_actions`：包含动作块的产生式数量。

---

## 五、第 4 步交付：文法预处理与增广文法构造（已完成）

## 5.1 代码落地位置

1. `src/yacc/preprocess/grammar_preprocessor.h`
2. `src/yacc/preprocess/grammar_preprocessor.cpp`
3. `src/tools/yacc_parse_main.cpp`（接入第 4 步执行流）
4. `src/yacc/report/report_exporter.h`
5. `src/yacc/report/report_exporter.cpp`（新增 step4 导出）

## 5.2 第 4 步已实现能力

核心接口：

```text
GrammarPreprocessReport preprocess_grammar(Grammar& grammar);
```

已实现处理：

1. 重建 `lhs -> 产生式ID列表` 索引，避免后续算法反复全表扫描。
2. 校验增广产生式固定为 `#0`，并满足 `S' -> start_symbol`。
3. 校验关键符号有效性：`start`、`S'`、`$`、`epsilon`。
4. 校验产生式编号连续与符号 ID 边界合法。
5. 检查“无产生式非终结符”（开始符号为硬错误，其余为告警）。
6. 检查 RHS 显式 `epsilon` 使用（作为规范告警）。

## 5.3 第 4 步结构化导出

第 4 步默认导出目录：

```text
artifacts/
  yacc/
    step4/
      <input_stem>/
        summary.txt
        raw/
          symbols.tsv
          productions.txt
          augmented_grammar.txt
          prod_index_by_lhs.tsv
          user_subroutines.c
        analysis/
          report.txt
```

命令：

1. `./build/src/yacc_parse_tool c99.y`
2. `./build/src/yacc_parse_tool c99.y --export`

验证结果（`c99.y`）：

1. `preprocess_passed=true`
2. `preprocess_errors_count=0`
3. `preprocess_warnings_count=0`

结论：第 4 步已完成。

---

## 六、第 5 步交付：First 集计算（已完成）

## 6.1 代码落地位置

1. `src/yacc/first/first_set.h`
2. `src/yacc/first/first_set.cpp`
3. `src/tools/yacc_parse_main.cpp`（接入第 5 步执行流）
4. `src/yacc/report/report_exporter.h`
5. `src/yacc/report/report_exporter.cpp`（新增 step5 导出）

## 6.2 第 5 步已实现能力

核心接口：

```text
FirstSetResult compute_first_sets(const Grammar& grammar);
std::set<int> compute_first_of_sequence(const Grammar& grammar,
                                        const FirstSetResult& result,
                                        const std::vector<int>& symbol_ids,
                                        size_t start_index = 0);
FirstSetValidationReport validate_first_sets(const Grammar& grammar,
                                             const FirstSetResult& result);
```

已实现处理：

1. 终结符、`$`、`epsilon` 的 First 初始化。
2. 非终结符 First 不动点迭代与 epsilon 传播。
3. 可空非终结符自动统计。
4. 符号串 First(β) 计算（用于后续 LR(1) lookahead）。
5. First 集关键不变量校验与失败中断机制。

## 6.3 第 5 步结构化导出

第 5 步默认导出目录：

```text
artifacts/
  yacc/
    step5/
      <input_stem>/
        summary.txt
        raw/
          symbols.tsv
          productions.txt
          augmented_grammar.txt
          prod_index_by_lhs.tsv
          first_sets.tsv
          first_sequence_examples.txt
          user_subroutines.c
        analysis/
          report.txt
```

命令：

1. `./build/src/yacc_parse_tool c99.y`
2. `./build/src/yacc_parse_tool c99.y --export`

验收信号（`analysis/report.txt`）：

1. `first_converged=true`
2. `first_validation_passed=true`

结论：第 5 步已完成。

---

## 七、第 6 步交付：LR(1) 项与闭包/Goto（已完成）

## 7.1 代码落地位置

1. `src/yacc/lr1/lr1_items.h`
2. `src/yacc/lr1/lr1_items.cpp`
3. `src/tools/yacc_parse_main.cpp`（接入第 6 步执行流）
4. `src/yacc/report/report_exporter.h`
5. `src/yacc/report/report_exporter.cpp`（新增 step6 导出）

## 7.2 第 6 步已实现能力

核心接口：

```text
LR1Step6Result build_step6_lr1_items(const Grammar& grammar,
                                     const FirstSetResult& first_result);
LR1Step6ValidationReport validate_step6_lr1_items(const Grammar& grammar,
                                                  const FirstSetResult& first_result,
                                                  const LR1Step6Result& result);
std::string format_lr1_item(const Grammar& grammar, const LR1Item& item);
```

已实现处理：

1. LR(1) 项结构：`production_id`、`dot_pos`、`lookahead_symbol_id`。
2. `closure(I)`：按 `FIRST(beta+a)` 扩展非终结符项并去重。
3. `goto(I, X)`：移点后再做闭包，结果可重复。
4. 第 6 步校验：项合法性、闭包完备性、去重一致性。
5. lookahead 来源说明记录，支持追溯“为什么出现该展望符”。

## 7.3 第 6 步结构化导出

第 6 步默认导出目录：

```text
artifacts/
  yacc/
    step6/
      <input_stem>/
        summary.txt
        raw/
          symbols.tsv
          productions.txt
          augmented_grammar.txt
          prod_index_by_lhs.tsv
          first_sets.tsv
          first_sequence_examples.txt
          lr1_i0_items.txt
          lr1_i0_goto.tsv
          lr1_i0_goto_items.txt
          lr1_lookahead_derivation.txt
          user_subroutines.c
        analysis/
          report.txt
```

命令：

1. `./build/src/yacc_parse_tool c99.y`
2. `./build/src/yacc_parse_tool c99.y --export`

验收信号（`analysis/report.txt`）：

1. `lr1_step6_validation_passed=true`
2. `lr1_i0_closure_items` 与 `lr1_i0_goto_edges` 非零（对 `c99.y`）

结论：第 6 步已完成。

---

## 八、第 7 步交付：LR(1) 规范族与状态转移图（已完成）

## 8.1 代码落地位置

1. `src/yacc/lr1/lr1_items.h`
2. `src/yacc/lr1/lr1_items.cpp`
3. `src/tools/yacc_parse_main.cpp`（接入第 7 步执行流）
4. `src/yacc/report/report_exporter.h`
5. `src/yacc/report/report_exporter.cpp`（新增 step7 导出）

## 8.2 第 7 步已实现能力

核心接口：

```text
LR1Step7Result build_step7_lr1_canonical_collection(const Grammar& grammar,
                                                     const FirstSetResult& first_result);
LR1Step7ValidationReport validate_step7_lr1_canonical_collection(
    const Grammar& grammar,
    const FirstSetResult& first_result,
    const LR1Step7Result& result);
```

已实现处理：

1. 从 `I0` 出发，按所有可转移符号反复执行 `goto`，构造完整 LR(1) 项目集规范族。
2. 状态判等采用“完整 LR(1) 项集键值”去重，保证状态唯一与可复现。
3. 输出状态转移边 `from --symbol--> to`，并可反查每个状态来源。
4. 第 7 步校验：状态合法性、状态去重一致性、转移与 `goto` 一致性。

## 8.3 第 7 步结构化导出

第 7 步默认导出目录：

```text
artifacts/
  yacc/
    step7/
      <input_stem>/
        summary.txt
        raw/
          lr1_states.tsv
          lr1_state_items.txt
          lr1_transitions.tsv
          lr1_predecessors.tsv
          ...（继承 step6 及之前产物）
        analysis/
          report.txt
```

命令：

1. `./build/src/yacc_parse_tool c99.y`
2. `./build/src/yacc_parse_tool c99.y --export`

验收信号（`analysis/report.txt`）：

1. `lr1_step7_validation_passed=true`
2. `lr1_states`、`lr1_transitions` 为稳定非零值（对 `c99.y`）

实测结果（`c99.y`）：

1. `lr1_states=1855`
2. `lr1_transitions=17745`

结论：第 7 步已完成。

---

## 九、第 8 步交付：Action/Goto 分析表与冲突处理（已完成）

## 9.1 代码落地位置

1. `src/yacc/table/parse_table.h`
2. `src/yacc/table/parse_table.cpp`
3. `src/tools/yacc_parse_main.cpp`（接入第 8 步执行流）
4. `src/yacc/report/report_exporter.h`
5. `src/yacc/report/report_exporter.cpp`（新增 step8 导出）

## 9.2 第 8 步已实现能力

核心接口：

```text
LR1Step8Result build_step8_lr1_parsing_table(const Grammar& grammar,
                                             const LR1Step7Result& step7_result);
LR1Step8ValidationReport validate_step8_lr1_parsing_table(
    const Grammar& grammar,
    const LR1Step7Result& step7_result,
    const LR1Step8Result& step8_result);
```

已实现处理：

1. 根据第 7 步状态图构造：
- `Action[state, terminal]`：`shift/reduce/accept`
- `Goto[state, nonterminal]`：状态跳转
2. 冲突检测：
- `shift/reduce`
- `reduce/reduce`
- 其他异常动作组合
3. 冲突消解（确定性策略）：
- `shift/reduce`：优先 `shift`
- `reduce/reduce`：优先“产生式编号更小”者
- `accept/*`：优先 `accept`
4. 冲突日志：
- 冲突明细（冲突发生点与相关项）
- 冲突消解日志（最终选取动作与原因）
5. 第 8 步校验：表项合法性、与状态转移图一致性、归约依据一致性、冲突日志一致性。

## 9.3 第 8 步结构化导出

第 8 步默认导出目录：

```text
artifacts/
  yacc/
    step8/
      <input_stem>/
        summary.txt
        raw/
          action_table.tsv
          goto_table.tsv
          parse_table_conflicts.tsv
          parse_table_conflict_resolution.tsv
          ...（继承 step7 及之前产物）
        analysis/
          report.txt
```

命令：

1. `./build/src/yacc_parse_tool c99.y`
2. `./build/src/yacc_parse_tool c99.y --export`

验收信号（`analysis/report.txt`）：

1. `lr1_step8_validation_passed=true`
2. `parse_table_conflicts` 与 `parse_table_conflict_resolutions` 数量一致

实测结果（`c99.y`）：

1. `action_entries=30080`
2. `goto_entries=7489`
3. `parse_table_conflicts=2`
4. `parse_table_conflict_resolutions=2`
5. 两条冲突均为 `ELSE` 的经典 `shift/reduce` 场景，已按策略选择 `shift`。

结论：第 8 步已完成。

---

## 十、第 9 步交付：LR 总控程序（已完成）

## 10.1 代码落地位置

1. `src/yacc/runtime/lr_parser.h`
2. `src/yacc/runtime/lr_parser.cpp`
3. `src/tools/yacc_parse_main.cpp`（接入第 9 步参数与执行流）
4. `scripts/yacc_step9_validate.py`（第 9 步自动验收）
5. `contracts/yacc/tokens/*.tokens`（样例 token 流）

## 10.2 第 9 步已实现能力

核心接口：

```text
std::vector<RuntimeToken> load_runtime_tokens_from_file(const Grammar& grammar,
                                                        const std::string& path);
LRParseRunResult run_step9_lr_parse(const Grammar& grammar,
                                    const LR1Step8Result& table,
                                    const std::vector<RuntimeToken>& tokens,
                                    int max_steps);
```

已实现处理：

1. 读取 token 文件，支持 `TOKEN` / `TOKEN LEXEME` / `TOKEN LEXEME LINE COLUMN` 格式。
2. 自动补 `$` 结束符，统一查表驱动执行。
3. 维护状态栈与符号栈，执行 `shift / reduce / accept / error`。
4. 导出完整 parse trace、规约序列、错误定位与期望终结符集合。
5. `--max-parse-steps` 防死循环保护。

## 10.3 第 9 步结构化导出

第 9 步默认导出目录：

```text
artifacts/
  yacc/
    step9/
      <input_stem>/
        summary.txt
        raw/
          parse_input_tokens.tsv
          parse_trace.tsv
          parse_reductions.txt
          parse_error.txt
          ...（继承 step8 及之前产物）
        analysis/
          report.txt
```

命令：

1. `./build/src/yacc_parse_tool c99.y --parse-tokens contracts/yacc/tokens/c99_decl_int.tokens --export`
2. `python3 scripts/yacc_step9_validate.py --tokens contracts/yacc/tokens/c99_func_return_const.tokens --expect accept`
3. `python3 scripts/yacc_step9_validate.py --tokens contracts/yacc/tokens/c99_invalid_if.tokens --expect reject`

验收信号（`summary.txt` / `analysis/report.txt`）：

1. 合法输入：`parse_accepted=true`
2. 非法输入：`parse_error=true` 且含状态号、lookahead、expected terminals

结论：第 9 步已完成。

---

## 十一、第 10 步交付：LR(1) 到 LALR(1) 转换（已完成）

## 11.1 代码落地位置

1. `src/yacc/lalr/lalr_builder.h`
2. `src/yacc/lalr/lalr_builder.cpp`
3. `src/tools/yacc_parse_main.cpp`（接入第 10 步执行与终端输出）
4. `src/yacc/report/report_exporter.h`
5. `src/yacc/report/report_exporter.cpp`（新增 step10 导出）

## 11.2 第 10 步已实现能力

核心接口：

```text
LR1Step10Result build_step10_lalr_from_lr1(const Grammar& grammar,
                                           const LR1Step7Result& lr1_step7_result,
                                           const LR1Step8Result& lr1_step8_result);
LR1Step10ValidationReport validate_step10_lalr_from_lr1(
    const Grammar& grammar,
    const LR1Step7Result& lr1_step7_result,
    const LR1Step8Result& lr1_step8_result,
    const LR1Step10Result& step10_result);
```

已实现处理：

1. 以 LR(0) 核（`production_id + dot_pos`）相同为准合并 LR(1) 状态。
2. 合并组内 lookahead（并集）形成 LALR 状态项集。
3. 生成 `LR(1)状态 -> LALR状态` 映射与合并组明细。
4. 将 LR(1) 转移投影为 LALR 转移并去重。
5. 在 LALR 状态机上重新构造 Action/Goto 表并输出冲突对比。
6. 校验合并映射完整性、转移确定性和 LALR 表合法性。

## 11.3 第 10 步结构化导出

第 10 步默认导出目录：

```text
artifacts/
  yacc/
    step10/
      <input_stem>/
        summary.txt
        raw/
          lr1_to_lalr_state_map.tsv
          lalr_merge_groups.tsv
          lalr_state_items.txt
          lalr_transitions.tsv
          lalr_action_table.tsv
          lalr_goto_table.tsv
          lalr_parse_table_conflicts.tsv
          lalr_parse_table_conflict_resolution.tsv
          ...（继承 step8 及之前产物）
        analysis/
          report.txt
```

命令：

1. `./build/src/yacc_parse_tool c99.y --export`

验收信号（`summary.txt` / `analysis/report.txt`）：

1. `lr1_step10_validation_passed=true`
2. 可对比 `lr1_states` 与 `lalr_states`
3. 可对比 `lr1_conflicts` 与 `lalr_conflicts`

当前实测（`c99.y`）：

1. `lr1_states=1855`
2. `lalr_states=399`
3. `state_merged=1456`
4. `lr1_conflicts=2`
5. `lalr_conflicts=1`

结论：第 10 步已完成。

---

## 十二、阶段结论与后续入口

当前文档已把第 1 至第 10 步从“讨论状态”提升为“定稿 + 实现状态”。

下一阶段直接进入：

1. 第 11 步：与 Lex 对接（token 编号/命名/接口规范统一）。
2. 第 12 步：语义接口与 AST/中间表示衔接。
3. 第 13 步及之后：冲突与错误恢复增强、测试集、可视化与答辩材料收敛。

本文件将持续追加后续步骤，保持“YACC 全流程仅一份总文档”。
