#pragma once

#include <cstddef>
#include <set>
#include <string>
#include <vector>

#include "yacc/model/grammar.h"

namespace seu::yacc {

// 第 5 步结果：保存每个符号的 First 集、可空非终结符与迭代信息。
struct FirstSetResult {
    bool converged = false;
    int iteration_count = 0;
    std::vector<std::set<int>> first_sets_by_symbol_id;
    std::vector<int> nullable_nonterminal_ids;
};

// 第 5 步校验报告：用于确认 First 集可稳定支撑后续 LR(1) lookahead 计算。
struct FirstSetValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// 计算整套文法的 First 集（含终结符/非终结符/特殊符号）。
FirstSetResult compute_first_sets(const Grammar& grammar);

// 计算符号串 First(β)，用于 LR(1) 闭包中的 lookahead 传播。
std::set<int> compute_first_of_sequence(const Grammar& grammar, const FirstSetResult& result,
    const std::vector<int>& symbol_ids, std::size_t start_index = 0);

// 校验 First 集关键不变量，作为第 5 步最低验收标准之一。
FirstSetValidationReport validate_first_sets(const Grammar& grammar, const FirstSetResult& result);

}  // namespace seu::yacc

