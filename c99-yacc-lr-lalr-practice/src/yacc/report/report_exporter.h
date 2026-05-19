#pragma once

#include <string>
#include <vector>

#include "yacc/first/first_set.h"
#include "yacc/lr1/lr1_items.h"
#include "yacc/lalr/lalr_builder.h"
#include "yacc/model/grammar.h"
#include "yacc/preprocess/grammar_preprocessor.h"
#include "yacc/runtime/lr_parser.h"
#include "yacc/table/parse_table.h"

namespace seu::yacc {

// 解析结果分析：用于识别潜在质量问题，辅助后续阶段调试。
struct GrammarAnalysis {
    std::vector<int> unused_terminal_ids;         // 声明/注册但在 RHS 未使用的终结符
    std::vector<int> unreachable_nonterminal_ids; // 从开始符号不可达的非终结符
    int productions_with_actions = 0;             // 含动作块的产生式数量
};

GrammarAnalysis analyze_grammar(const Grammar& grammar);

// 导出目录约定：artifacts/yacc/step3/<input_stem>/
std::string make_default_export_dir(const std::string& input_path);

// 将解析结果和分析结果导出到结构化目录。
void export_report(
    const Grammar& grammar, const GrammarAnalysis& analysis, const std::string& output_dir);

// 第 4 步导出目录约定：artifacts/yacc/step4/<input_stem>/
std::string make_default_step4_export_dir(const std::string& input_path);

// 将第 4 步“文法预处理”结果导出。
void export_step4_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const std::string& output_dir);

// 第 5 步导出目录约定：artifacts/yacc/step5/<input_stem>/
std::string make_default_step5_export_dir(const std::string& input_path);

// 将第 5 步 First 集结果导出。
void export_step5_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const std::string& output_dir);

// 第 6 步导出目录约定：artifacts/yacc/step6/<input_stem>/
std::string make_default_step6_export_dir(const std::string& input_path);

// 将第 6 步 LR(1) 项、closure(I0)、goto(I0, X) 结果导出。
void export_step6_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_result,
    const LR1Step6ValidationReport& lr1_validation, const std::string& output_dir);

// 第 7 步导出目录约定：artifacts/yacc/step7/<input_stem>/
std::string make_default_step7_export_dir(const std::string& input_path);

// 将第 7 步 LR(1) 项目集规范族与状态转移图导出。
void export_step7_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const std::string& output_dir);

// 第 8 步导出目录约定：artifacts/yacc/step8/<input_stem>/
std::string make_default_step8_export_dir(const std::string& input_path);

// 将第 8 步 Action/Goto 分析表与冲突报告导出。
void export_step8_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const std::string& output_dir);

// 第 9 步导出目录约定：artifacts/yacc/step9/<input_stem>/
std::string make_default_step9_export_dir(const std::string& input_path);

// 将第 9 步 LR 解析执行结果导出。
void export_step9_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const LRParseRunResult& lr1_parse_result,
    const LRParseRunResult& lalr_parse_result, const std::vector<RuntimeToken>& input_tokens,
    const std::string& token_source,
    const std::string& output_dir);

// 第 10 步导出目录约定：artifacts/yacc/step10/<input_stem>/
std::string make_default_step10_export_dir(const std::string& input_path);

// 将第 10 步 LR(1)->LALR(1) 合并结果导出。
void export_step10_report(const Grammar& grammar, const GrammarAnalysis& analysis,
    const GrammarPreprocessReport& preprocess_report, const FirstSetResult& first_result,
    const FirstSetValidationReport& first_validation, const LR1Step6Result& lr1_step6_result,
    const LR1Step6ValidationReport& lr1_step6_validation, const LR1Step7Result& lr1_step7_result,
    const LR1Step7ValidationReport& lr1_step7_validation, const LR1Step8Result& lr1_step8_result,
    const LR1Step8ValidationReport& lr1_step8_validation, const LR1Step10Result& step10_result,
    const LR1Step10ValidationReport& step10_validation, const std::string& output_dir);

}  // namespace seu::yacc
