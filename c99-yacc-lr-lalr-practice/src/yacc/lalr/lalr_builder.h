#pragma once

#include <string>
#include <vector>

#include "yacc/lr1/lr1_items.h"
#include "yacc/model/grammar.h"
#include "yacc/table/parse_table.h"

namespace seu::yacc {

// 单个 LALR 状态的来源信息：由哪些 LR(1) 状态合并而来。
struct LALRMergeGroup {
    int lalr_state_id = -1;
    std::vector<int> source_lr1_state_ids;
    std::vector<LR1Item> merged_items;
};

// 第 10 步输出：LR(1)->LALR(1) 合并结果 + LALR 分析表 + 对比指标。
struct LR1Step10Result {
    std::vector<int> lr1_to_lalr_state_id;  // 下标=LR(1)状态号，值=LALR状态号
    std::vector<LALRMergeGroup> merge_groups;

    LR1Step7Result lalr_step7_result;  // 合并后的状态与转移（结构与 step7 对齐）
    LR1Step8Result lalr_step8_result;  // 基于 LALR 状态机构造的 Action/Goto 表

    std::size_t lr1_state_count = 0;
    std::size_t lalr_state_count = 0;
    std::size_t lr1_conflict_count = 0;
    std::size_t lalr_conflict_count = 0;
};

// 第 10 步校验报告：关注“合并映射正确 + 转移一致 + 构表可用”。
struct LR1Step10ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// 构造第 10 步产物：按 LR(0) 核相同原则合并 LR(1) 状态，得到 LALR(1)。
LR1Step10Result build_step10_lalr_from_lr1(
    const Grammar& grammar, const LR1Step7Result& lr1_step7_result, const LR1Step8Result& lr1_step8_result);

// 校验第 10 步结果：
// 1) 每个 LR(1) 状态都被映射；
// 2) LALR 转移确定；
// 3) LALR Action/Goto 表合法；
// 4) 冲突统计与对比信息可追踪。
LR1Step10ValidationReport validate_step10_lalr_from_lr1(const Grammar& grammar,
    const LR1Step7Result& lr1_step7_result, const LR1Step8Result& lr1_step8_result,
    const LR1Step10Result& step10_result);

}  // namespace seu::yacc

