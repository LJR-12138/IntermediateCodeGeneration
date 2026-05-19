#pragma once

#include <string>
#include <vector>

#include "yacc/model/grammar.h"
#include "yacc/table/parse_table.h"

namespace seu::yacc {

// 第 9 步输入 token：由词法模块或手工 token 文件提供。
struct RuntimeToken {
    int symbol_id = -1;
    std::string symbol_name;
    std::string lexeme;
    int line = 0;
    int column = 0;
};

// 第 9 步单步执行快照：用于导出和可视化移进/归约过程。
struct LRParseTraceRow {
    int step_no = 0;
    int state_id = -1;
    int lookahead_symbol_id = -1;
    std::string lookahead_symbol_name;
    std::string action_text;
    int production_id = -1; // reduce 时有效
    std::string state_stack_text;
    std::string symbol_stack_text;
    int input_index = 0;
};

// 第 9 步错误信息：保留状态、当前 token 和可期待终结符集合。
struct LRParseErrorInfo {
    bool has_error = false;
    int step_no = 0;
    int input_index = 0;
    int state_id = -1;
    int lookahead_symbol_id = -1;
    std::string lookahead_symbol_name;
    std::string message;
    std::vector<int> expected_terminal_ids;
};

// 第 9 步运行结果：包括接受状态、规约序列、完整 trace 和错误信息。
struct LRParseRunResult {
    bool accepted = false;
    int consumed_tokens = 0;  // 不计自动补的 EOF
    int total_steps = 0;
    std::vector<int> reduction_production_ids;
    std::vector<LRParseTraceRow> trace_rows;
    LRParseErrorInfo error;
};

// 读取 token 文件（每行一个 token）：
// 1) TOKEN
// 2) TOKEN LEXEME
// 3) TOKEN LEXEME LINE COLUMN
// 支持注释行（以 # 开头）和空行。
// 若文件末尾不含 EOF($)，会自动补一个 $。
std::vector<RuntimeToken> load_runtime_tokens_from_file(
    const Grammar& grammar, const std::string& token_file_path);

// 运行第 9 步 LR 总控程序（查表移进-归约）。
LRParseRunResult run_step9_lr_parse(const Grammar& grammar, const LR1Step8Result& step8_result,
    const std::vector<RuntimeToken>& input_tokens, int max_steps);

}  // namespace seu::yacc
