#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "yacc/lr1/lr1_items.h"
#include "yacc/model/grammar.h"

namespace seu::yacc {

// 分析表动作类型：移进、归约、接受。
enum class ParseActionType {
    Shift,
    Reduce,
    Accept
};

// Action 表单元。
struct ParseActionEntry {
    ParseActionType type = ParseActionType::Shift;
    int target_state_id = -1;   // shift 时有效
    int reduce_production_id = -1;  // reduce 时有效
};

// 第 8 步冲突信息：用于定位 shift/reduce、reduce/reduce 等问题。
struct ParseTableConflict {
    int state_id = -1;
    int symbol_id = -1;
    std::string conflict_type;   // 例如 shift/reduce、reduce/reduce
    std::string existing_action; // 旧动作
    std::string incoming_action; // 新动作
    std::vector<LR1Item> related_items;
};

// 冲突消解日志：记录最终采用的动作及原因，便于后续复现与答辩说明。
struct ParseTableConflictResolutionLog {
    int state_id = -1;
    int symbol_id = -1;
    std::string conflict_type;
    std::string existing_action;
    std::string incoming_action;
    std::string resolved_action;
    std::string reason;
};

// 第 8 步输出：Action/Goto 表 + 冲突列表。
struct LR1Step8Result {
    // action_table[state_id][terminal_id] = action
    std::vector<std::unordered_map<int, ParseActionEntry>> action_table;
    // goto_table[state_id][nonterminal_id] = to_state_id
    std::vector<std::unordered_map<int, int>> goto_table;
    std::vector<ParseTableConflict> conflicts;
    std::vector<ParseTableConflictResolutionLog> conflict_resolution_logs;
};

// 第 8 步校验报告：构表完整性、一致性、冲突统计。
struct LR1Step8ValidationReport {
    bool passed = false;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

// 基于第 7 步 LR(1) 规范族构造 Action/Goto 表。
LR1Step8Result build_step8_lr1_parsing_table(
    const Grammar& grammar, const LR1Step7Result& step7_result);

// 校验第 8 步结果：
// 1) 表项合法性；
// 2) 与状态转移/归约项一致性；
// 3) 冲突可追踪。
LR1Step8ValidationReport validate_step8_lr1_parsing_table(const Grammar& grammar,
    const LR1Step7Result& step7_result, const LR1Step8Result& step8_result);

// 将 Action 表单元渲染为可读文本（如 s23 / r45 / acc）。
std::string format_parse_action_entry(const ParseActionEntry& entry);

}  // namespace seu::yacc
