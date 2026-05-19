#pragma once

#include <string>
#include <vector>

#include "yacc/first/first_set.h"
#include "yacc/model/grammar.h"

namespace seu::yacc {

// LR(1) 项：A -> α · β, a
// production_id: 产生式编号
// dot_pos: 点位置，取值范围 [0, rhs_size]
// lookahead_symbol_id: 展望符（通常是终结符或 $）
struct LR1Item {
    int production_id = -1;
    int dot_pos = 0;
    int lookahead_symbol_id = -1;
};

// 第 6 步输出：仅聚焦“单个状态（I0）的构造与可视化”。
struct LR1Step6Result {
    std::vector<LR1Item> i0_kernel_items;   // I0 的核心项（当前只有 [S'->·S,$]）
    std::vector<LR1Item> i0_closure_items;  // I0 闭包完整项集（已去重排序）

    std::vector<int> goto_symbol_ids;                   // 从 I0 可转移的符号集合
    std::vector<std::vector<LR1Item>> goto_item_sets;   // 与 goto_symbol_ids 一一对应

    std::vector<std::string> lookahead_derivation_notes;  // 展望符来源说明（用于验收与调试）
};

// 第 6 步校验报告：确保 closure/goto 结果可重复、无遗漏、无重复爆炸。
struct LR1Step6ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// LR(1) 状态（规范族中的一个项目集）。
struct LR1State {
    int state_id = -1;
    std::vector<LR1Item> items;
};

// LR(1) 状态转移边：state --symbol--> state
struct LR1Transition {
    int from_state_id = -1;
    int symbol_id = -1;
    int to_state_id = -1;
};

// 第 7 步输出：完整 LR(1) 项目集规范族与状态转移图。
struct LR1Step7Result {
    std::vector<LR1State> states;
    std::vector<LR1Transition> transitions;
};

// 第 7 步校验报告：确保规范族完整、状态去重一致、goto 转移正确。
struct LR1Step7ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// 构造第 6 步核心产物：I0 kernel、closure(I0) 与基于 I0 的 goto(I0, X)。
LR1Step6Result build_step6_lr1_items(
    const Grammar& grammar, const FirstSetResult& first_result);

// 校验第 6 步结果，重点检查：
// 1) 项合法性；
// 2) 闭包完备性；
// 3) 去重一致性。
LR1Step6ValidationReport validate_step6_lr1_items(
    const Grammar& grammar, const FirstSetResult& first_result, const LR1Step6Result& result);

// 构造第 7 步核心产物：完整 LR(1) 项目集规范族与状态转移图。
LR1Step7Result build_step7_lr1_canonical_collection(
    const Grammar& grammar, const FirstSetResult& first_result);

// 校验第 7 步结果：
// 1) 状态项合法性；
// 2) 状态去重一致性；
// 3) 转移合法性与 goto 一致性。
LR1Step7ValidationReport validate_step7_lr1_canonical_collection(
    const Grammar& grammar, const FirstSetResult& first_result, const LR1Step7Result& result);

// 将 LR(1) 项渲染成可读文本。
std::string format_lr1_item(const Grammar& grammar, const LR1Item& item);

}  // namespace seu::yacc
